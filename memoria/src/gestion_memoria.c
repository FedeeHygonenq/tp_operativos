#include "gestion_memoria.h"
#include <math.h>
#define min(a,b) (a<b?a:b)

t_dictionary* tabla_global;
void* espacio_usuario;
int cantFrames;
int* bitMap;

void crear_tabla_de_paginas(int pid){
    tabla_pagina* tabPagina = malloc(sizeof(tabla_pagina));
    tabPagina->pid = pid;
    tabPagina->paginas = list_create();
    int int_pid = pid; 
    dictionary_put(tabla_global,string_itoa(int_pid), tabPagina);
    log_info(logger_memoria, "PID %d -  tamanio %d", tabPagina->pid, list_size(tabPagina->paginas));
}

void ajustar_tam_proceso(t_buffer* buffer_cpu){
    uint32_t pid = buffer_read_uint32(buffer_cpu);
    int int_pid = pid;
    uint32_t tam_bytes = buffer_read_uint32(buffer_cpu);
    int cant_frames_requeridos = ceil((double)tam_bytes/configuracion.TAM_PAGINA); // funcion de <math.h>
    
    tabla_pagina* paginas_del_proceso = dictionary_get(tabla_global, string_itoa(int_pid));

    int tam_actual = list_size(paginas_del_proceso->paginas);
    

    if(tam_actual > cant_frames_requeridos) {
        log_info(logger_memoria, "PID %d - tamanio actual %d - tamanio a reducir %d",pid, tam_actual*configuracion.TAM_PAGINA , cant_frames_requeridos * configuracion.TAM_PAGINA );
        reduccion_del_proceso(paginas_del_proceso, cant_frames_requeridos); 
    }else
    {

       if( hay_espacio(tam_actual , cant_frames_requeridos)){

        if(tam_actual == 0 ) {
            asignar_id_pagina(paginas_del_proceso);
            cantFrames--;
        }
    
        }else{
            void* a_enviar = malloc(sizeof(op_code));
            op_code mot = NO_RESIZE;
            memcpy(a_enviar,&mot,sizeof(op_code));
            log_error(logger_memoria, "OUT OF MEMORY____");
            send(sockets.socket_cliente_CPU, a_enviar, sizeof(op_code), MSG_WAITALL);
            free(a_enviar);
        return ;
    
        }
    

        log_info(logger_memoria, "PID %d - tamanio actual %u - tamanio a ampliar %d",pid, tam_bytes, cant_frames_requeridos * configuracion.TAM_PAGINA);

        ampliacion_del_proceso(paginas_del_proceso, cant_frames_requeridos); 

    }
        op_code mot = RESIZE_SUCCESS;
        send(sockets.socket_cliente_CPU, &mot, sizeof(op_code), MSG_WAITALL);
    return;
}


bool hay_espacio(uint32_t espacioActualUsado, uint32_t cantFramesQueSeNecesitan){
    if(cantFramesDisponibles() >= (cantFramesQueSeNecesitan - espacioActualUsado)){
        return true;
    }
    return false;
}


int cantFramesDisponibles(){
    int contador =0 ;
    for(int i=0; i <= (cantFrames -1); i++){
        if(bitMap[i] == 0) contador++;
    }
    return contador;
}

void asignar_id_pagina(tabla_pagina* tabla_proceso ){
    t_pagina* primer_pagina = malloc(sizeof(t_pagina)); 
    primer_pagina->pid_pagina = 0;
    primer_pagina->numero_marco = marco_disponible();
   
    list_add(tabla_proceso->paginas, primer_pagina);

}

void reduccion_del_proceso(tabla_pagina* tabla_proceso, int marcos_final){    
    for(int i = list_size(tabla_proceso->paginas); i > marcos_final;i--){
    
        t_pagina* salida = list_remove(tabla_proceso->paginas, i-1);
        bitMap[salida->numero_marco] = 0;
        free(salida);
    }
}


void ampliacion_del_proceso(tabla_pagina* tabla_proceso, int marcos_final){ 
    for(int i = list_size(tabla_proceso->paginas); i<marcos_final; i++){
        t_pagina* entrada = malloc(sizeof(t_pagina));
        entrada->numero_marco = marco_disponible();
        list_add(tabla_proceso->paginas, entrada);
    }
}


int marco_disponible(){
    for(int i=0; i< cantFrames ; i++){
        if(bitMap[i] == 0) {

        bitMap[i]=1;
        return i;
    }
    }
    return -1; //out_of_memory
    }

void access_espacio_usuario(t_buffer* buffer, int cliente_socket) {
    uint32_t pid = buffer_read_uint32(buffer);
    uint32_t accion = buffer_read_uint32(buffer); // 1 escribir, 0 leer -> a lo clock 2.0
    uint32_t direc_fisica = buffer_read_uint32(buffer);
    uint32_t tamanio = buffer_read_uint32 (buffer); // tamanio a leer/escribir

    if(accion == 0 ){ 
    log_info(logger_memoria, "PID %d - accion leer - direc_fisica %d - tamanio %d ",pid, direc_fisica, tamanio );
    return leer_espacio_usuario(pid, direc_fisica, tamanio,cliente_socket);
    }
    if(accion == 1){
    log_info(logger_memoria, "PID %d - accion escribir - direc_fisica %d - tamanio %d ",pid, direc_fisica, tamanio );  
    return  escribir_espacio_usuario(pid, direc_fisica, tamanio, buffer,cliente_socket);
    }
    exit(EXIT_FAILURE);
}   

void leer_espacio_usuario(uint32_t pid,uint32_t direc_fisica, uint32_t tamanio, int cliente_socket){
    void* valor = malloc(tamanio);
    t_paquete* info_a_enviar = crear_paquete(RESPUESTA_LECTURA_MEMORIA, tamanio + sizeof(uint32_t));
    valor = leer_memoria (direc_fisica, tamanio);
    buffer_add_uint32(info_a_enviar->buffer, tamanio);
    buffer_add(info_a_enviar->buffer, valor, tamanio);
    enviar_paquete(info_a_enviar, cliente_socket);//lectura
    return;
}

void* leer_memoria(uint32_t dir_fisica, uint32_t tamanio){
	void* valor_leido = malloc(tamanio);
    sem_wait(&mutex_memoria);
    memcpy(valor_leido, espacio_usuario + dir_fisica, tamanio);
    sem_post(&mutex_memoria);
    return valor_leido;

}

void escribir_espacio_usuario(uint32_t pid,uint32_t direc_fisica,uint32_t tamanio,t_buffer* buffer, int cliente_socket){
    void* data_a_escribir = malloc(tamanio);
    buffer_read(buffer, data_a_escribir, tamanio);
    //tabla_pagina* tabla = dictionary_get(tabla_global, string_itoa(pid));
    
    escribir_memoria(direc_fisica,tamanio, data_a_escribir);
    op_code confirmacion_escritura = OK_ESCRITURA;
    free(data_a_escribir);
    send(cliente_socket, &confirmacion_escritura, sizeof(op_code), MSG_WAITALL);//escritura
    return ;
    }


  void escribir_memoria(uint32_t direc_fisica, uint32_t tamanio, void* data_a_escribir){
     sem_wait(&mutex_memoria);
     memcpy(espacio_usuario + direc_fisica , data_a_escribir , tamanio); 
     sem_post(&mutex_memoria);
     return; 
}  
  


t_paquete* buscar_marco_pagina (t_buffer* buffer_de_cpu){
    t_paquete* a_enviar = crear_paquete(MARCO_BUSCADO, sizeof(uint32_t));//op_code y numero marco
    uint32_t pid = buffer_read_uint32(buffer_de_cpu);
    int int_pid = pid;
    uint32_t numero_pagina = buffer_read_uint32(buffer_de_cpu);
    tabla_pagina* tabla = dictionary_get(tabla_global, string_itoa(int_pid));
    if(list_size(tabla->paginas) > numero_pagina ){
    t_pagina* pagina = list_get(tabla->paginas, numero_pagina);
    buffer_add_uint32(a_enviar->buffer, pagina->numero_marco);
    log_info(logger_memoria, "PID %d - Pagina %d - Marco %d", pid , numero_pagina, pagina->numero_marco);
    }else exit(EXIT_FAILURE);
    
    return a_enviar;
}

t_paquete* enviar_tam_memoria(){
    t_paquete* paquete = crear_paquete(TAMANIO_PAGINA, sizeof(uint32_t));
    buffer_add_uint32 (paquete->buffer, configuracion.TAM_PAGINA);
    return paquete;
}



/*bool hay_lugar_contiguo(t_list* tabla,t_pagina* pag_a_leer,uint32_t offSet,uint32_t tamanio){
    int paginas_a_escribir_extras = (tamanio - offSet) / configuracion.TAM_MEMORIA;
    int indice_pagina = 0;
    while(paginas_a_escribir_extras >= 0){
        indice_pagina++;
        t_pagina* pagina = list_get(tabla, pag_a_leer->pid_pagina+ indice_pagina );
        if(pagina->escrita)return false;
    }
return true;
}*/


/*
void escribir_memoria(t_list* paginas , t_pagina* pag_a_leer,uint32_t offSet,uint32_t tamanio, void* data_a_escribir){
   int index_pagina = 0;
   while(tamanio > 0 ){
    int tam_a_escribir = pag_a_leer->tam_disponible- offSet;
    memcpy(espacio_usuario+ offSet+ pag_a_leer->numero_marco* configuracion.TAM_MEMORIA, data_a_escribir, tam_a_escribir);
    pag_a_leer->escrita = true;
    
    pag_a_leer->tam_disponible = min(0, pag_a_leer->tam_disponible - tamanio);
    tamanio -= tam_a_escribir; 
    offSet =0;
    index_pagina++;
    pag_a_leer = list_get(paginas, pag_a_leer->pid_pagina +index_pagina);
   } 
}



}
*/

