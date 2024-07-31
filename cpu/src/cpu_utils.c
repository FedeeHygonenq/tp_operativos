#include "cpu_utils.h"

#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b? a:b)
int ind_contexto_kernel = 0;
sem_t sem_contexto_kernel;
str_sockets sockets;
int llego_interrupcion = 0;
uint32_t tamanio_pagina;
sem_t sem_inter;
sem_t sem_contexto;
sem_t sem_apagar_cpu;

void iniciar_server_kernel(){
    pthread_t dispatch, interrupt;
    pthread_create(&dispatch,NULL,(void*)inicializar_cpu_dispatch,NULL);
    pthread_create(&interrupt,NULL,(void*)inicializar_cpu_interrupt,NULL);
    pthread_join(dispatch,NULL);
    pthread_join(interrupt,NULL);
}

void inicializar_cpu_dispatch(){
    sockets.socket_server_D = iniciar_servidor(string_itoa(configuracion.PUERTO_ESCUCHA_DISPATCH));
    log_info(logger_conexiones, "CPU DISPATCH esta escuchando");
}
    
void inicializar_cpu_interrupt(){
    sockets.socket_server_I = iniciar_servidor(string_itoa(configuracion.PUERTO_ESCUCHA_INTERRUPT));
    log_info(logger_conexiones, "CPU INTERRUPT esta escuchando");
    
}

void establecer_conexion_memoria(){
    int fd_memoria = crear_conexion(configuracion.IP_MEMORIA,string_itoa(configuracion.PUERTO_MEMORIA),logger_conexiones,"CPU");
    log_info(logger_conexiones, "Conectado-CPU-memoria");
    sockets.socket_memoria = fd_memoria;
    solicitar_tamanio_pagina();
}

void atender_conexiones(){
    sleep(1);
    pthread_t ki, kd;
    pthread_create(&ki,NULL,(void*)server_escuchar,(void*)(sockets.socket_server_I));
    pthread_create(&kd,NULL,(void*)server_escuchar,(void*)(sockets.socket_server_D));
    pthread_join(ki,NULL);
    pthread_join(kd,NULL);
}

int server_escuchar(int server_socket) {
    char* nom_cliente = malloc(20);
    int cliente_socket = esperar_cliente(server_socket,logger_conexiones,nom_cliente);

    if (cliente_socket != -1) {
        pthread_t hilo;
        t_procesar_conexion_args* args = malloc(sizeof(t_procesar_conexion_args));
        args->fd = cliente_socket;
        args->cliente_name = nom_cliente;
        pthread_create(&hilo, NULL, (void*) procesar_conexion, (void*) args);
        pthread_detach(hilo);
        return 1;

    }
    return 0;
}

//PROBABLEMENTE HAYA QUE CAMBIAR COSAS.
void procesar_conexion(void* void_args) {
    t_procesar_conexion_args* args = (t_procesar_conexion_args*) void_args;
    int cliente_socket = args->fd;
    char* nombre_cliente = args->cliente_name;
    free(args);

    op_code codigo_op;
    if(strcmp(nombre_cliente, "KERNEL_I") == 0){
        sockets.socket_kernel_I = cliente_socket;
    }
    if(strcmp(nombre_cliente, "KERNEL_D") == 0){
    sockets.socket_kernel_D = cliente_socket;
    }
    while (cliente_socket != -1) {

        if (recv(cliente_socket, &codigo_op, sizeof(op_code), 0) != sizeof(op_code)) {
            log_info(logger_conexiones, "%s DISCONNECT!", nombre_cliente);
            free(nombre_cliente);
            
            return;
        }

        switch (codigo_op) {
        case CONTEXTO_EXEC:
            sem_wait(&sem_contexto);
            ind_contexto_kernel = 1;
            proceso_enviado = 0;
            recibir_contexto_ejecucion(cliente_socket);
            op_code cod = CONTEXTO_RECIBIDO;
            send(cliente_socket, &cod, sizeof(op_code), 0);
            sem_post(&sem_contexto);
            while(ind_contexto_kernel == 1){
                ciclo_de_instruccion();
            }
            break;

        case INTERRUPT_PROC:
            log_info(logger_cpu, "Se recibió una interrupción de proceso de parte del Kernel.");
            recibir_interrupcion_de_kernel();
            sem_wait(&sem_inter);
            llego_interrupcion++;
            sem_post(&sem_inter);
            break;

        case APAGAR:
            sem_post(&sem_apagar_cpu);
            break;
        
        default:
            log_info(logger_errores_cpu, "Se recibió un código de operación incorrecto de parte del Kernel. El mismo fue: %s", string_itoa(codigo_op));
            break;
        }
        
    }

    free(nombre_cliente);
}

void recibir_contexto_ejecucion(int cliente_socket){
    t_buffer* buffer = recibir_todo_elbuffer(cliente_socket);
    PID =  buffer_read_uint32(buffer);
    buffer_read(buffer,contexto_registros, sizeof(registros_CPU));

    buffer_destroy(buffer);
}

//NO SÉ SI ESTÁ BIEN. CHECKEAR.
void recibir_interrupcion_de_kernel(){
    t_buffer* buffer = recibir_todo_elbuffer(sockets.socket_kernel_I);
    buffer_read(buffer, &motivo_interrupcion, sizeof(tipo_de_interrupcion));
    buffer_destroy(buffer);    
}

void inicializar_estructuras(){
    contexto_registros = malloc(sizeof(registros_CPU));
    longitud_linea_instruccion = malloc(sizeof(uint32_t));
    tlb = malloc(sizeof(t_TLB));
    inicializar_TLB(configuracion.CANTIDAD_ENTRADAS_TLB, configuracion.ALGORITMO_TLB);
    sem_init(&sem_apagar_cpu, 0, 0);
    sem_init(&sem_inter, 0, 1);
    sem_init(&sem_contexto, 0, 1);
    sem_init(&mutex_log, 0, 1);
}

void destruir_estructuras(){
    free(contexto_registros);
    free(longitud_linea_instruccion);
    destruir_TLB();
    sem_destroy(&mutex_log);
    sem_destroy(&sem_contexto);
    sem_destroy(&sem_inter);
    sem_destroy(&sem_apagar_cpu);
}

void enviar_contexto_a_kernel(motivo_desalojo motivo){
    sem_wait(&sem_contexto);
    ind_contexto_kernel = 0;
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(registros_CPU) + sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, &motivo, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    enviar_paquete(paquete, sockets.socket_kernel_D);
    sem_post(&sem_contexto);
    proceso_enviado = 1;
}

void envios_de_std_a_kernel(t_instruccion motivo_io, char* nombre_interfaz,
                           uint32_t tamanio_data, t_buffer* buffer){ // tamanio std
    uint32_t direc_fisica = mmu(PID);                        
    uint32_t offset = direc_fisica % tamanio_pagina;                                             
    uint32_t tamanio_std;  
    if(tamanio_data == sizeof(uint32_t)){
        tamanio_std = buffer_read_uint32(buffer);
    }else{
        tamanio_std = buffer_read_uint8(buffer);
    }
    uint32_t tamanio_disponible = min(tamanio_pagina - offset, tamanio_std);
    
    uint32_t cantidad_paginas = cantidad_paginas_que_ocupa(tamanio_std, offset);
    int direccion_fisica[cantidad_paginas]; 
    int tam_a_escribir[cantidad_paginas];
    int contador =0;
    while(cantidad_paginas > contador){

        direccion_fisica[contador] = direc_fisica;
        tam_a_escribir[contador] = tamanio_disponible;
        tamanio_std = max(0 ,(int)tamanio_std - (int)tamanio_disponible);
        if(tamanio_std > 0){
        dir_logica += tamanio_disponible;
        tamanio_disponible = min(tamanio_pagina , tamanio_std);
        direc_fisica = mmu(PID);
        }
        contador++;
    }

    enviar_std_a_kernel(motivo_io, nombre_interfaz,cantidad_paginas, direccion_fisica,tam_a_escribir);

    
}

void enviar_std_a_kernel(t_instruccion motivo_io, char* nombre_interfaz,int cant_paginas,int direccion_fisica[] , int tam_a_escribir[]){
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(t_instruccion) + sizeof(registros_CPU)
                                        + string_length(nombre_interfaz)+1+sizeof(uint32_t)*3
                                        + sizeof(int)+sizeof(int)* cant_paginas * 2);
    motivo_desalojo mot_des = PETICION_IO;
    buffer_add(paquete->buffer, &mot_des, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add(paquete->buffer, &motivo_io, sizeof(t_instruccion));
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    buffer_add(paquete->buffer, &cant_paginas,sizeof(int));
    buffer_add_uint32(paquete->buffer,sizeof(int) * cant_paginas);
    buffer_add(paquete->buffer, direccion_fisica,sizeof(int) * cant_paginas);
    buffer_add_uint32(paquete->buffer,sizeof(int)* cant_paginas);
    buffer_add(paquete->buffer, tam_a_escribir,sizeof(int) * cant_paginas);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void solicitar_create_delete_fs_a_kernel(t_instruccion motivo_io, char* nombre_interfaz, char* nombre_archivo){
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(t_instruccion) + sizeof(registros_CPU)
                                        + string_length(nombre_interfaz)+1 + string_length(nombre_archivo)+1 + sizeof(uint32_t)*2);
    motivo_desalojo mot_des = PETICION_IO;
    buffer_add(paquete->buffer, &mot_des, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add(paquete->buffer, &motivo_io, sizeof(t_instruccion));
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    buffer_add_string(paquete->buffer, string_length(nombre_archivo)+1, nombre_archivo);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void solicitar_truncate_fs_a_kernel(t_instruccion motivo_io, char* nombre_interfaz, char* nombre_archivo,
                                    uint32_t tamanio_fs){
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(t_instruccion) + sizeof(registros_CPU)
                                        + string_length(nombre_interfaz)+1 + string_length(nombre_archivo)+1
                                         + sizeof(uint32_t)*3);
    motivo_desalojo mot_des = PETICION_IO;
    buffer_add(paquete->buffer, &mot_des, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add(paquete->buffer, &motivo_io, sizeof(t_instruccion));
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    buffer_add_string(paquete->buffer, string_length(nombre_archivo)+1, nombre_archivo);
    buffer_add_uint32(paquete->buffer, tamanio_fs);
    

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void solicitudes_fs_a_kernel(t_instruccion motivo_io, char* nombre_interfaz, char* nombre_archivo,
                                t_buffer* buffer, uint32_t tamanio_data1, //tamaño buffer
                                uint32_t puntero_archivo){ 
    uint32_t direc_fisica = mmu(PID);                        
    uint32_t offset = direc_fisica % tamanio_pagina;
    uint32_t tamanio_fs; //cant bytes a leer/escribir
    if(tamanio_data1 == sizeof(uint32_t)){
        tamanio_fs = buffer_read_uint32(buffer);
    }else{
        tamanio_fs = buffer_read_uint8(buffer);
    }
    uint32_t tamanio_disponible = min(tamanio_pagina - offset, tamanio_fs);
    
    uint32_t cantidad_paginas = cantidad_paginas_que_ocupa(tamanio_fs, offset);
    int direccion_fisica[cantidad_paginas]; 
    int tam_a_escribir[cantidad_paginas];
    int contador =0;
    while(cantidad_paginas > contador){

        direccion_fisica[contador] = direc_fisica;
        tam_a_escribir[contador] = tamanio_disponible;
        tamanio_fs = max(0 ,(int)tamanio_fs- (int)tamanio_disponible);
        if(tamanio_fs > 0){
        dir_logica += tamanio_disponible;
        tamanio_disponible = min(tamanio_pagina , tamanio_fs);
        direc_fisica = mmu(PID);
        }
        contador++;
    }
    
    solicitar_write_read_fs_a_kernel(motivo_io, nombre_interfaz, nombre_archivo, cantidad_paginas, direccion_fisica, tam_a_escribir, puntero_archivo);
    
    
}



void solicitar_write_read_fs_a_kernel(t_instruccion motivo_io, char* nombre_interfaz, char* nombre_archivo,
                                        uint32_t cantidad_paginas, int direccion_fisica[], int tam_a_escribir[],
                                        uint32_t puntero_archivo){
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(t_instruccion) + sizeof(registros_CPU)
                                        + string_length(nombre_interfaz)+1 + string_length(nombre_archivo)+1
                                        + sizeof(uint32_t)*6 + sizeof(int) * cantidad_paginas * 2);
    motivo_desalojo mot_des = PETICION_IO;
    buffer_add(paquete->buffer, &mot_des, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add(paquete->buffer, &motivo_io, sizeof(t_instruccion));
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    buffer_add_string(paquete->buffer, string_length(nombre_archivo)+1, nombre_archivo);
    buffer_add_uint32(paquete->buffer, cantidad_paginas);
    buffer_add_uint32(paquete->buffer,sizeof(int) * cantidad_paginas);
    buffer_add(paquete->buffer, direccion_fisica,sizeof(int) * cantidad_paginas);
    buffer_add_uint32(paquete->buffer,sizeof(int)* cantidad_paginas);
    buffer_add(paquete->buffer, tam_a_escribir,sizeof(int) * cantidad_paginas);
    buffer_add_uint32(paquete->buffer, puntero_archivo);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;//deberia estar en todos los entrada y salida
}

void solicitar_tamanio_pagina(){
    op_code* codigo_operacion = malloc(sizeof(op_code));
    *codigo_operacion = SOLICITUD_TAMANIO_PAGINA;

    send(sockets.socket_memoria, codigo_operacion, sizeof(op_code),0);

    
    *codigo_operacion = recibir_operacion(sockets.socket_memoria);
    
    if(*codigo_operacion == TAMANIO_PAGINA){
        t_buffer* buffer_memoria = recibir_todo_elbuffer(sockets.socket_memoria);

        tamanio_pagina =  buffer_read_uint32(buffer_memoria);
    }
    else{
        log_info(logger_errores_cpu, "El código de operación recibido no fue el esperado de memoria. El mismo fue: %d", *codigo_operacion);
    }
    free(codigo_operacion);
}

void solicitar_instruccion_a_memoria(){
    t_paquete* paquete = crear_paquete(SOLICITUD_INSTRUCCION, sizeof(uint32_t) * 2);
    buffer_add_uint32(paquete->buffer, PID);
    buffer_add_uint32(paquete->buffer, contexto_registros->PC);

    enviar_paquete(paquete, sockets.socket_memoria);
}

void recibir_instruccion_de_memoria(){
    op_code codigo = recibir_operacion(sockets.socket_memoria);
    if(codigo == INSTRUCCION){
        t_buffer* buffer = recibir_todo_elbuffer(sockets.socket_memoria);
        linea_de_instruccion = buffer_read_string(buffer, longitud_linea_instruccion);
        buffer_destroy(buffer);
    }
    else{
        log_info(logger_errores_cpu, "El código de operación recibido de Memoria no fue una instrucción. El código de operación recibido fue: %d", codigo);
    }
    
    
}

void recibir_respuesta_resize_memoria(uint32_t PID){
    op_code code = recibir_operacion(sockets.socket_memoria);

    switch (code){
    case NO_RESIZE:
        enviar_contexto_a_kernel(OUT_OF_MEMORY);
        log_error(logger_errores_cpu, "El resize del proceso %u sufrio un OUT_OF_MEMORY.", PID);
        break;
    
    case RESIZE_SUCCESS:
        log_info(logger_cpu, "El resize del proceso %u fue exitoso.", PID);
        break;

    default:
        log_error(logger_errores_cpu, "Ocurrió un error recibiendo la respuesta de Memoria tras Resize. El código de operación recibido fue: %d", code);
        break;
    }
}

uint32_t cantidad_paginas_que_ocupa(uint32_t tamanio, uint32_t desplazamiento){
    return ceil(((double)(tamanio + desplazamiento) / (double)tamanio_pagina));
}

void* leer_en_memoria_mas_de_una_pagina(t_buffer* buffer_auxiliar, uint32_t tamanio_auxiliar, uint32_t tamanio_total){
    uint32_t direccion_fisica = mmu(PID);
    uint32_t offset = direccion_fisica % tamanio_pagina;
    uint32_t tamanio_a_leer = min(tamanio_auxiliar, tamanio_pagina-offset);
    void* leido =  solicitar_leer_en_memoria(direccion_fisica, tamanio_a_leer);
    
    buffer_add(buffer_auxiliar, leido, tamanio_a_leer);

    tamanio_auxiliar = max(0,(int)(tamanio_auxiliar - tamanio_a_leer));
    
    
    dir_logica = dir_logica + tamanio_a_leer;
    if(tamanio_auxiliar > 0){
     
        leer_en_memoria_mas_de_una_pagina(buffer_auxiliar, tamanio_auxiliar, tamanio_total);    
    }
    else{
        void* retorno = malloc(tamanio_total);
        buffer_auxiliar->offset = 0 ;
        buffer_read(buffer_auxiliar, retorno, tamanio_total);
        return retorno;
    }
}

void escribir_en_memoria_mas_de_una_pagina(t_buffer* buffer_auxiliar, uint32_t tamanio_total){
    uint32_t direccion_fisica = mmu(PID);
    uint32_t offset = direccion_fisica % tamanio_pagina;
    uint32_t tamanio_a_escribir = min(tamanio_total, tamanio_pagina-offset);
    void* data_a_escribir = malloc(tamanio_a_escribir);
    
    buffer_read(buffer_auxiliar, data_a_escribir, tamanio_a_escribir);
    tamanio_total -= tamanio_a_escribir;
    
    solicitar_escribir_en_memoria(direccion_fisica, data_a_escribir, tamanio_a_escribir);
    
    if(recibir_operacion(sockets.socket_memoria) != OK_ESCRITURA){
        log_error(logger_errores_cpu, "NO RECIBIO OK! )L:L)");
    }

    free(data_a_escribir);
    dir_logica = dir_logica + tamanio_a_escribir;
    if(tamanio_total > 0){
        escribir_en_memoria_mas_de_una_pagina(buffer_auxiliar, tamanio_total);
    }
    else{
        return;
    }
}


void* solicitar_leer_en_memoria(uint32_t direccion_fisica, uint32_t tamanio){
    t_paquete* paquete = crear_paquete(ACCESS_ESPACIO_USUARIO_CPU, sizeof(uint32_t) * 4);
    
    buffer_add_uint32(paquete->buffer, PID);
    buffer_add_uint32(paquete->buffer, (uint32_t) 0 ); // 1 escribir, 0 leer
    buffer_add_uint32(paquete->buffer, direccion_fisica);
    buffer_add_uint32(paquete->buffer, tamanio);

    enviar_paquete(paquete, sockets.socket_memoria);
    return leer_de_memoria();
}

void* leer_de_memoria(){
    
    op_code codigo = recibir_operacion(sockets.socket_memoria);
    if(codigo == RESPUESTA_LECTURA_MEMORIA){

        t_buffer* buffer = recibir_todo_elbuffer(sockets.socket_memoria);
        uint32_t tamanio = buffer_read_uint32(buffer);
        void* datos_de_memoria = malloc(tamanio);
        buffer_read(buffer, datos_de_memoria, tamanio);


        buffer_destroy(buffer);
        return datos_de_memoria;
    }
    else{
        log_info(logger_errores_cpu, "Ocurrió un error tras realizar una lectura de Memoria. El código de operación recibido fue: %d", codigo);
        

        return NULL;
    }
}

void solicitar_escribir_en_memoria(uint32_t direccion_fisica, void* datos_de_registro, uint32_t tamanio){
    t_paquete* paquete = crear_paquete(ACCESS_ESPACIO_USUARIO_CPU, sizeof(uint32_t) * 4 + tamanio);
    
    buffer_add_uint32(paquete->buffer, PID);
    buffer_add_uint32(paquete->buffer, (uint32_t) 1 ); // 1 escribir, 0 leer
    buffer_add_uint32(paquete->buffer, direccion_fisica);
    buffer_add_uint32(paquete->buffer, tamanio);
    buffer_add(paquete->buffer, datos_de_registro, tamanio);
    
    enviar_paquete(paquete, sockets.socket_memoria);
}

int solicitar_marco_a_memoria(uint32_t numero_pagina){
    t_paquete* paquete = crear_paquete(ACCESO_TABLA_PAGINAS, sizeof(uint32_t) * 2);
    buffer_add_uint32(paquete->buffer, PID);
    buffer_add_uint32(paquete->buffer, numero_pagina);

    enviar_paquete(paquete, sockets.socket_memoria);
    
    int op_code = recibir_operacion(sockets.socket_memoria);
    
    if(op_code == MARCO_BUSCADO ){
        t_buffer* buffer_memoria = recibir_todo_elbuffer(sockets.socket_memoria);

        return buffer_read_uint32(buffer_memoria);
    }

    return -1;
}

/*
int enviar_log_D( int fd_conexion_ptr){
    int cod_op = recibir_operacion(fd_conexion_ptr);
    if(cod_op == COD_OP_FETCH ) log_info(logger_cpu, "PID: <PID> - FETCH - Program Counter: <PROGRAM_COUNTER>");
    else return EXIT_FAILURE;
    return 0;
}

int enviar_log_I( int fd_conexion_ptr){
    int cod_op = recibir_operacion(fd_conexion_ptr);
    if(cod_op == COD_OP_INSTRUCCION_EJECUTADA ) log_info(logger_cpu, "PID: <PID> - Ejecutando: <INSTRUCCION> - <PARAMETROS>");
    else return EXIT_FAILURE;
    return 0;
}

*/

/*
void procesos_cpu(){
    pthread_t hilo_memoria, hilo_kernel_dispatch, hilo_kernel_interrupcion;
pthread_create(&hilo_kernel_dispatch, NULL,(void*) escucha_KD, NULL); 
pthread_create(&hilo_kernel_interrupcion, NULL,(void *) escucha_KI, NULL);
pthread_create(&hilo_memoria, NULL,(void *) establecer_conexion_memoria, NULL);
pthread_join(hilo_kernel_interrupcion, NULL);
pthread_join(hilo_kernel_dispatch,NULL);
pthread_join(hilo_memoria,NULL);
}
*/
