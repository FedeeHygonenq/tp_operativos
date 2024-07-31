
#ifndef CONEXION_H_
#define CONEXION_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include "serializacion.h"

typedef enum {
    CODIGO,
    OK,
    FALLO
}cod_handshake;


int iniciar_servidor(char*);
int esperar_cliente(int,t_log*,char*);
int crear_conexion(char *, char* ,t_log*,char*);
void liberar_conexion(int);
int enviar_handshake(int,char*);
void crear_buffer(t_paquete* paquete);
t_buffer* recibir_todo_elbuffer(int);
op_code recibir_operacion(int);
motivo_desalojo recibir_desalojo(int);



#endif
