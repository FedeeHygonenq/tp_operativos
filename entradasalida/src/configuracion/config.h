#ifndef _ENTRADASALIDA_CONFIG_
#define _ENTRADASALIDA_CONFIG_

#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <stdint.h>
#include <string.h>
#include "utils/conexion.h"
#include "utils/serializacion.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "E_S_utils.h"


typedef struct {
    t_interfaz TIPO_INTERFAZ;
    uint16_t TIEMPO_UNIDAD_TRABAJO;
    char* IP_KERNEL;
    uint16_t PUERTO_KERNEL;
    char* IP_MEMORIA;
    uint16_t PUERTO_MEMORIA;
    char* PATH_BASE_DIALFS;
    uint8_t BLOCK_SIZE;
    uint16_t BLOCK_COUNT;
    uint32_t RETRASO_COMPACTACION;
}t_cfg;

typedef struct {
    uint32_t indice_de_bloques;
    char nombre_archivo[PATH_MAX];
} t_indice;

extern t_cfg configuracion;
extern t_config* config;
extern t_list* config_list;
extern t_log* logger_entrada_salida;
extern t_log* logger_conexiones;
extern t_log* logger_salida;
extern t_log* logger_fs;
extern t_log* logger_errores;
extern t_bitarray* bitmap;
extern int tamanio_bitmap_bytes;
extern int tamanio_memoria_bloques;
extern int fd_bitmap;
extern void* bitmap_memoria;
extern int fd_bloques;
extern int fd_indice;
extern void ** bloques;
extern char** indice;
extern char* path_indice;



void iniciar_logger();
void obtener_config(char*);
t_bitarray* crear_bitmap(char*, uint32_t);
void* crear_bloques(char*, uint32_t , uint32_t);
t_bitarray* mapear_archivo_bitmap (int, int );
void* mapear_archivo_bloques(int , uint32_t , uint32_t);
int crear_indice(char*, uint32_t);
void levantar_fs(char*, uint8_t, uint16_t);
void inicializar_fs(char *, char*, char*, uint8_t, uint16_t );
void destruir_config();

#endif