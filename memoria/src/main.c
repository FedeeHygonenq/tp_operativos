#include <stdlib.h>
#include <stdio.h>
#include <main.h>

sem_t sem_apagar;

int main(int argc, char* argv[]) {
    pthread_t escuchar;
    sem_init(&sem_escuchar, 0, 1);
    sem_init(&sem_apagar,0,0);
    iniciar_logger();
    obtener_config();
    inicializar_memoria();

    t_list* listaDeProcesos = list_create();

    //pthread_create(&levantar_server,NULL,(void*)inicializar_memoria,NULL);
    pthread_create(&escuchar,NULL,(void*)atender_escuchas,NULL);
    
    //pthread_join(levantar_server,NULL);
    pthread_detach(escuchar);
    
    sem_wait(&sem_apagar);
    sleep(1);
    destruir_semaforos();
    list_destroy(listaDeProcesos);
    destruir_estructuras();
    destruir_config();
    sleep(1);

    return 0;
}

void destruir_estructuras(){
    free(bitMap);
    list_destroy_and_destroy_elements(listaDeProcesos,(void*)eliminar_proceso_lista);
    eliminar_tabla_global();
}

void eliminar_tabla_global(){
    t_list* keys = dictionary_keys(tabla_global);
    for(int i = 0; i < list_size(keys); i++){
        tabla_pagina* a_eliminar = dictionary_remove(tabla_global, list_get(keys,i));
        list_destroy_and_destroy_elements(a_eliminar->paginas,(void*) free);
        free(a_eliminar);
    }
    dictionary_destroy(tabla_global);
}

void eliminar_proceso_lista(procesoListaInst* proceso){
    list_destroy_and_destroy_elements(proceso->instruccionesParaCpu,(void*) free);
    free(proceso);
}

void destruir_semaforos(){
    sem_destroy(&sem_escuchar);
    sem_destroy(&sem_apagar);
    sem_destroy(&mutex_memoria);
}