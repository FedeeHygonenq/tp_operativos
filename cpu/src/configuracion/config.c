#include "config.h"

t_cfg configuracion;
t_config* config;
t_log* logger_cpu;
t_log* logger_errores_cpu;
t_log* logger_conexiones;
t_log* logger_tlb;

void obtener_config(){
    config = config_create("cfg/cpu.config");    
    configuracion.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
    configuracion.PUERTO_MEMORIA = config_get_int_value(config,"PUERTO_MEMORIA");
    configuracion.PUERTO_ESCUCHA_DISPATCH = config_get_int_value(config,"PUERTO_ESCUCHA_DISPATCH");
    configuracion.PUERTO_ESCUCHA_INTERRUPT = config_get_int_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    configuracion.CANTIDAD_ENTRADAS_TLB = config_get_int_value(config,"CANTIDAD_ENTRADAS_TLB");
    configuracion.ALGORITMO_TLB = config_get_string_value(config,"ALGORITMO_TLB");
    
}

void iniciar_logger(){
    logger_cpu = log_create("logs/cpu.log","cpu",0,LOG_LEVEL_INFO);
    logger_errores_cpu = log_create("logs/cpu_errores.log","errores_cpu",0,LOG_LEVEL_INFO);
    logger_conexiones = log_create("logs/conexiones.log","Conexion",0,LOG_LEVEL_INFO);
    logger_tlb = log_create("logs/tlb.log","TLB",0,LOG_LEVEL_INFO);
}

void destruir_config(){
    log_destroy(logger_cpu);
    log_destroy(logger_errores_cpu);
    log_destroy(logger_conexiones);
    log_destroy(logger_tlb);
    config_destroy(config);
}