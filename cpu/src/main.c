#include <stdlib.h>
#include <stdio.h>
#include <main.h>

int main(int argc, char* argv[]) {
    pthread_t levantar_server, conexion, escuchar_conexiones;
    iniciar_logger();
    obtener_config();
    inicializar_estructuras();

    pthread_create(&levantar_server, NULL,(void*)iniciar_server_kernel, NULL);
    pthread_create(&conexion, NULL, (void*)establecer_conexion_memoria, NULL);
    pthread_create(&escuchar_conexiones, NULL, (void*)atender_conexiones, NULL);

    pthread_detach(levantar_server);
    pthread_detach(conexion);
    pthread_detach(escuchar_conexiones);

    sem_wait(&sem_apagar_cpu);
    log_info(logger_cpu, "Ooaa ooaa apago C, P, U~");
    destruir_estructuras();
    sleep(1);
    destruir_config();
    sleep(1);
    return 0;
}