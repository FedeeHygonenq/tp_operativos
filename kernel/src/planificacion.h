#ifndef PLANIFICACION_H
#define PLANIFICACION_H

#include <commons/string.h>
#include <stdint.h>
#include <stdlib.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/temporal.h>
#include "kernel_utils.h"
#include "pcb.h"
#include <pthread.h>

extern sem_t sem_grado_multiprogramacion;
extern sem_t proceso_ejecutando;
extern sem_t hay_procesos_nuevos;
extern sem_t sem_pausa_planificacion_largo_plazo;
extern sem_t sem_pausa_planificacion_corto_plazo;
extern sem_t sem_proceso_en_ready;
extern bool pausar_plani;
extern char* mensaje_ingreso_ready;
extern t_queue *cola_new;
extern t_queue *cola_ready;
extern t_queue *cola_prioritaria_VRR;
extern t_list *bloqueado;
extern t_queue *suspendido_bloqueado;
extern t_queue *suspendido_listo; 
extern t_temporal* temp_quantum;
extern int cont_salteo_signal;
extern int multiprogramacion_actual;

void iniciar_colas();
void iniciar_semaforos_planificacion();
void liberar_proceso(uint32_t);
void crear_proceso(char*);
void planificar_a_largo_plazo();
void planificar_a_corto_plazo_segun_algoritmo();
t_pcb* proximo_ejecutar_FIFO();
t_pcb* proximo_ejecutar_RR();
t_pcb* proximo_ejecutar_VRR();
void ejecutar_con_quantum(t_pcb *);
void ejecutar_FIFO(t_pcb *);
void ejecutar_RR(t_pcb *);
void ejecutar_VRR(t_pcb *);
void esperar_interrupcion_quantum(t_pcb *);
void pausar_planificacion();
void reanudar_planificacion();
void esperar_confirmacion_contexto();


#endif



