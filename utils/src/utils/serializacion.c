#include "serializacion.h"

// Crea un buffer vacío de tamaño size y offset 0.
t_buffer *buffer_create(uint32_t size) {
    t_buffer* buffer = malloc(sizeof(t_buffer));
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        exit(EXIT_FAILURE);
    }

    buffer->size = size;
    buffer->offset = 0;
    buffer->stream = malloc(size);
    if (buffer->stream == NULL) {
        perror("Error allocating memory for buffer stream");
        free(buffer);
        exit(EXIT_FAILURE);
    }
    
    memset(buffer->stream, 0, size); // Inicializa la memoria asignada a stream

    return buffer;
}

// Libera la memoria asociada al buffer.
void buffer_destroy(t_buffer *buffer){
	free(buffer->stream);
	free(buffer);
}

// Agrega un stream al buffer en la posición actual y avanza el offset.
void buffer_add(t_buffer *buffer, void *data, uint32_t size){
    // Verifica si hay suficiente espacio en el buffer
    if (buffer->offset + size > buffer->size) {
        // Maneja el error de forma adecuada (puedes hacer un assert, retornar un error, etc.)
        fprintf(stderr, "Error: buffer overflow detected in buffer_add\n");
        exit(EXIT_FAILURE);
    }

    // Copia los datos al buffer
    memcpy(buffer->stream + buffer->offset, data, size);
    
    // Avanza el offset
    buffer->offset += size;
}

// Guarda size bytes del principio del buffer en la dirección data y avanza el offset.
void buffer_read(t_buffer *buffer, void *data, uint32_t size){
    // Verifica si hay suficiente espacio en el buffer
    if (buffer->offset + size > buffer->size) {
        // Maneja el error de forma adecuada (puedes hacer un assert, retornar un error, etc.)
        fprintf(stderr, "Error: buffer overflow detected in buffer_read\n");
        exit(EXIT_FAILURE);
    }

    // Copia los datos del buffer
    memcpy(data, buffer->stream + buffer->offset, size);
    
    // Avanza el offset
    buffer->offset += size;
}
// Agrega un uint32_t al buffer
void buffer_add_uint32(t_buffer *buffer, uint32_t data){
	buffer_add(buffer, &data, sizeof(uint32_t));
}

// Lee un uint32_t del buffer y avanza el offset
uint32_t buffer_read_uint32(t_buffer *buffer){
	uint32_t data = -1;

	buffer_read(buffer, &data, sizeof(uint32_t));

	return data;
}

// Agrega un uint8_t al buffer
void buffer_add_uint8(t_buffer *buffer, uint8_t data){
	buffer_add(buffer, &data, sizeof(uint8_t));
}

// Lee un uint8_t del buffer y avanza el offset
uint8_t buffer_read_uint8(t_buffer *buffer){
	uint8_t data = 0;

	buffer_read(buffer, &data, sizeof(uint8_t));

	return data;
}

// Agrega string al buffer con un uint32_t adelante indicando su longitud
void buffer_add_string(t_buffer *buffer, uint32_t length, char *string){
	buffer_add_uint32(buffer, length);
	buffer_add(buffer, string, length);
}

// Lee un string y su longitud del buffer y avanza el offset
char *buffer_read_string(t_buffer *buffer, uint32_t *length){
	*length = buffer_read_uint32(buffer);
	char *data = malloc((*length)*sizeof(char));
	buffer_read(buffer, data, *length);

	return data;
}

void* a_enviar_create(t_paquete* paquete){
	void* a_enviar = malloc(paquete->buffer->size + sizeof(uint32_t) + sizeof(op_code));
	int offset = 0;
	memcpy(a_enviar + offset, &(paquete->codigo_operacion), sizeof(op_code));
	offset += sizeof(op_code);
	memcpy(a_enviar + offset, &(paquete->buffer->size), sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(a_enviar + offset, paquete->buffer->stream, paquete->buffer->size);
	return a_enviar;
}

t_paquete* crear_paquete(op_code code, uint32_t size_buff){
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = code;
	paquete->buffer = buffer_create(size_buff);
	return paquete;
}

void eliminar_paquete(t_paquete* paquete){
    free(paquete->buffer->stream);
    free(paquete->buffer);
    free(paquete);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente){
    
    void* a_enviar = a_enviar_create(paquete);
    send(socket_cliente, a_enviar, paquete->buffer->size + sizeof(uint32_t) + sizeof(op_code), 0);
	eliminar_paquete(paquete);
    free(a_enviar);
}
