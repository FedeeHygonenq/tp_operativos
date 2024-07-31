#ifndef _MEMORIA_CONFIG_
#define _MEMORIA_CONFIG_

#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdint.h>
#include<stdlib.h>
#include "utils/estructuras.h"

typedef struct {
    uint16_t PUERTO_ESCUCHA;
    uint16_t TAM_MEMORIA;
    uint16_t TAM_PAGINA;
    char* PATH_INSTRUCCIONES;
    uint16_t RETARDO_RESPUESTA;

}t_cfg;

extern t_cfg configuracion;

extern t_config* config;

void obtener_config();

extern t_log* logger_memoria;
extern t_log* logger_conexiones;


void iniciar_logger();
void destruir_config();

#endif