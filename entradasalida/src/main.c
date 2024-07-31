#include <stdlib.h>
#include <stdio.h>
#include "main.h"

extern char* nombre_interfaz;
sem_t sem_apagar;
int main(int argc, char* argv[]) {
    
    sem_init(&mutex_conexion,0,1);
    sem_init(&sem_apagar,0,0);
    int contador_argumentos=1;
    iniciar_logger();
    config_list = list_create();
    while (contador_argumentos < argc) {
        sem_wait(&mutex_conexion);
        char* path_config = argv[contador_argumentos];
        contador_argumentos++;
        nombre_interfaz = argv[contador_argumentos];
        contador_argumentos++;
        pthread_t conexion;
        obtener_config(path_config);
        pthread_create(&conexion,NULL,(void*)establecer_conexiones,NULL);
        pthread_detach(conexion);
    }
    
    sem_wait(&sem_apagar);
    sem_destroy(&sem_apagar);
    sem_destroy(&mutex_conexion);
    sleep(1);
    destruir_config();
    sleep(1);
    return 0;
}
