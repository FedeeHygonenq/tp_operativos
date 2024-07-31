#ifndef RECURSOS_H
#define RECURSOS_H

#include "kernel_utils.h"
#include "planificacion.h"
#include <commons/collections/dictionary.h>

typedef struct{
    t_queue* cola;
    sem_t cantidad_recursos;
    sem_t recurso_solicitado;
    t_list* procesos_okupas;
    char* nombre;
}str_recursos;

extern int cantidad_recursos;
extern t_dictionary* recursos;

void crear_colas_bloqueo();
void eliminar_recuros(char*);
void gestionar_recurso(str_recursos*);
void eliminar_recurso(char*);
#endif