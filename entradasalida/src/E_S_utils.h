#ifndef E_S_UTILS_H
#define E_S_UTILS_H

#include <utils/conexion.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/temporal.h>
#include <unistd.h>
#include <configuracion/config.h>
#include <pthread.h>
#include "utils/conexion.h"
#include "utils/serializacion.h"
#include <semaphore.h>
#include <readline/readline.h>
#include <readline/history.h>
typedef struct {
    int socket_memoria;
    int socket_kernel;
}str_sockets;

extern str_sockets sockets;
extern sem_t mutex_conexion;
extern sem_t sem_apagar;

//void proceso_E_S();
void establecer_conexiones();
void establecer_conexion_memoria(int*);
void establecer_conexion_kernel(int*);
//void iniciar_interfaz();


void enviar_info_io_a_kernel(int);

void procesar_io_gen_sleep (t_buffer*, int, char*);
void procesar_io_stdin_read(t_buffer* , uint32_t,int,int, char*);
void procesar_io_stdout_write(t_buffer* , uint32_t,int, int, char* );
void procesar_io_fs_create(t_buffer*, uint32_t, int, char*);
void procesar_io_fs_delete(t_buffer*, uint32_t,int, char*);
void procesar_io_fs_truncate(t_buffer* , uint32_t,int, char*);
void procesar_io_fs_write(t_buffer*, uint32_t , int , int,char*);
void procesar_io_fs_read(t_buffer* ,uint32_t,int,int, char*);

char* leer_consola();
void escribir_en_memoria(void* ,uint32_t ,uint32_t ,uint32_t,  int) ;
void recibir_instrucciones (int,int);
char* string_de_instruccion (t_instruccion);
int contar_bloques_libres(t_bitarray*);
int primer_bloque_libre(t_bitarray*);
void limpiar_bits(t_bitarray* , off_t , off_t);
bool hay_bits_ocupados(t_bitarray* , int , int );
void enviar_fin_de_instruccion(int, char*);
void agregar_archivo_a_indice(char*, int);
int obtener_nuevo_bloque_final (char *, uint32_t);
void eliminar_archivo_de_indice(char*);
int compactar_y_acomodar_al_final(void **, t_bitarray *, int, int, uint32_t);
void modificar_bloque_inicial_indice (int, int);
int obtener_ultimo_bloque_de_archivo (char *);
void escribir_en_fs (int , uint32_t , uint32_t , void* );
int buscar_archivo_en_indice(int , char* );
int modificar_bloque_inicial_indice_por_nombre (char* , int );

#endif