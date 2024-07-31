#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H

#include <pthread.h>
#include "configuracion/config.h"
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include "utils/conexion.h"
#include "utils/serializacion.h"
#include <semaphore.h>
#include "pcb.h"
#include "consola.h"
#include "manejoES.h"
#include "recursos.h"



typedef struct {

    int socket_server;
    int socket_CPU_I;
    int socket_CPU_D;
    int socket_memoria;
    int socket_cliente_E_S;
}str_sockets;


extern str_sockets sockets;
extern sem_t sem_escuchar;
extern sem_t sem_apagar;
extern pthread_t conexion_CPU_I,conexion_CPU_D, conexion_memoria;
extern bool apagando_sistema;

void establecer_conexion_cpu_D();

void establecer_conexion_memoria();

void establecer_conexion_cpu_I();

void inicializar_kernel();
void atender_escuchas();
void procesar_conexion_CPU_D(int);
void establecer_conexiones();
int server_escuchar();
void procesar_conexion(void*);
void agregar_PID(void*);
void agregar_PID_ready(void*);
#endif