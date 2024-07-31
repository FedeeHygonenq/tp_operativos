#include <planificacion.h>

sem_t sem_grado_multiprogramacion;
sem_t proceso_ejecutando;
sem_t hay_procesos_nuevos;
sem_t sem_pausa_planificacion_largo_plazo;
sem_t sem_pausa_planificacion_corto_plazo;
sem_t sem_proceso_en_ready;
pthread_t temporizador_quantum;
bool pausar_plani;
char* mensaje_ingreso_ready;
int cont_salteo_signal;
int multiprogramacion_actual;



//colas de estado
t_queue *cola_new;
t_queue *cola_ready;
t_queue *cola_prioritaria_VRR;
t_list *bloqueado;
t_queue *suspendido_bloqueado;
t_queue *suspendido_listo; 
t_temporal* temp_quantum;



void iniciar_semaforos_planificacion(){
    int grado_multiprogramacion = configuracion.GRADO_MULTIPROGRAMACION;
    sem_init(&hay_procesos_nuevos, 0, 0);
    sem_init(&sem_grado_multiprogramacion, 0, grado_multiprogramacion);
    sem_init(&proceso_ejecutando, 0, 1);
    sem_init(&sem_pausa_planificacion_corto_plazo,0,0);
    sem_init(&sem_pausa_planificacion_largo_plazo,0,0);
    sem_init(&sem_proceso_en_ready,0,0);
}

void iniciar_colas(){
    cola_new = queue_create(); // cambio
    cola_ready = queue_create();
    cola_prioritaria_VRR = queue_create();
    bloqueado = list_create();
    suspendido_bloqueado = queue_create();
    suspendido_listo = queue_create();
    //lista_ejecutando = list_create();
}


void liberar_proceso(uint32_t pid){
    t_paquete* paquete = crear_paquete(FINALIZAR_PROCESO,sizeof(uint32_t));
    buffer_add_uint32(paquete->buffer,pid);
    enviar_paquete(paquete,sockets.socket_memoria);
    eliminar_pcb(string_from_format("%u", pid));
/*
    if(cont_salteo_signal > 0){
        cont_salteo_signal--;
        if(cont_salteo_signal == 0){
            sem_post(&sem_pausar_ready);
            pausar_ready = false;
        }
    }
    else{
    sem_post(&sem_grado_multiprogramacion);
    }
    */
    sem_post(&sem_grado_multiprogramacion);
}

void crear_proceso(char* path){
    t_pcb* pcb = crear_pcb(path);
    queue_push(cola_new,pcb);
    uint32_t tam_string = string_length(pcb->pathOperaciones)+1;
    t_paquete* paquete = crear_paquete(INICIAR_PROCESO,sizeof(uint32_t)*2+tam_string);
    buffer_add_uint32(paquete->buffer,pcb->pid);
    buffer_add_string(paquete->buffer,tam_string,pcb->pathOperaciones);
    enviar_paquete(paquete,sockets.socket_memoria);
    op_code cod;
    recv(sockets.socket_memoria,&cod,sizeof(op_code), MSG_WAITALL);
    if(cod == PROCESO_CREADO){
        log_info(logger_kernel,"Se crea el proceso %u en NEW",pcb->pid); 
        sem_post(&hay_procesos_nuevos);
    }
    else{
       log_info(logger_error,"No se recibio correctamente la confirmacion para la creacion del proceso en memoria correctamente");
    }
    
}

void planificar_a_largo_plazo(){
    while(1){
        sem_wait(&hay_procesos_nuevos);
        sem_wait(&sem_grado_multiprogramacion);
        if(apagando_sistema){
            return;
        }
        if(pausar_plani){
            sem_wait(&sem_pausa_planificacion_largo_plazo);
            reanudar_planificacion();
        }
        if(cont_salteo_signal>0){
            sem_post(&hay_procesos_nuevos);
            cont_salteo_signal--;
            continue;
        }
        t_pcb* proceso_para_ready = queue_pop(cola_new);
        proceso_para_ready->estado = READY;
        queue_push(cola_ready, proceso_para_ready);
        mensaje_ingreso_ready = string_new();
        list_iterate(cola_ready->elements,agregar_PID_ready);
        log_info(logger_ingresos_ready,"Proceso %u ingreso a READY - Cola Ready: %s",proceso_para_ready->pid, mensaje_ingreso_ready);
        sem_post(&sem_proceso_en_ready);
        free(mensaje_ingreso_ready);
    }

}

void planificar_a_corto_plazo_segun_algoritmo(){
    char *algoritmo_planificador = configuracion.ALGORITMO_PLANIFICACION;
    while(1){
        sem_wait(&sem_proceso_en_ready);
        if(apagando_sistema){
            return;
        }
        if(pausar_plani){
                sem_wait(&sem_pausa_planificacion_corto_plazo);
                reanudar_planificacion();
            }

        if(strcmp(algoritmo_planificador,"FIFO") == 0){ 

            t_pcb *a_ejecutar = proximo_ejecutar_FIFO();
            ejecutar_FIFO(a_ejecutar);
            }

        else if (strcmp(algoritmo_planificador, "RR") == 0){

            t_pcb* a_ejecutar = proximo_ejecutar_RR();
            ejecutar_RR(a_ejecutar);
            }

        else if (strcmp(algoritmo_planificador, "VRR") == 0){
            t_pcb* a_ejecutar = proximo_ejecutar_VRR();
            ejecutar_VRR(a_ejecutar);
            }

        esperar_confirmacion_contexto();
    }
    free(algoritmo_planificador);
}

void esperar_confirmacion_contexto(){
    op_code cop = recibir_operacion(sockets.socket_CPU_D);
    if(cop != CONTEXTO_RECIBIDO){
        log_info(logger_error,"CPU no recibio correctamente el contexto o cruzo mensanjes");
    }
        cop = recibir_operacion(sockets.socket_CPU_D);
        if(cop == CONTEXTO_EXEC){
        recibir_contexto_exec(pcb_en_ejecucion);
    }
}

t_pcb* proximo_ejecutar_FIFO(){
    return (t_pcb*)queue_pop(cola_ready);
}

t_pcb* proximo_ejecutar_RR(){
    return (t_pcb*)queue_pop(cola_ready);
}

t_pcb* proximo_ejecutar_VRR(){
    if(queue_is_empty(cola_prioritaria_VRR)){
        return (t_pcb*)queue_pop(cola_ready);
    }
    return (t_pcb*)queue_pop(cola_prioritaria_VRR);
}

void ejecutar_FIFO(t_pcb *a_ejecutar){

    sem_wait(&proceso_ejecutando);
    a_ejecutar->estado = EXEC;
    crear_paquete_contexto_exec(a_ejecutar);
    log_info(logger_kernel, "PID: %u - Estado Anterior: READY - Estado Actual: EXEC", a_ejecutar->pid);


}

void ejecutar_RR(t_pcb *a_ejecutar){

    sem_wait(&proceso_ejecutando);
    a_ejecutar->estado = EXEC;
    a_ejecutar->quantum = configuracion.QUANTUM;
    //creo y envio el contexto de ejecucion
    ejecutar_con_quantum(a_ejecutar);
    log_info(logger_kernel, "PID: %u - Estado Anterior: READY - Estado Actual: EXEC", a_ejecutar->pid);
    

}

void ejecutar_VRR(t_pcb *a_ejecutar){

    sem_wait(&proceso_ejecutando);
    a_ejecutar->estado = EXEC;
    //creo y envio el contexto de ejecucion
    ejecutar_con_quantum(a_ejecutar);
    log_info(logger_kernel, "PID: %u - Estado Anterior: READY - Estado Actual: EXEC", a_ejecutar->pid);

}

void ejecutar_con_quantum(t_pcb *a_ejecutar){
    crear_paquete_contexto_exec(a_ejecutar);
    temp_quantum = temporal_create();
    pthread_create(&temporizador_quantum, NULL, (void*)esperar_interrupcion_quantum, (void*)a_ejecutar);
    pthread_detach(temporizador_quantum);
    //falta terminar
    
}

void esperar_interrupcion_quantum(t_pcb *a_ejecutar){
    a_ejecutar->ticket++;
    int ticket_referencia = a_ejecutar->ticket;
    usleep(a_ejecutar->quantum * 1000);
    if(ticket_referencia == a_ejecutar->ticket && a_ejecutar->estado == EXEC){
        a_ejecutar->quantum = configuracion.QUANTUM;
        t_paquete* paquete = crear_paquete (INTERRUPT_PROC,sizeof(tipo_de_interrupcion));
        tipo_de_interrupcion inter = DESALOJO_QUANTUM;
        buffer_add(paquete->buffer,&inter,sizeof(tipo_de_interrupcion));
        enviar_paquete(paquete,sockets.socket_CPU_I);
    }

}

void pausar_planificacion() {
    pausar_plani = true;
    log_info(logger_kernel, "Planificación pausada");
}

void reanudar_planificacion() {
    pausar_plani = false;
    log_info(logger_kernel, "Planificación reanudada");
}



/*
//Acá se envía el contexto de ejecución al CPU a través de dispatch
    crear_paquete_pcb(*proceso);
//Escuchar por el contexto actualizado luego de la ejecución
*/

