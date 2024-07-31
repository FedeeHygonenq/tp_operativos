#ifndef SERIALIZACION_H_
#define SERIALIZACION_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "estructuras.h"




// Crea un buffer vacío de tamaño size y offset 0
t_buffer *buffer_create(uint32_t size);

// Libera la memoria asociada al buffer
void buffer_destroy(t_buffer *buffer);

// Agrega un stream al buffer en la posición actual y avanza el offset
void buffer_add(t_buffer *buffer, void *data, uint32_t size);

// Guarda size bytes del principio del buffer en la dirección data y avanza el offset
void buffer_read(t_buffer *buffer, void *data, uint32_t size);

// Agrega un uint32_t al buffer
void buffer_add_uint32(t_buffer *buffer, uint32_t data);

// Lee un uint32_t del buffer y avanza el offset
uint32_t buffer_read_uint32(t_buffer *buffer);

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_buffer *buffer, uint8_t data);

// Lee un uint8_t del buffer y avanza el offset
uint8_t buffer_read_uint8(t_buffer *buffer);

// Agrega string al buffer con un uint32_t adelante indicando su longitud
void buffer_add_string(t_buffer *buffer, uint32_t length, char *string);

// Lee un string y su longitud del buffer y avanza el offset
char *buffer_read_string(t_buffer *buffer, uint32_t *lengwhath);

void desempaquetar_pcb(t_buffer*,t_pcb*);

void* a_enviar_create(t_paquete*);

t_paquete* crear_paquete(op_code, uint32_t);

void eliminar_paquete(t_paquete*);

void enviar_paquete(t_paquete* paquete, int socket_cliente);
#endif /* UTILS_H_ */
