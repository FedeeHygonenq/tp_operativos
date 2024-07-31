#ifndef MANEJOES_H_
#define MANEJOES_H_

#include "kernel_utils.h"
#include <poll.h>
#include <commons/collections/queue.h>

typedef struct{
    char* nombre;
    t_interfaz tipo_interfaz;
    sem_t esta_libre;
    sem_t pidieron_interfaz;
    int socket;
    t_pcb* proceso_okupa;
    t_queue* cola;
    bool inactiva;
}dispositivo_IO;

typedef struct 
{
    t_pcb* proceso;
    t_paquete* paquete;
}proceso_en_cola;


extern t_dictionary* dicc_IO;

dispositivo_IO* recibir_info_io(int, t_buffer*);
dispositivo_IO* crear_dispositivo_IO(int, t_interfaz, char*);
void procesar_peticion_IO(char*,t_instruccion*,uint32_t, t_buffer*);
void destruir_dispositivo_IO(char*);
void gestionar_interfaces();
void monitor_desconexion(dispositivo_IO*);

#endif