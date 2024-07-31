#ifndef _KERNEL_CONFIG_
#define _KERNEL_CONFIG_

#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <commons/log.h>
#include <stdint.h>
#include <stdlib.h>
#include "utils/estructuras.h"


typedef struct {
    char* IP_MEMORIA;
    char* IP_CPU;
    uint16_t PUERTO_MEMORIA;
    uint16_t PUERTO_ESCUCHA;
    uint16_t PUERTO_CPU_DISPATCH;
    uint16_t PUERTO_CPU_INTERRUPT;
    char* ALGORITMO_PLANIFICACION;
    int QUANTUM;
    t_list* RECURSOS;
    t_list* INSTANCIAS_RECURSOS;
    uint8_t GRADO_MULTIPROGRAMACION;

}t_cfg;

extern t_cfg configuracion;

extern t_config* config;

void obtener_config();

extern t_log* logger_kernel;
extern t_log* logger_conexiones;
extern t_log* logger_recurso_ES;
extern t_log* logger_ingresos_ready;
extern t_log* logger_error;


void iniciar_logger();
void destruir_config();

#endif