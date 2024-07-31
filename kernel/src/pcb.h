#ifndef PCB_H
#define PCB_H

#include <commons/collections/dictionary.h>
#include <commons/string.h>
#include <stdint.h>
#include <stdlib.h>
#include <configuracion/config.h>
#include <utils/conexion.h>
#include <utils/serializacion.h>
#include "kernel_utils.h"
#include "planificacion.h"
#include "recursos.h"
#include "manejoES.h"



extern t_dictionary* dicc_pcb;
extern int current_pid;
extern registros_CPU* registros;
extern t_pcb* pcb_en_ejecucion;
extern sem_t sem_detener_desalojo;
extern sem_t sem_ejecucion;

t_pcb* crear_pcb(char*);
void eliminar_pcb(char*);
registros_CPU* crear_registros();
int siguiente_PID();
t_buffer* crear_buffer_pcb();
void crear_paquete_pcb(t_pcb*);
void crear_paquete_contexto_exec(t_pcb*);
void recibir_contexto_exec(t_pcb*);

#endif