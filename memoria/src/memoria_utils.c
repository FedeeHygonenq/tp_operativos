#include <memoria_utils.h>

str_sockets sockets;
sem_t sem_escuchar;
t_list* listaDeProcesos;
char* path_kernel;
struct paquetePcb kernelPcb;
uint32_t pid_buscar_proceso;
sem_t mutex_memoria;



void inicializar_memoria(){
    int puerto = configuracion.PUERTO_ESCUCHA;
    sockets.socket_server = iniciar_servidor(string_itoa(puerto));
    log_info(logger_conexiones, "Memoria esta escuchando");
    espacio_usuario = malloc(configuracion.TAM_MEMORIA);
    cantFrames = configuracion.TAM_MEMORIA/ configuracion.TAM_PAGINA;
    bitMap = malloc(cantFrames * sizeof(int));
    memset(bitMap, 0 , sizeof(int) * cantFrames);
    tabla_global = dictionary_create();
    listaDeProcesos = list_create();
    sem_init(&mutex_memoria, 0,1);
} 

void iniciar_proceso(t_buffer* bufferDeKernel){
    procesoListaInst* procesos = malloc(sizeof(procesoListaInst));
// ------------ cargar buffer kernel -------//
    procesos->pid = buffer_read_uint32(bufferDeKernel);
    uint32_t sizeDelProceso = 0;
// ------------ leo archivo instrucciones ---------//    
    procesos->instruccionesParaCpu = leer_instrucciones_del_path(buffer_read_string(bufferDeKernel,&sizeDelProceso));
    agregarProcesoALaCola(procesos);
    crear_tabla_de_paginas(procesos->pid);
    
   }

void atender_escuchas(){
    while(1){
    sleep(1);
    sem_wait(&sem_escuchar);
    pthread_t escuchar;
    pthread_create(&escuchar,NULL,(void*)server_escuchar,NULL);
    pthread_join(escuchar,NULL);
    }
}

int server_escuchar() {
    char* nom_cliente = malloc(20);
    int cliente_socket = esperar_cliente(sockets.socket_server,logger_conexiones,nom_cliente);
    sem_post(&sem_escuchar);
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

void procesar_conexion(void* void_args) {
    t_procesar_conexion_args* args = (t_procesar_conexion_args*) void_args;
    int cliente_socket = args->fd;
    char* nombre_cliente = args->cliente_name;
    free(args);
    void* a_enviar;//espacio_usuario
    op_code cop;
    if(strcmp(nombre_cliente,"CPU")==0){
        sockets.socket_cliente_CPU = cliente_socket;
    }
    else if(strcmp(nombre_cliente,"KERNEL")==0){
        sockets.socket_cliente_kernel = cliente_socket;
    }
    while (cliente_socket != -1) {

    t_buffer* buffer_de_cpu;
    t_buffer* buffer_de_kernel;
    t_buffer* buffer_ENTRADA_SALIDA;
            if (recv(cliente_socket, &cop, sizeof(op_code), 0) != sizeof(op_code)) {
            log_info(logger_conexiones, "%s DISCONNECT!", nombre_cliente);
            free(nombre_cliente);
            return;
        }
        usleep(configuracion.RETARDO_RESPUESTA *1000);
        switch (cop)
        {
        case SOLICITUD_INSTRUCCION:           buffer_de_cpu = recibir_todo_elbuffer(cliente_socket);
            enviar_instrucciones_cpu(buffer_de_cpu,cliente_socket);
            //log_info(logger_memoria, "Inicio envio de instrucciones");
            buffer_destroy(buffer_de_cpu);
            break;
        case INICIAR_PROCESO:
            buffer_de_kernel = recibir_todo_elbuffer(cliente_socket);
            iniciar_proceso(buffer_de_kernel);
            // log_info(logger_memoria, "Inicio proceso");
            op_code cod = PROCESO_CREADO;
            send(cliente_socket, &cod, sizeof(op_code), 0);
            buffer_destroy( buffer_de_kernel);
            break;
        case AJUSTAR_TAMANIO:
            buffer_de_cpu = recibir_todo_elbuffer(cliente_socket);
            ajustar_tam_proceso(buffer_de_cpu);
           // log_info(logger_memoria, "Ajusto tamanio proceso");
            buffer_destroy(buffer_de_cpu);
            break;

        case FINALIZAR_PROCESO:
             buffer_de_kernel = recibir_todo_elbuffer(cliente_socket);
             finalizar_proceso(buffer_de_kernel);
             log_info(logger_memoria, "Finalizo el proceso");
            buffer_destroy( buffer_de_kernel);
            break;
        case ACCESS_ESPACIO_USUARIO_ES:
            buffer_ENTRADA_SALIDA = recibir_todo_elbuffer(cliente_socket);
            access_espacio_usuario(buffer_ENTRADA_SALIDA,cliente_socket);
            
     //       if(a_enviar == NULL)  send(cliente_socket, &confirmacion_escritura, sizeof(op_code), MSG_WAITALL);//lectura
       //     else{enviar_paquete((t_paquete*)a_enviar, cliente_socket); //escritura podria ser cliente socket /././././././././././
         //   }
            buffer_destroy( buffer_ENTRADA_SALIDA);
            
            break;
        
        case ACCESS_ESPACIO_USUARIO_CPU:
            buffer_de_cpu = recibir_todo_elbuffer(cliente_socket);
            access_espacio_usuario(buffer_de_cpu,cliente_socket);
            //confirmacion_escritura = OK_ESCRITURA;
            //if(a_enviar == NULL) send(cliente_socket, &confirmacion_escritura, sizeof(op_code), MSG_WAITALL);//escritura
            //else{
            //    
            //}
            buffer_destroy( buffer_de_cpu);
            
            break;

        case ACCESO_TABLA_PAGINAS://cpu
            buffer_de_cpu = recibir_todo_elbuffer(cliente_socket);
            a_enviar = buscar_marco_pagina (buffer_de_cpu);
            enviar_paquete((t_paquete*) a_enviar, cliente_socket);
            buffer_destroy( buffer_de_cpu);
            break;
        case SOLICITUD_TAMANIO_PAGINA: //a cpu
            enviar_paquete(enviar_tam_memoria(), cliente_socket);
            break;
        case APAGAR: //a cpu
            sem_post(&sem_apagar);
            return;
            break;
        default:
            break;
        }
    }
    
    free(nombre_cliente);
}





// checkpoint 2, capaz de leer las instrucciones y enviarlas al cpu

// leer las instrucciones del path
t_list* leer_instrucciones_del_path(char* rutaKernel) {
   

    FILE* archivo_instrucciones = fopen(rutaKernel, "rb");
   
    if (archivo_instrucciones == NULL) {
        perror("No pude abrir el archivo");
        log_error(logger_memoria,"No se pudo abrir el archivo de instrucciones");
        exit(EXIT_FAILURE);
    }
    char aux [256];
    t_list* instruccionesParaCPu = list_create();
    while (fgets(aux, sizeof(aux), archivo_instrucciones)) {
        // Eliminar el salto de línea si está presente
        size_t len = strlen(aux);
        if (len > 0 && aux[len - 1] == '\n') {
            aux[len - 1] = '\0';  // Reemplaza el '\n' por '\0'
        }

        // Asignar memoria suficiente para la cadena más el carácter nulo
        char* a_guardar = malloc(len + 1);
        if (a_guardar == NULL) {
            perror("No me aloque (malloc)");
            fclose(archivo_instrucciones);
            exit(EXIT_FAILURE);
        }

        // Copiar la cadena
        strcpy(a_guardar, aux);

        // Añadir a la lista
        list_add(instruccionesParaCPu, a_guardar);
    }

    fclose(archivo_instrucciones);
   
    return instruccionesParaCPu;   
}



void agregarProcesoALaCola(procesoListaInst* proceso){
    list_add(listaDeProcesos, proceso);
}





procesoListaInst* buscar_procesoPorId(uint32_t pid){
    uint32_t pid_buscar_proceso = pid;
    bool pidIguales(procesoListaInst* proceso){
        return proceso->pid == pid_buscar_proceso;
    }
    return list_find(listaDeProcesos, (void*) pidIguales);       
}

/*
procesoListaInst* buscar_procesoPorId(uint32_t pid){
    bool pidIguales(void* proceso,int pid){
   return ((procesoListaInst*)proceso)->pid == pid;
}
    return (procesoListaInst*)list_find(listaDeProcesos, (void*) pidIguales);       
}
*/

void finalizar_proceso(t_buffer* buffer_kernel){
    
    uint32_t pid = buffer_read_uint32(buffer_kernel);
    uint32_t pid_buscar_proceso = pid;
    int pid_int = pid;
    tabla_pagina* paginas_del_proceso = dictionary_get(tabla_global, string_itoa(pid_int));
    uint32_t tam_tab_pagina = list_size(paginas_del_proceso->paginas);

    reduccion_del_proceso(paginas_del_proceso, 0); // marco bits en 0 y saco paginas de la lista

    tabla_pagina* tab_pagina = dictionary_remove(tabla_global, string_itoa(pid_int));// saco del proceso.
    free(tab_pagina);

    log_info(logger_memoria, "PID %d -  tamanio %d", pid, tam_tab_pagina);


bool pidIguales(procesoListaInst* proceso){
   return proceso->pid == pid_buscar_proceso;
}
	procesoListaInst* procesos =  list_remove_by_condition(listaDeProcesos, (void*) pidIguales);

    list_destroy_and_destroy_elements(procesos->instruccionesParaCpu, free);
    free(procesos);
}

char* instruccionActual (procesoListaInst* proceso, uint32_t ip ){

    if(ip<0 || list_size(proceso->instruccionesParaCpu) < ip ){
        exit(EXIT_FAILURE);
        return NULL;
    }

    return list_get(proceso->instruccionesParaCpu,(int)ip); 
}

















//  int atender_cliente(int* fd_conexion_ptr){
      // recibo cod_op
//         int cod_op  = recibir_operacion(*fd_conexion_ptr);
//         return enviar_log(*fd_conexion_ptr, cod_op);     
// }

// int enviar_log(int fd_conexion_ptr, int cod_op){
//     switch (cod_op) {
        
//         case COD_OP_CREACION_TABLA:
// Log de creacion de tabla de paginas y destruccion de pagina
           
//         case COD_OP_DESTRUCCION_TABLA:
//enviar_mensaje("PID: <PID> - Tamanio: <CANTIDAD_PAGINAS>", fd_conexion_ptr);
//                     log_info(logger_memoria, "PID: <PID> - Tamaño: <CANTIDAD_PAGINAS>");
//             break;
        
//         case COD_OP_ACCESO_TABLA:
        // Log de acceso a tabla de paginas
         //enviar_mensaje("PID: <PID> - Pagina: <PAGINA> - Marco: <MARCO>",fd_conexion_ptr);
        //log_info(logger_memoria, "PID: <PID> - Pagina: <PAGINA> - Marco: <MARCO>");
        //             break;
//         case COD_OP_AMPLIACION_PROCESO:
        // Log de ampliacion de proceso
        //  enviar_mensaje("PID: <PID> - Tamanio Actual: <TAMANIO_ACTUAL> - Tamanio a Ampliar: <TAMANIO_A_AMPLIAR>" ,fd_conexion_ptr);
        //  log_info(logger_memoria, "PID: <PID> - Tamaño Actual: <TAMAÑO_ACTUAL> - Tamaño a Ampliar: <TAMAÑO_A_AMPLIAR>");
                    
//             break;
//         case COD_OP_REDUCION_PROCESO:
            // Log de reducciOn de proceso
            // enviar_mensaje("PID: <PID> - Tamanio Actual: <TAMANIO_ACTUAL> - Tamanio a Reducir: <TAMANIO_A_REDUCIR>",fd_conexion_ptr);
            //log_info(logger_memoria, "PID: <PID> - Tamaño Actual: <TAMAÑO_ACTUAL> - Tamaño a Reducir: <TAMAÑO_A_REDUCIR>");
            //             break;
            //         case COD_OP_ACCESO_ESPACIO_USUARIO:
           // Log de acceso a espacio de usuario
            // enviar_mensaje("PID: <PID> - Accion: <LEER / ESCRIBIR> - Direccion fisica: <DIRECCION_FISICA>", fd_conexion_ptr);
            // log_info(logger_memoria, "PID: <PID> - Accion: <LEER / ESCRIBIR> - Direccion fisica: <DIRECCION_FISICA>");
            //break;
        
            //         case -1:
            //         	log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
            // 			return EXIT_FAILURE; 

            //         default:
            //             // Log para codigo de operacion desconocido
            //                  //   enviar_mensaje("codigo desconocido", fd_conexion_ptr);
            //                     log_info(logger_memoria, "codigo desconocido");
            //             break;
            //     }
            //         return EXIT_SUCCESS;
            // }

/*
void escuchar_instrucciones(){

    pthread_t thread_kernel;
    pthread_t thread_cpu;
    pthread_t thread_ES;
    pthread_create(&thread_kernel,
                    NULL,
                    (void*)escucha_kernel,
                    NULL);
                    

    
    pthread_create(&thread_cpu,
                    NULL,
                    (void*) escucha_cpu,
                    NULL);



    pthread_create(&thread_ES,
                    NULL,
                    (void*) escucha_E_S,
                    NULL);
                    
    pthread_join(thread_cpu,NULL);
    pthread_join(thread_ES,NULL);
    pthread_join(thread_kernel,NULL);

    return;

}
*/