#include <stdlib.h>
#include <stdio.h>
#include <main.h>

sem_t sem_apagar;

int main(int argc, char* argv[]) {

    pthread_t levantar_kernel, escuchar_conexiones, 
              establecer_conexion, planificador_corto_plazo, 
              planificador_largo_plazo, consola;
    iniciar_logger();
    obtener_config();
    iniciar_semaforos();
    iniciar_estructuras();
    inicializar_variables();

    pthread_create(&levantar_kernel,NULL,(void*)inicializar_kernel,NULL);
    pthread_create(&establecer_conexion,NULL,(void*)establecer_conexiones,NULL);
    pthread_create(&escuchar_conexiones,NULL,(void*)atender_escuchas,NULL);
    pthread_create(&planificador_corto_plazo,NULL,(void*)planificar_a_corto_plazo_segun_algoritmo,NULL);
    pthread_create(&planificador_largo_plazo,NULL,(void*)planificar_a_largo_plazo,NULL);
    pthread_create(&consola,NULL,(void*)ejecutar_consola_kernel,NULL);
    

    pthread_detach(levantar_kernel);
    pthread_detach(establecer_conexion);
    pthread_detach(escuchar_conexiones);
    pthread_detach(planificador_corto_plazo);
    pthread_detach(planificador_largo_plazo);
    pthread_detach(consola);

    sem_wait(&sem_apagar);
    destruir_colas();
    destruir_semaforos();
    dictionary_destroy(dicc_IO);
    dictionary_destroy(dicc_pcb);
    dictionary_destroy(recursos);
    sleep(1);
    destruir_config();
    sleep(1);
    return 0;
}

void iniciar_semaforos(){
    sem_init(&sem_escuchar, 0, 1); //inicializo escuchando
    sem_init(&sem_detener_desalojo,0,0);
    sem_init(&sem_apagar,0,0);
    sem_init(&sem_ejecucion, 0, 1);
    iniciar_semaforos_planificacion();
}

void iniciar_estructuras(){
    dicc_pcb = dictionary_create();
    dicc_IO = dictionary_create();
    crear_colas_bloqueo();
    iniciar_colas();
    
}

void inicializar_variables(){
    current_pid = 0;
    cont_salteo_signal = 0;
    pausar_plani = false;
    multiprogramacion_actual = configuracion.GRADO_MULTIPROGRAMACION;
    apagando_sistema = false;
}

void destruir_colas(){
    queue_clean(cola_new);
    queue_destroy(cola_new);
    queue_clean(cola_prioritaria_VRR);
    queue_destroy(cola_prioritaria_VRR);
    queue_clean(suspendido_bloqueado);
    queue_destroy(suspendido_bloqueado);
    queue_clean(cola_ready);
    queue_destroy(cola_ready);
    queue_clean(suspendido_listo);
    queue_destroy(suspendido_listo);
    list_clean(bloqueado);
    list_destroy(bloqueado);
}

void destruir_semaforos(){
    sem_destroy(&sem_escuchar);
    sem_destroy(&sem_detener_desalojo);
    sem_destroy(&sem_apagar);
    sem_destroy(&sem_ejecucion);
    sem_destroy(&sem_grado_multiprogramacion);
    sem_destroy(&proceso_ejecutando);
    sem_destroy(&hay_procesos_nuevos);
    sem_destroy(&sem_pausa_planificacion_largo_plazo);
    sem_destroy(&sem_pausa_planificacion_corto_plazo);
    sem_destroy(&sem_proceso_en_ready);
  
}
