#ifndef CPU_UTILS_H
#define CPU_UTILS_H

#include "utils/conexion.h"
#include "utils/serializacion.h"
#include <configuracion/config.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <semaphore.h>
#include "ciclo_de_instruccion.h"
#include "utils/estructuras.h"
#include "tlb.h"

typedef struct{
    int socket_server_I;
    int socket_server_D;
    int socket_memoria;
    int socket_kernel_I;
    int socket_kernel_D;
}str_sockets;

extern int ind_contexto_kernel;
extern int llego_interrupcion;
extern uint32_t tamanio_pagina;
extern str_sockets sockets;
extern sem_t sem_inter;
extern sem_t sem_contexto;
extern sem_t sem_apagar_cpu;

void iniciar_server_kernel();
void inicializar_cpu_dispatch();
void inicializar_cpu_interrupt();
void establecer_conexion_memoria();
void atender_conexiones();
void procesar_conexion(void*);
int server_escuchar(int);
void recibir_contexto_ejecucion(int);
void recibir_interrupcion_de_kernel();
void inicializar_estructuras();
void destruir_estructuras();
void enviar_contexto_a_kernel(motivo_desalojo);
void envios_de_std_a_kernel(t_instruccion, char*, uint32_t, t_buffer*);
void enviar_std_a_kernel(t_instruccion, char* ,int ,int[] , int[]);
void solicitar_create_delete_fs_a_kernel(t_instruccion, char*, char*);
void solicitar_truncate_fs_a_kernel(t_instruccion, char*, char*, uint32_t);
void solicitudes_fs_a_kernel(t_instruccion, char*, char*, t_buffer*, uint32_t, uint32_t);
void solicitar_write_read_fs_a_kernel(t_instruccion, char*, char*, uint32_t, int[], int[], uint32_t);
void solicitar_tamanio_pagina();
void solicitar_instruccion_a_memoria();
int solicitar_marco_a_memoria(uint32_t);
void recibir_instruccion_de_memoria();
void recibir_respuesta_resize_memoria(uint32_t);
uint32_t cantidad_paginas_que_ocupa(uint32_t, uint32_t);
void* leer_en_memoria_mas_de_una_pagina(t_buffer*, uint32_t, uint32_t);
void escribir_en_memoria_mas_de_una_pagina(t_buffer*, uint32_t);
void* solicitar_leer_en_memoria(uint32_t, uint32_t);
void* leer_de_memoria();
void solicitar_escribir_en_memoria(uint32_t, void*, uint32_t);

#endif