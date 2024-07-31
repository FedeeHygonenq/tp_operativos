#ifndef MEMORIA_UTILS_H
#define MEMORIA_UTILS_H

#include <utils/conexion.h>
#include "configuracion/config.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <semaphore.h>
#include <utils/estructuras.h>
#include <gestion_memoria.h>
#define maxiumLineLength 256


typedef struct {
    int socket_server;
    int socket_cliente_CPU;
    int socket_cliente_kernel;
    int socket_cliente_E_S;
}str_sockets;

extern str_sockets sockets;
extern sem_t sem_escuchar;
extern sem_t mutex_memoria;
extern sem_t sem_apagar;
extern char rutaKernel[256];
extern int socket_server;
void inicializar_memoria();
void atender_escuchas();
void escuchar_instrucciones();
int server_escuchar();
void procesar_conexion(void*);
int enviar_log(int, int);
void finalizar_proceso(t_buffer*);
t_list* leer_instrucciones_del_path();
typedef struct {
    uint32_t pid;
    t_list* instruccionesParaCpu;
}procesoListaInst;

struct paquetePcb{
    int pid;
};

extern char* path_kernel;
extern struct paquetePcb kernelPcb;
extern t_list* listaDeProcesos;

void enviar_instrucciones_cpu(t_buffer*, int);
void tam_pagina(t_buffer*);
char* instruccionActual (procesoListaInst*, uint32_t );
void enviar_instruccion(char*, int);
void ajustar_tam_proceso( t_buffer*);
void agregarProcesoALaCola(procesoListaInst*);
procesoListaInst* buscar_procesoPorId(uint32_t);

#endif