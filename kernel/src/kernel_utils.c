#include "kernel_utils.h"

str_sockets sockets;
sem_t sem_escuchar;
bool apagando_sistema;


pthread_t conexion_CPU_I,conexion_CPU_D, conexion_memoria;

void establecer_conexion_memoria()
{
    int fd_memoria = crear_conexion(configuracion.IP_MEMORIA, string_itoa(configuracion.PUERTO_MEMORIA),logger_conexiones,"KERNEL");
    log_info(logger_conexiones, "Conectado Kernel memoria");
    sockets.socket_memoria = fd_memoria;
}

void atender_escuchas(){
    while(1){
    sem_wait(&sem_escuchar);
    if(apagando_sistema){
        return;
    }
    pthread_t escuchar;
    pthread_create(&escuchar,NULL,(void*)server_escuchar,NULL);
    pthread_join(escuchar,NULL);
    }
}

void establecer_conexion_cpu_D()
{
    int fd_cpu_distpach = crear_conexion(configuracion.IP_CPU, string_itoa(configuracion.PUERTO_CPU_DISPATCH),logger_conexiones,"KERNEL_D");
    log_info(logger_conexiones, "Conectado Kernel Cpu_Dispatch");
    sockets.socket_CPU_D = fd_cpu_distpach;
}

void establecer_conexion_cpu_I()
{
    int fd_cpu_interrupt = crear_conexion(configuracion.IP_CPU, string_itoa(configuracion.PUERTO_CPU_INTERRUPT),logger_conexiones,"KERNEL_I");
    log_info(logger_conexiones, "Concetado Kernel CPU-interrupt ");
    sockets.socket_CPU_I = fd_cpu_interrupt;
   
}

void inicializar_kernel(){
    sockets.socket_server = iniciar_servidor( string_itoa(configuracion.PUERTO_ESCUCHA));
    //socket_server queda en listen() 
    log_info(logger_conexiones, "Kernel esta escuchando");
} 

int server_escuchar() {
    char* nom_cliente = malloc(20);

    int cliente_socket = esperar_cliente(sockets.socket_server,logger_conexiones,nom_cliente); //conecta, valida con handshake y devuelve socket
    //log_info(logger_conexiones, "Conectado 1");
    sem_post(&sem_escuchar);
    //log_info(logger_conexiones, "No bloqueado");
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
    dispositivo_IO* interfaz;
    op_code cop;
    while (cliente_socket != -1) {

        if (recv(cliente_socket, &cop, sizeof(op_code), 0) != sizeof(op_code)) {
            log_info(logger_conexiones, "%s DISCONNECT!", nombre_cliente);
            if(strcmp("ENTRADASALIDA",nombre_cliente)==0){
                destruir_dispositivo_IO(interfaz->nombre);
            }
            free(nombre_cliente);
            return;
        }

        switch (cop){

        case ENTRADASALIDA:{
            t_buffer* buffer = recibir_todo_elbuffer(cliente_socket);
            interfaz = recibir_info_io(cliente_socket,buffer);
            break;
        }

        case ENTRADASALIDA_LIBERADO:{
            t_buffer* buffer = recibir_todo_elbuffer(cliente_socket);
            uint32_t len = 0;
            char* io = buffer_read_string(buffer,&len);
            dispositivo_IO* interfaz = dictionary_get(dicc_IO, io);
            list_remove_element(bloqueado, interfaz->proceso_okupa);
            if(strcmp(configuracion.ALGORITMO_PLANIFICACION,"VRR")== 0 && interfaz->proceso_okupa->quantum < configuracion.QUANTUM){
                interfaz->proceso_okupa->estado = READY;
                queue_push(cola_prioritaria_VRR,interfaz->proceso_okupa);
                mensaje_ingreso_ready = string_new();
                list_iterate(cola_prioritaria_VRR->elements,agregar_PID_ready);
                log_info(logger_ingresos_ready,"Proceso %u ingreso a READY - Ready Prioridad con quantum %u: %s",interfaz->proceso_okupa->pid, interfaz->proceso_okupa->quantum,  mensaje_ingreso_ready);
                sem_post(&sem_proceso_en_ready);
                free(mensaje_ingreso_ready);
            }
            else{
                interfaz->proceso_okupa->estado = READY;
                queue_push(cola_ready,interfaz->proceso_okupa);
                mensaje_ingreso_ready = string_new();
                list_iterate(cola_ready->elements,agregar_PID_ready);
                log_info(logger_ingresos_ready,"Proceso %u ingreso a READY - Cola Ready: %s", interfaz->proceso_okupa->pid, mensaje_ingreso_ready);         
                sem_post(&sem_proceso_en_ready);
                free(mensaje_ingreso_ready);
            }

            log_info(logger_kernel, "PID: %u - Estado Anterior: BLOQUEADO - Estado Actual: READY", interfaz->proceso_okupa->pid);
            interfaz->proceso_okupa = NULL;
            sem_post(&interfaz->esta_libre);
            free(io);
            buffer_destroy(buffer);

            break;
        }

        default:
            log_info(logger_conexiones,"no estas mandando nada");
            break;
        }
    }
    if(apagando_sistema){
        sem_post(&sem_escuchar);
    }
    free(nombre_cliente);
}

void establecer_conexiones(){
    sleep(1);
    pthread_t cpu_D, cpu_I, memoria;
    pthread_create(&cpu_D,NULL, (void*) establecer_conexion_cpu_D, NULL);
    pthread_create(&cpu_I,NULL, (void*) establecer_conexion_cpu_I, NULL);
    pthread_create(&memoria,NULL, (void*) establecer_conexion_memoria, NULL);
    pthread_detach(cpu_D);
    pthread_detach(cpu_I);
    pthread_detach(memoria);
}


void agregar_PID(void* value){
    t_pcb* pcb = (t_pcb*)value;
    string_append_with_format(&mensaje_listado, "PID: %u ", pcb->pid);
}

void agregar_PID_ready(void* value){
    t_pcb* pcb = (t_pcb*)value;
    string_append_with_format(&mensaje_ingreso_ready, "PID: %u ", pcb->pid);
}