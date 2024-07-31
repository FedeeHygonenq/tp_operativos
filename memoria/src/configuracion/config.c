#include "config.h"

t_cfg configuracion;
t_config* config;
t_log* logger_memoria;
t_log* logger_conexiones;

void obtener_config(){
    config = config_create("cfg/memoria.config");
    configuracion.PUERTO_ESCUCHA = config_get_int_value(config,"PUERTO_ESCUCHA");
    configuracion.TAM_MEMORIA = config_get_int_value(config,"TAM_MEMORIA");
    configuracion.TAM_PAGINA = config_get_int_value(config,"TAM_PAGINA");
    configuracion.PATH_INSTRUCCIONES = config_get_string_value(config,"PATH_INSTRUCCIONES");
    configuracion.RETARDO_RESPUESTA = config_get_int_value(config,"RETARDO_RESPUESTA");

}

void iniciar_logger(){
    logger_memoria = log_create("logs/memoria.log","Memoria",0,LOG_LEVEL_INFO);
    logger_conexiones = log_create("logs/conexiones.log","Conexion",0,LOG_LEVEL_INFO);
}

void destruir_config(){
    log_destroy(logger_conexiones);
    log_destroy(logger_memoria);
    config_destroy(config);

}