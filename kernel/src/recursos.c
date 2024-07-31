#include "recursos.h"

int cantidad_recursos;
t_dictionary* recursos;
void crear_colas_bloqueo(){

    recursos = dictionary_create();
    for(int i = 0; i< list_size(configuracion.RECURSOS);i++)
    {
    str_recursos* recurso = malloc(sizeof(str_recursos));
    recurso->cola = queue_create();
    recurso->procesos_okupas = list_create();
    int cant = atoi(list_get(configuracion.INSTANCIAS_RECURSOS, i));
    sem_init(&(recurso->cantidad_recursos), 0, cant);
    sem_init(&(recurso->recurso_solicitado),0,0);
    recurso->nombre = list_get(configuracion.RECURSOS, i);
    dictionary_put(recursos, recurso->nombre, recurso);
    pthread_t hilo;
    pthread_create(&hilo, NULL, (void*) gestionar_recurso, (void*) recurso);
    pthread_detach(hilo);
    }

}

void eliminar_recurso(char* nom_recurso){
    str_recursos* recurso = dictionary_remove(recursos, nom_recurso);
    queue_clean(recurso->cola);
    queue_destroy(recurso->cola);
    free(recurso->nombre);
    list_clean(recurso->procesos_okupas);
    list_destroy(recurso->procesos_okupas);
    sem_destroy(&(recurso->cantidad_recursos));
    sem_destroy(&(recurso->recurso_solicitado));
    free(recurso);
}

void gestionar_recurso(str_recursos* recurso) {
    while (1) {
        sem_wait(&(recurso->recurso_solicitado));
        sem_wait(&(recurso->cantidad_recursos));
        if(apagando_sistema){
            return;
        }
        if (queue_size(recurso->cola) > 0) {
            t_pcb* pcb = queue_pop(recurso->cola);
            list_add(recurso->procesos_okupas, &pcb->pid);
            list_remove_element(bloqueado, pcb);
            pcb->estado = READY;
            log_info(logger_recurso_ES, "El proceso %u tomo el recurso %s", pcb->pid, recurso->nombre);

            if (strcmp(configuracion.ALGORITMO_PLANIFICACION, "VRR") == 0 && pcb->quantum < configuracion.QUANTUM) {
                pcb->estado = READY;
                queue_push(cola_prioritaria_VRR,pcb);
                sem_post(&sem_proceso_en_ready);
                mensaje_ingreso_ready = string_new();
                list_iterate(cola_prioritaria_VRR->elements,agregar_PID_ready);
                log_info(logger_ingresos_ready,"Proceso %u ingreso a READY - Ready Prioridad: %s",pcb->pid, mensaje_ingreso_ready);
                free(mensaje_ingreso_ready);
            } else {
                queue_push(cola_ready,pcb);
                pcb->estado = READY;
                sem_post(&sem_proceso_en_ready);
                mensaje_ingreso_ready = string_new();
                list_iterate(cola_ready->elements,agregar_PID_ready);
                log_info(logger_ingresos_ready,"Proceso %u ingreso a READY - Cola Ready: %s",pcb->pid, mensaje_ingreso_ready);
                free(mensaje_ingreso_ready);
            }

            log_info(logger_kernel, "PID: %u - Estado Anterior: BLOQUEADO - Estado Actual: READY", pcb->pid);
        }
        else{
            sem_post(&(recurso->cantidad_recursos));
        }
    }
}



