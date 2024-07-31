#include <E_S_utils.h>

#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b? a:b)

str_sockets sockets;
char* nombre_interfaz;
sem_t mutex_conexion;

void establecer_conexion_kernel(int* socket_kernel){
    //motivo_desalojo instruccion_a_procesar;
    *socket_kernel = crear_conexion(configuracion.IP_KERNEL ,string_itoa(configuracion.PUERTO_KERNEL),logger_conexiones,"Entrada Salida");
    log_info(logger_conexiones, "Conectado Entrada/Salida-Kernel");
    enviar_info_io_a_kernel(*socket_kernel);
    
    log_info(logger_conexiones, "Info enviada");
    
}
void establecer_conexion_memoria(int* socket_memoria){
    *socket_memoria = crear_conexion(configuracion.IP_MEMORIA ,string_itoa(configuracion.PUERTO_MEMORIA),logger_conexiones,"Entrada Salida");
    log_info(logger_conexiones, "Conectado Entrada/Salida-Memoria");
}

void establecer_conexiones(){
    pthread_t memoria, kernel;
    int socket_kernel = -1;
    int socket_memoria = -1;
    pthread_create(&memoria,NULL,(void*)establecer_conexion_memoria,(void*) &socket_memoria);
    pthread_create(&kernel,NULL,(void*)establecer_conexion_kernel,(void*) &socket_kernel);
    pthread_join(memoria,NULL);
    pthread_join(kernel,NULL);
    
    recibir_instrucciones(socket_kernel, socket_memoria);
}


void procesar_io_gen_sleep (t_buffer* buffer_kernel, int socket_kernel, char* nombre) {
    uint32_t milisegundos_de_espera;
    buffer_read (buffer_kernel,&milisegundos_de_espera, sizeof(uint32_t));
    t_temporal *temporal = temporal_create();
	usleep(milisegundos_de_espera*1000);
	temporal_stop(temporal);
    log_info(logger_entrada_salida, "Tiempo esperado: %lu", temporal_gettime(temporal));
    temporal_destroy(temporal);
    enviar_fin_de_instruccion(socket_kernel, nombre);
}


void enviar_info_io_a_kernel(int socket_kernel){
    t_paquete* paquete = crear_paquete(ENTRADASALIDA, sizeof(t_interfaz)+ sizeof(uint32_t) + string_length(nombre_interfaz)+1);
    buffer_add(paquete->buffer, &configuracion.TIPO_INTERFAZ, sizeof(t_interfaz));
    
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    enviar_paquete(paquete, socket_kernel);

}


void procesar_io_stdin_read(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, int socket_memoria, char* nombre) {
  
    int cantidad_paginas;
    buffer_read(buffer_kernel, &cantidad_paginas, sizeof(int));
    int direcciones_fisicas_memoria_read[cantidad_paginas];  
    int tamanio_direcciones_read[cantidad_paginas]; 
    uint32_t tamanio_array_direcciones = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, direcciones_fisicas_memoria_read, tamanio_array_direcciones);
    uint32_t tamanio_arrays_tam_a_escribir = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, tamanio_direcciones_read, tamanio_arrays_tam_a_escribir);
    /*
    buffer_add(paquete->buffer, cant_paginas,sizeof(int));
    buffer_add_uint32(paquete->buffer,sizeof(direc_fisica));
    buffer_add(paquete->buffer, direccion_fisica,sizeof(direccion_fisica));
    buffer_add_uint32(paquete->buffer,sizeof(tam_a_escribir));
    buffer_add(paquete->buffer, tam_a_escribir,sizeof(tam_a_escribir));
    */
    
    char* input_consola = leer_consola();
    int offset = 0;
    for(int i=0;i<cantidad_paginas;i++)
    {
        char* a_escribir = string_substring(input_consola,offset,offset+tamanio_direcciones_read[i]);
        offset+=tamanio_direcciones_read[i];
        escribir_en_memoria(a_escribir, pid, direcciones_fisicas_memoria_read[i],tamanio_direcciones_read[i],socket_memoria );
        free(a_escribir);
        if(recibir_operacion(socket_memoria) != OK_ESCRITURA){
            log_error(logger_errores, "Hubo un error al recibir el codigo de escritura de memoria");
        }
    }
    enviar_fin_de_instruccion(socket_kernel,nombre);
    
}

char* leer_consola(){
	char* leido;
	leido = readline("> ");
    log_info(logger_entrada_salida,"%s",leido);
    return leido;	
}


void escribir_en_memoria(void* input_consola,uint32_t pid,uint32_t direccion_fisica_memoria_write,uint32_t tamanio, int socket_memoria) {
    uint32_t accion =1 ;//escribir
    t_paquete* paquete = crear_paquete(ACCESS_ESPACIO_USUARIO_ES, tamanio + sizeof(uint32_t) * 4);



    buffer_add_uint32(paquete->buffer, pid);
    buffer_add_uint32(paquete->buffer, accion); 
    buffer_add_uint32(paquete->buffer, direccion_fisica_memoria_write);
    buffer_add_uint32(paquete->buffer, tamanio);
    buffer_add(paquete->buffer,input_consola,tamanio);

    enviar_paquete(paquete, socket_memoria);//peticion escritura memoria.. si pone ok! escribio correctamente

    }

void procesar_io_stdout_write(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, int socket_memoria, char* nombre) {
    //leer el valor que se encuentra en la o las direcciones físicas pedidas y mostrar el resultado por pantalla
    /*
    IO_STDOUT_WRITE (Interfaz, Registro Dirección, Registro Tamaño):
     Esta instrucción solicita al Kernel que mediante la interfaz seleccionada, se lea desde la posición de memoria indicada por 
     la Dirección Lógica almacenada en el Registro Dirección, un tamaño indicado por el Registro Tamaño y se imprima por pantalla.
    */

    int cantidad_paginas_a_leer;
    uint32_t accion =0;
    buffer_read(buffer_kernel, &cantidad_paginas_a_leer, sizeof(int));
    int direcciones_fisicas_memoria_a_leer[cantidad_paginas_a_leer];  
    int tamanio_direcciones_a_leer[cantidad_paginas_a_leer]; 
    uint32_t tamanio_array_direcciones = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, direcciones_fisicas_memoria_a_leer, tamanio_array_direcciones);
    uint32_t tamanio_arrays_tam_a_leer = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, tamanio_direcciones_a_leer, tamanio_arrays_tam_a_leer);
    char* mostrar_de_memoria =string_new();
    
    for(int i=0;i<cantidad_paginas_a_leer;i++){
        t_paquete* paquete = crear_paquete(ACCESS_ESPACIO_USUARIO_ES, sizeof(uint32_t) * 4);
        buffer_add_uint32(paquete->buffer, pid);
        buffer_add_uint32(paquete->buffer, accion);
        buffer_add_uint32(paquete->buffer, (uint32_t)direcciones_fisicas_memoria_a_leer[i]);
        buffer_add_uint32(paquete->buffer, (uint32_t)tamanio_direcciones_a_leer[i]);
        enviar_paquete(paquete, socket_memoria);
        op_code codigo_leido = recibir_operacion(socket_memoria);
        if(codigo_leido != RESPUESTA_LECTURA_MEMORIA){
            log_error(logger_errores,"Hubo un error al recibir la confirmacion de lectura de memoria");
        }
        t_buffer* memoria = recibir_todo_elbuffer (socket_memoria);
        uint32_t* length = malloc(sizeof(uint32_t));
        char* aux = buffer_read_string(memoria, length); 
        string_n_append(&mostrar_de_memoria,aux,*length);
        free(aux);
        free(length);
        buffer_destroy(memoria);
    }

    printf("%s",mostrar_de_memoria);
    log_info(logger_salida,"%s",mostrar_de_memoria);
    free(mostrar_de_memoria);
    enviar_fin_de_instruccion(socket_kernel, nombre);
}

//luego pasar socket memoria
void recibir_instrucciones (int socket_kernel, int socket_memoria){
    uint32_t pid ;
    t_instruccion instruccion_a_procesar;
    op_code cop;
    char* nombre = malloc(sizeof(char)*(strlen(nombre_interfaz) + 1));
    nombre = strcpy(nombre,nombre_interfaz);
    sem_post(&mutex_conexion);
    while(socket_kernel!=-1) {
        if (recv(socket_kernel, &cop, sizeof(op_code), 0) != sizeof(op_code)) {
                log_info(logger_conexiones, "Error en el OpCode!");
                return;
            }
        if (cop != ENTRADASALIDA) {
            if(cop == APAGAR){
                sem_post(&sem_apagar);
                free(nombre);
                return;
            }
            log_info(logger_conexiones, "OpCode recibido no corresponde con ENTRADASALIDA");
            return;
        }
        t_buffer* buffer_kernel = recibir_todo_elbuffer(socket_kernel);
        buffer_read(buffer_kernel,&instruccion_a_procesar,sizeof(t_instruccion));
        char* instruccion_string = string_de_instruccion(instruccion_a_procesar);
        pid = buffer_read_uint32(buffer_kernel);
        log_info(logger_entrada_salida, "PID: %u - Operacion: %s",pid,instruccion_string);
        switch (instruccion_a_procesar) {
            case GEN_SLEEP:
            procesar_io_gen_sleep(buffer_kernel,socket_kernel,nombre);
            break;
            case STDIN_READ:
            procesar_io_stdin_read(buffer_kernel,pid,socket_kernel, socket_memoria,nombre);
            break;
            case STDOUT_WRITE:
            procesar_io_stdout_write(buffer_kernel,pid,socket_kernel, socket_memoria,nombre);
            break;
            case FS_CREATE:
            procesar_io_fs_create(buffer_kernel,pid,socket_kernel,nombre);
            break;
            case FS_DELETE:
            procesar_io_fs_delete(buffer_kernel,pid,socket_kernel,nombre);
            break;
            case FS_TRUNCATE:
            procesar_io_fs_truncate(buffer_kernel,pid,socket_kernel,nombre);
            break;
            case FS_WRITE:
            procesar_io_fs_write(buffer_kernel,pid,socket_kernel,socket_memoria,nombre);
            break;
            case FS_READ:
            procesar_io_fs_read(buffer_kernel,pid,socket_kernel,socket_memoria,nombre);
            break;
            default:
            log_info(logger_entrada_salida, "Instruccion invalida");
            break;
        }

        free(instruccion_string);
        buffer_destroy(buffer_kernel);
        
    }
    return;
}


char* string_de_instruccion (t_instruccion instruccion_a_procesar){
    char* instruccion_en_string;
    switch(instruccion_a_procesar) {
        case GEN_SLEEP:
        instruccion_en_string = string_from_format("GEN_SLEEP");
        break;
        case STDIN_READ:
        instruccion_en_string = string_from_format("STDIN_READ");
        break;
        case STDOUT_WRITE:
        instruccion_en_string = string_from_format("STDOUT_WRITE");
        break;
        case FS_CREATE:
        instruccion_en_string = string_from_format("FS_CREATE");
        break;
        case FS_DELETE:
        instruccion_en_string = string_from_format("FS_DELETE");
        break;
        case FS_TRUNCATE:
        instruccion_en_string = string_from_format("FS_TRUNCATE");
        break;
        case FS_WRITE:
        instruccion_en_string = string_from_format("FS_WRITE");       
        break;
        case FS_READ:
        instruccion_en_string = string_from_format("FS_READ");
        break;
    }
    return instruccion_en_string;
}


void enviar_fin_de_instruccion (int socket_fd, char* nombre) {
    t_paquete* paquete = crear_paquete(ENTRADASALIDA_LIBERADO, + sizeof(uint32_t) +string_length(nombre)+1);
    buffer_add_string(paquete->buffer, string_length(nombre)+1, nombre);
    enviar_paquete(paquete, socket_fd); // kernel
 }

void procesar_io_fs_create(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, char* nombre){
    usleep(configuracion.TIEMPO_UNIDAD_TRABAJO * 1000);
    uint32_t longitud_nombre_archivo;
    char* nombre_archivo = buffer_read_string(buffer_kernel, &longitud_nombre_archivo);
    char* path_archivo = string_new();
    string_append(&path_archivo, configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo, "/");
    string_append(&path_archivo, nombre_archivo);
    log_info(logger_fs, "PID: %u - Crear Archivo: %s",pid,nombre_archivo);
    
    /*
    IO_FS_CREATE (Interfaz, Nombre Archivo): Esta instrucción solicita al 
    Kernel que mediante la interfaz seleccionada, se cree un archivo en el FS montado en dicha interfaz.
    */
    FILE *file = fopen(path_archivo, "r+");
    if (file != NULL) {
        log_error(logger_errores, "El archivo ya existe");
        fclose(file);
        return;
    }
    if(contar_bloques_libres(bitmap) == 0){
        log_error(logger_errores, "No hay mas bloques disponibles");
        fclose(file);
        return;
    }
    if (errno == ENOENT) {
        file = fopen(path_archivo, "w+");
        if (file == NULL) {
            log_error(logger_errores, "Error al crear el archivo");
            return;
        }
        int fd = fileno(file);
        if (ftruncate(fd, 0) != 0) {
            log_error(logger_errores, "Error al truncar el archivo");
            fclose(file);
            return;
        }
        fclose(file);
        } else {
            perror("Error al abrir el archivo");
            return;
        }
        int bloque_asignado = primer_bloque_libre(bitmap);
        t_config* config_archivo = config_create(path_archivo);
        config_set_value(config_archivo,"BLOQUE_INICIAL",string_itoa(bloque_asignado));
        config_set_value(config_archivo,"TAMANIO_ARCHIVO","0");
        config_save(config_archivo);
        bitarray_set_bit(bitmap,bloque_asignado);
        log_info(logger_fs, "Bit %i seteado",bloque_asignado);
        log_info(logger_fs, "Bits libres: %i",contar_bloques_libres(bitmap));
        msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
        agregar_archivo_a_indice(nombre_archivo ,bloque_asignado);
        enviar_fin_de_instruccion(socket_kernel,nombre);
}


int primer_bloque_libre(t_bitarray* bitmap) {
    int primer_bit_libre;
    for (int i=0;i<bitarray_get_max_bit(bitmap);i++){
        if(!bitarray_test_bit(bitmap,i)){
            primer_bit_libre=i;
            return primer_bit_libre;
        }
    }
    return -1;
}

int contar_bloques_libres(t_bitarray* bitmap) {
    int bits_libres=0;
    for (int i=0;i<bitarray_get_max_bit(bitmap);i++){
        if(!bitarray_test_bit(bitmap,i)){
            bits_libres+=1;
        }
    }
    return bits_libres;
}

int obtener_primer_bloque_de_archivo (char * nombre_archivo){
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);
    t_config* config_achivo = config_create(path_archivo);
    int bloque_inicial = config_get_int_value(config_achivo,"BLOQUE_INICIAL");
    free(path_archivo);
    return bloque_inicial;
}

int obtener_tamanio_de_archivo (char * nombre_archivo){
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);
    t_config* config_achivo = config_create(path_archivo);
    int tamanio_archivo = config_get_int_value(config_achivo,"TAMANIO_ARCHIVO");
    config_destroy(config_achivo);
    free(path_archivo);
    return tamanio_archivo;
}
//devuelve nuevo bloque final segun bloque inicial actual en archivo de indice
int obtener_nuevo_bloque_final (char * nombre_archivo, uint32_t tamanio_nuevo){
    int bloque_inicial = obtener_primer_bloque_de_archivo(nombre_archivo);
    int nuevo_bloque_final = (tamanio_nuevo / configuracion.BLOCK_SIZE)-1;
    if (tamanio_nuevo%configuracion.BLOCK_SIZE!=0) {
        nuevo_bloque_final+=1;
    }
    nuevo_bloque_final+=bloque_inicial;
    if(nuevo_bloque_final>configuracion.BLOCK_COUNT){
        log_error(logger_errores, "Error: el bloque final supera el limite del FS");
        return -1;
    }
    return nuevo_bloque_final;
}

int obtener_nuevo_bloque_final_compactado (uint32_t tamanio_nuevo, int bloque_inicial_nuevo){
    int nuevo_bloque_final = (tamanio_nuevo / configuracion.BLOCK_SIZE)-1;
    if (tamanio_nuevo%configuracion.BLOCK_SIZE!=0) {
        nuevo_bloque_final+=1;
    }
    nuevo_bloque_final+=bloque_inicial_nuevo;
    if(nuevo_bloque_final>configuracion.BLOCK_COUNT){
        log_error(logger_errores, "Error: el bloque final supera el limite del FS");
        return -1;
    }
    return nuevo_bloque_final;
}

int obtener_ultimo_bloque_de_archivo (char * nombre_archivo){
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);
    t_config* config_achivo = config_create(path_archivo);
    int bloque_inicial = config_get_int_value(config_achivo,"BLOQUE_INICIAL");
    int tamanio_archivo = config_get_int_value(config_achivo,"TAMANIO_ARCHIVO");
    int bloque_final = tamanio_archivo / configuracion.BLOCK_SIZE;
    bloque_final+=bloque_inicial;
    config_destroy(config_achivo);
    free(path_archivo);
    return bloque_final;
}

void procesar_io_fs_delete(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, char* nombre){
    /*
    IO_FS_DELETE (Interfaz, Nombre Archivo): Esta instrucción solicita al Kernel que mediante la interfaz seleccionada, 
    se elimine un archivo en el FS montado en dicha interfaz
    */
    usleep(configuracion.TIEMPO_UNIDAD_TRABAJO * 1000);
    uint32_t longitud_nombre_archivo;
    char* nombre_archivo = buffer_read_string(buffer_kernel, &longitud_nombre_archivo);
    char* path_archivo = string_new();
    string_append(&path_archivo, configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo, "/");
    string_append(&path_archivo,nombre_archivo);
    log_info(logger_fs, "PID: %u - Eliminar Archivo: %s",pid,nombre_archivo);
    int primer_bit_archivo = obtener_primer_bloque_de_archivo(nombre_archivo);
    int ultimo_bit_archivo = obtener_ultimo_bloque_de_archivo(nombre_archivo);
    if (remove(path_archivo) == 0) {
        log_info(logger_fs, "Archivo %s eliminado",nombre_archivo);
        limpiar_bits(bitmap,primer_bit_archivo,ultimo_bit_archivo);
    } else {
        log_error(logger_errores, "Error al eliminar el archivo");
    }
    eliminar_archivo_de_indice(nombre_archivo);
    free(nombre_archivo);
    free(path_archivo);
    enviar_fin_de_instruccion(socket_kernel, nombre);
}

int calcular_cantidad_de_bloques (int tamanio){
    int bloques_totales = tamanio / configuracion.BLOCK_SIZE;
    if(tamanio%configuracion.BLOCK_SIZE!=0){
        bloques_totales+=1;
    }
    return bloques_totales;
}


void limpiar_bits(t_bitarray* bitmap, off_t bit_inicial, off_t bit_final) {
    for (off_t i=bit_inicial;i<=bit_final;i++){
        bitarray_clean_bit(bitmap,i);
        log_info(logger_fs, "Bit %li eliminado",i);
    }
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
}

void setear_bits(t_bitarray* bitmap, off_t bit_inicial, off_t bit_final) {
    for (off_t i=bit_inicial;i<=bit_final;i++){
        bitarray_set_bit(bitmap,i);
        log_info(logger_fs, "Bit %li seteado",i);
    }
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
}


void procesar_io_fs_truncate(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, char* nombre){
    /*
    IO_FS_TRUNCATE (Interfaz, Nombre Archivo, Registro Tamaño): Esta instrucción solicita al Kernel que mediante la interfaz seleccionada, 
    se modifique el tamaño del archivo en el FS montado en dicha interfaz, actualizando al valor que se encuentra en el registro indicado 
    por Registro Tamaño.
    */
    usleep(configuracion.TIEMPO_UNIDAD_TRABAJO*1000);
    uint32_t longitud_nombre_archivo;
    char* nombre_archivo = buffer_read_string(buffer_kernel,&longitud_nombre_archivo);

    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);

    uint32_t tamanio_nuevo = buffer_read_uint32(buffer_kernel);
    log_info(logger_fs, "PID: %u - Truncar Archivo: %s - Tamaño %i",pid,nombre_archivo,tamanio_nuevo);
    
    int bloque_inicial_archivo = obtener_primer_bloque_de_archivo(nombre_archivo);
    int pre_bloque_final_archivo = obtener_ultimo_bloque_de_archivo(nombre_archivo);
    int post_bloque_final_archivo = obtener_nuevo_bloque_final (nombre_archivo,tamanio_nuevo); 

    if((post_bloque_final_archivo > pre_bloque_final_archivo) ||  post_bloque_final_archivo == -1) { //-1 representa que se excede de bloque maximo
        if(post_bloque_final_archivo!=-1 && !hay_bits_ocupados(bitmap, bloque_inicial_archivo,post_bloque_final_archivo)){ 
            //si hay espacio para extender, ocupo bits
            setear_bits(bitmap,bloque_inicial_archivo,post_bloque_final_archivo);
        } else{
            //si tenemos bloques suficientes, compactar, sino devolver error
            if((contar_bloques_libres(bitmap)+(pre_bloque_final_archivo-bloque_inicial_archivo)+1)>=calcular_cantidad_de_bloques(tamanio_nuevo)) { 
                int nuevo_bloque_inicial = compactar_y_acomodar_al_final(bloques,bitmap,bloque_inicial_archivo,pre_bloque_final_archivo,pid);
                //actualizar_valores
                bloque_inicial_archivo = nuevo_bloque_inicial;
                post_bloque_final_archivo = obtener_nuevo_bloque_final_compactado (tamanio_nuevo,bloque_inicial_archivo);
                setear_bits(bitmap,bloque_inicial_archivo+1,post_bloque_final_archivo);
            } else {
                log_error(logger_errores, "Error al truncar: no hay suficiente espacio libre en FS");
                return;
            }
        }
    }
    if (post_bloque_final_archivo < pre_bloque_final_archivo) {
        //https://github.com/sisoputnfrba/foro/issues/3993 no es necesario limpiar, puede quedar basura en el archivo
        limpiar_bits(bitmap,bloque_inicial_archivo+1,post_bloque_final_archivo);
    } 
    t_config* config_archivo = config_create(path_archivo);
    config_set_value(config_archivo,"BLOQUE_INICIAL",string_itoa(bloque_inicial_archivo));
    config_set_value(config_archivo,"TAMANIO_ARCHIVO",string_itoa(tamanio_nuevo));
    config_save(config_archivo);
    config_destroy(config_archivo);
    free(nombre_archivo);
    free(path_archivo);
    enviar_fin_de_instruccion(socket_kernel,nombre);
}

bool hay_bits_ocupados(t_bitarray* bitmap, int inicio, int fin){
    for (int i=inicio; i<fin;i++) {
        if(bitarray_test_bit(bitmap,i)){
            return true;
        }
    }
    return false;
}



int compactar_y_acomodar_al_final(void **bloques, t_bitarray *bitmap, int bloque_inicial, int bloque_final, uint32_t pid) {
    /*
    DialFS - Inicio Compactación: “PID: <PID> - Inicio Compactación.” 
    DialFS - Fin Compactación: “PID: <PID> - Fin Compactación.”
    */
    log_info(logger_fs, "PID: %u - Inicio Compactación",pid);
    int indice_auxiliar = 0;
    int cantidad_a_mover = bloque_final - bloque_inicial +1;
    void **bloques_auxiliares = malloc(cantidad_a_mover * sizeof(void *));
    char* archivo_a_desplazar = string_new();
    if(buscar_archivo_en_indice(bloque_inicial,archivo_a_desplazar)==-2) { 
        log_error(logger_errores,"Archivo no encontrado en indices.dat");
        free(archivo_a_desplazar);
        return -1;
    }
    //Guardo temporalmente los bloques que quiero dejar al final y marco como libre el bitmap en esas posiciones
    for (int j = 0; j < cantidad_a_mover; ++j) {
        int i = bloque_inicial+j;
        bloques_auxiliares[j] = malloc(configuracion.BLOCK_SIZE);
        memcpy(bloques_auxiliares[j], bloques[i], configuracion.BLOCK_SIZE);
        bitarray_clean_bit(bitmap,i); 
        log_info(logger_fs, "Bit %i liberado",i);
    }
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);

    // Muevo todos los bloques ocupados a las primeras posiciones libres
    for (int i = 0; i < configuracion.BLOCK_COUNT; ++i) {
        if (bitarray_test_bit(bitmap, i)) {
            if (i != indice_auxiliar) { //si el bit no se encuentra en la primer posicion libre (indice auxiliar comienza en 0)
                void *bloque_desplazado = bloques[i];
                void *posicion_libre = bloques[indice_auxiliar];
                memcpy(posicion_libre, bloque_desplazado, configuracion.BLOCK_SIZE);
                bitarray_set_bit(bitmap, indice_auxiliar);
                log_info(logger_fs, "Bit %i seteado",indice_auxiliar);
                bitarray_clean_bit(bitmap, i);
                log_info(logger_fs, "Bit %i liberado",i);
                //MODIFICO CONFIG DE ARCHIVO USANDO ARCHIVO DE INDICE
                modificar_bloque_inicial_indice(i,indice_auxiliar);
            }
            indice_auxiliar++;
        }
    }
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
    msync(bloques,tamanio_memoria_bloques,MS_SYNC);
    int nueva_posicion_inicial = indice_auxiliar;
    if(modificar_bloque_inicial_indice_por_nombre(archivo_a_desplazar,indice_auxiliar)==-1){
                log_error(logger_errores,"Error al reemplazar bloque inicial en archivo de indices");
                return -1;
    }
    // Muevo los bloques almacenados temporalmente a las últimas posiciones libres
    for (size_t j = 0; j < cantidad_a_mover; ++j) {
        if (indice_auxiliar < configuracion.BLOCK_COUNT) {
            void *posicion_libre = bloques[indice_auxiliar];
            memcpy(posicion_libre, bloques_auxiliares[j], configuracion.BLOCK_SIZE);
            bitarray_set_bit(bitmap, indice_auxiliar);
            log_info(logger_fs, "Bit %i seteado",indice_auxiliar);
            indice_auxiliar++;
        }
        free(bloques_auxiliares[j]); 
    }
    free(bloques_auxiliares);
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
    msync(bloques,tamanio_memoria_bloques,MS_SYNC);
    /* NO ES NECESARIO
    // Limpio los bloques que quedan al final de la compactacion (si hubiere)
    for (size_t i = indice_auxiliar; i < configuracion.BLOCK_COUNT; ++i) {
        bitarray_clean_bit(bitmap, i);
        log_info(logger_fs, "Bit %li liberado",i);
    }
    */
    msync(bitmap,bitarray_get_max_bit(bitmap),MS_SYNC);
    usleep(configuracion.RETRASO_COMPACTACION);
    log_info(logger_fs,"PID: %u - Fin Compactación",pid);
    return nueva_posicion_inicial;
}

 void agregar_archivo_a_indice(char* nombre_archivo, int bloque) {
    FILE* archivo_indice = fopen(path_indice, "a");
    if(archivo_indice == NULL) {
        log_error(logger_errores,"Error al abrir archivo de indices 1");
        return;
    }
    t_indice indice_del_archivo;
    indice_del_archivo.indice_de_bloques = bloque;
    strncpy(indice_del_archivo.nombre_archivo,nombre_archivo,PATH_MAX);
    fwrite(&indice_del_archivo,sizeof(t_indice),1,archivo_indice);
    fclose(archivo_indice);
 }

 void eliminar_archivo_de_indice(char* nombre_archivo) {
    FILE* archivo_indice = fopen(path_indice, "r");
    if(archivo_indice==NULL) {
        log_error(logger_errores,"Error al abrir archivo de indices 2");
        return;
    }
    int fd_archivo_temporal = open("indice_temporal.dat", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_archivo_temporal == -1) {
        log_error(logger_errores,"Error al crear archivo de indices auxiliar");
        fclose(archivo_indice);
        return;
    }
    FILE* archivo_temporal = fdopen(fd_archivo_temporal,"wb");
    if(archivo_temporal==NULL){
        log_error(logger_errores,"Error al abrir archivo de indices auxiliar");
        fclose(archivo_indice);
        close(fd_archivo_temporal);
        return;
    }
    t_indice indice_aux;
    while(fread(&indice_aux,sizeof(t_indice),1,archivo_indice)==1) { //recorro archivo de indices
        if(strcmp(indice_aux.nombre_archivo,nombre_archivo)!=0) { //si el indice no coincide, lo grabo en el archivo temporal
            fwrite(&indice_aux,sizeof(t_indice),1,archivo_temporal);
        }
    }
    fclose(archivo_indice);
    fclose(archivo_temporal);
    if(remove("indice.dat") == 0) {
        log_info(logger_fs,"Archivo de indices original borrado");
    }
    rename("indice_temporal.dat","indice.dat"); //quizas podria usar cosntances para evitar problemas debuggeando
    return;
 }

 int buscar_archivo_en_indice(int bloque_inicial, char* nombre_archivo) { //devuelve -1 si hay error y -2 si no encuentra el archivo
    FILE* archivo_indices = fopen(path_indice, "rb");
    if(archivo_indices == NULL){
        log_error(logger_errores,"Error al abrir archivo de indices");
        return -1;
    }
    t_indice indice_del_archivo;
    while (fread(&indice_del_archivo,sizeof(t_indice),1,archivo_indices)==1){
        if(indice_del_archivo.indice_de_bloques == bloque_inicial) {
            strcpy(nombre_archivo,indice_del_archivo.nombre_archivo);
            fclose(archivo_indices);
            return 0;
        }
    }
    fclose(archivo_indices);
    return -2;
 }

int modificar_archivo_indices(int bloque_original, int nuevo_bloque) {
    FILE* archivo_indices = fopen(path_indice, "r+");
    if(archivo_indices == NULL){
        log_error(logger_errores,"Error al abrir archivo de indices");
        return -1;
    }
    t_indice indice_del_archivo;
    while (fread(&indice_del_archivo,sizeof(t_indice),1,archivo_indices)==1){
        if(indice_del_archivo.indice_de_bloques == bloque_original) {
            indice_del_archivo.indice_de_bloques = nuevo_bloque;
            fseek(archivo_indices, -sizeof(t_indice), SEEK_CUR);
            fwrite(&indice_del_archivo, sizeof(t_indice), 1, archivo_indices);
            fclose(archivo_indices);
            return 0;
        }
    }
    fclose(archivo_indices);
    return -1;
}

int modificar_bloque_inicial_indice_por_nombre (char* nombre_archivo_a_cambiar, int nuevo_bloque) {
    FILE* archivo_indices = fopen(path_indice, "r+");
    if(archivo_indices == NULL){
        log_error(logger_errores,"Error al abrir archivo de indices");
        return -1;
    }
    t_indice indice_del_archivo;
    while (fread(&indice_del_archivo,sizeof(t_indice),1,archivo_indices)==1){
        if(strcmp(indice_del_archivo.nombre_archivo, nombre_archivo_a_cambiar) == 0) {
            indice_del_archivo.indice_de_bloques = nuevo_bloque;
            fseek(archivo_indices, -sizeof(t_indice), SEEK_CUR);
            fwrite(&indice_del_archivo, sizeof(t_indice), 1, archivo_indices);
            fclose(archivo_indices);
            return 0;
        }
    }
    fclose(archivo_indices);
    return -1;

}


 void modificar_bloque_inicial_indice (int bloque_inicial, int bloque_modificado) {
    char* nombre_del_archivo = string_new();

    if(buscar_archivo_en_indice(bloque_inicial,nombre_del_archivo)==-2) { //si el bit no corresponde a un bloque inicial de archivo, no hago nada
        free(nombre_del_archivo);
        return;
    }
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_del_archivo);    
    t_config* config_archivo = config_create(path_archivo);
    config_set_value(config_archivo,"BLOQUE_INICIAL",string_itoa(bloque_modificado));
    config_save(config_archivo);
    if(modificar_archivo_indices(bloque_inicial, bloque_modificado) == -1) {
        log_error(logger_errores,"Error al actualizar archivo de indices");
    }
    config_destroy(config_archivo);
    free(nombre_del_archivo);
    free(path_archivo);
 }

void procesar_io_fs_write(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, int socket_memoria, char* nombre){
    /*
        IO_FS_WRITE (Interfaz, Nombre Archivo, Registro Dirección, Registro Tamaño, Registro Puntero Archivo): 
        Esta instrucción solicita al Kernel que mediante la interfaz seleccionada, se lea desde Memoria la cantidad de bytes indicadas por el 
        Registro Tamaño a partir de la dirección lógica que se encuentra en el Registro Dirección y se escriban en el archivo a partir del valor 
        del Registro Puntero Archivo.
    */
    usleep(configuracion.TIEMPO_UNIDAD_TRABAJO*1000);
    uint32_t accion =0;
    uint32_t longitud_nombre_archivo;
    char* nombre_archivo = buffer_read_string(buffer_kernel,&longitud_nombre_archivo);
    uint32_t cant_paginas = buffer_read_uint32(buffer_kernel);
    int direcciones_fisicas_memoria_a_leer[cant_paginas];  
    int tamanio_direcciones_a_leer[cant_paginas]; 
    uint32_t tamanio_array_direcciones = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, direcciones_fisicas_memoria_a_leer, tamanio_array_direcciones);
    uint32_t tamanio_arrays_tam_a_leer = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, tamanio_direcciones_a_leer, tamanio_arrays_tam_a_leer);
    uint32_t puntero_archivo = buffer_read_uint32(buffer_kernel);
    int cont_tamanio = 0;
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);

    t_config* config_archivo = config_create(path_archivo);
    if (config_archivo == NULL) {
        log_error(logger_fs, "Error creando la configuración del archivo\n");
        free(nombre_archivo);
        free(path_archivo);
        return;
    }

    int bloque_inicial_archivo = config_get_int_value(config_archivo, "BLOQUE_INICIAL");
    if (bloque_inicial_archivo == -1) {
        log_error(logger_errores, "Error obteniendo el valor de BLOQUE_INICIAL\n");
        free(nombre_archivo);
        free(path_archivo);
        config_destroy(config_archivo);
        return;
    }

    void* info_memoria = NULL;
    int offset_memoria = 0;
    for(int i = 0 ; i < cant_paginas ; i++){
        cont_tamanio += tamanio_direcciones_a_leer[i];
        t_paquete* paquete = crear_paquete(ACCESS_ESPACIO_USUARIO_ES, sizeof(uint32_t) * 4);
        buffer_add_uint32(paquete->buffer, pid);
        buffer_add_uint32(paquete->buffer, accion);
        buffer_add_uint32(paquete->buffer, (uint32_t)direcciones_fisicas_memoria_a_leer[i]);
        buffer_add_uint32(paquete->buffer, (uint32_t)tamanio_direcciones_a_leer[i]);
        enviar_paquete(paquete, socket_memoria);
        op_code codigo_leido = recibir_operacion(socket_memoria);
        if(codigo_leido != RESPUESTA_LECTURA_MEMORIA){
            log_error(logger_errores, "Hubo un error al recibir el codigo de lectura de memoria");
        }
        t_buffer* memoria = recibir_todo_elbuffer (socket_memoria);
        uint32_t length = buffer_read_uint32(memoria);
        // Lógica para aumentar el tamaño del puntero en length
        void* temp = realloc(info_memoria, offset_memoria + length);
        if (temp == NULL) {
            log_error(logger_fs, "Error reallocando memoria\n");
            free(info_memoria);
            free(nombre_archivo);
            free(path_archivo);
            config_destroy(config_archivo);
            buffer_destroy(memoria);
            return;
        }

        info_memoria = temp;
        buffer_read(memoria, info_memoria + offset_memoria, length);
        offset_memoria += length;
        buffer_destroy(memoria);
    }
    log_info(logger_fs, "PID: %u - Escribir Archivo: %s - Tamaño a Escribir: %i - Puntero Archivo: %i", 
                pid,nombre_archivo,cont_tamanio,puntero_archivo);
               
    escribir_en_fs(bloque_inicial_archivo,puntero_archivo,cont_tamanio,info_memoria);
   
    config_destroy(config_archivo);
    free(nombre_archivo);
    free(path_archivo);
    free(info_memoria);
    enviar_fin_de_instruccion(socket_kernel, nombre);
}


void escribir_en_fs (int indice_bloques, uint32_t offset, uint32_t tamanio, void* data) {
    uint32_t bytes_escritos = 0;
    uint32_t bytes_pendientes_escritura = tamanio;
    uint32_t offset_de_bloques = offset/configuracion.BLOCK_SIZE;
    uint32_t offset_dentro_de_bloque = offset % configuracion.BLOCK_SIZE;
    uint32_t nuevo_indice = indice_bloques + offset_de_bloques;

    
    while(bytes_escritos<tamanio){
        //si lo que tengo que escribir supera lo que tengo remanente en el bloque
       
        int bytes_a_escribir = min(bytes_pendientes_escritura,configuracion.BLOCK_SIZE-offset_dentro_de_bloque);
        memcpy(bloques[nuevo_indice] + offset_dentro_de_bloque, data + bytes_escritos, bytes_a_escribir);
        char* a = string_new();
        string_n_append(&a,bloques[nuevo_indice] + offset_dentro_de_bloque,bytes_a_escribir);
        log_info(logger_fs, "Escribi en el bloque: %i - String escrito: %s", nuevo_indice, a);
        bytes_escritos += bytes_a_escribir;
        nuevo_indice ++;
        offset_dentro_de_bloque = 0;
        bytes_pendientes_escritura -= bytes_a_escribir;
        
    }
}


void procesar_io_fs_read(t_buffer* buffer_kernel, uint32_t pid, int socket_kernel, int socket_memoria, char* nombre){
    usleep(configuracion.TIEMPO_UNIDAD_TRABAJO*1000);
    
    uint32_t longitud_nombre_archivo;
    char* nombre_archivo = buffer_read_string(buffer_kernel,&longitud_nombre_archivo);
    uint32_t cant_paginas = buffer_read_uint32(buffer_kernel);
    int direcciones_fisicas_memoria_a_escribir[cant_paginas];  
    int tamanio_direcciones_a_escribir[cant_paginas]; 
    uint32_t tamanio_array_direcciones = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, direcciones_fisicas_memoria_a_escribir, tamanio_array_direcciones);
    uint32_t tamanio_arrays_tam_a_escribir = buffer_read_uint32(buffer_kernel);
    buffer_read(buffer_kernel, tamanio_direcciones_a_escribir, tamanio_arrays_tam_a_escribir);
    uint32_t puntero_archivo = buffer_read_uint32(buffer_kernel);
    int cont_tamanio = 0;
    char* path_archivo = string_new();
    string_append(&path_archivo,configuracion.PATH_BASE_DIALFS);
    string_append(&path_archivo,"/");
    string_append(&path_archivo,nombre_archivo);

    t_config* config_archivo = config_create(path_archivo);
    if (config_archivo == NULL) {
        log_error(logger_errores, "Error creando la configuración del archivo\n");
        free(nombre_archivo);
        free(path_archivo);
        return;
    }
    int bloque_inicial_archivo = config_get_int_value(config_archivo, "BLOQUE_INICIAL");
    if (bloque_inicial_archivo == -1) {
        log_error(logger_errores, "Error obteniendo el valor de BLOQUE_INICIAL\n");
        free(nombre_archivo);
        free(path_archivo);
        config_destroy(config_archivo);
        return;
    }

    int offset_archivo = 0;
    for(int i = 0, j=0 ; i < cant_paginas ; i++){
            while(tamanio_direcciones_a_escribir[i] > 0){
                int tamanio_a_escribir = min(configuracion.BLOCK_SIZE-puntero_archivo, tamanio_direcciones_a_escribir[i]);
                int offset_memoria = 0;
                void* data_a_escribir = malloc(tamanio_a_escribir);
                memcpy(data_a_escribir,bloques[bloque_inicial_archivo+j]+puntero_archivo+offset_archivo,tamanio_a_escribir);
                offset_archivo += tamanio_a_escribir;
                if((configuracion.BLOCK_SIZE - puntero_archivo - offset_archivo) <= 0){
                    offset_archivo = 0;
                    j++;
                }
                puntero_archivo = 0;
                escribir_en_memoria(data_a_escribir,pid,direcciones_fisicas_memoria_a_escribir[i]+offset_memoria,tamanio_a_escribir ,socket_memoria);
                free(data_a_escribir);
                offset_memoria += tamanio_a_escribir;
                tamanio_direcciones_a_escribir[i] -= tamanio_a_escribir;
            }
    }
  
  
    log_info(logger_fs, "PID: %u - Escribir Archivo: %s - Tamaño a Leer: %i - Puntero Archivo: %i",
                pid,nombre_archivo,cont_tamanio,puntero_archivo);
    config_destroy(config_archivo);
    free(nombre_archivo);
    free(path_archivo);
    enviar_fin_de_instruccion(socket_kernel, nombre);
    
    
}