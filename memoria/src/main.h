#ifndef MAIN_H_
#define MAIN_H_

#include<stdio.h>
#include<stdlib.h>
#include<readline/readline.h>
#include<readline/history.h>
#include<memoria_utils.h>
#include<configuracion/config.h>

void destruir_estructuras();
void eliminar_proceso_lista(procesoListaInst*);
void eliminar_tabla_global();
void destruir_semaforos();
#endif