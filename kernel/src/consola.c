#include "consola.h"


char* mensaje_listado;

char* construir_ruta(const char* path_relativo) {
    size_t len_base = strlen(BASE_DIR);
    size_t len_relativo = strlen(path_relativo);
    char* path_completo = malloc(len_base + len_relativo + 1); // +1 para el null terminator
    if (path_completo == NULL) {
        perror("No se pudo asignar memoria para el path completo");
        exit(EXIT_FAILURE);
    }
    strcpy(path_completo, BASE_DIR);
    strcat(path_completo, path_relativo);
    return path_completo;
}

void ejecutar_comandos(char* input){
    int tipo_comando = get_tipo_comando(input);
        switch (tipo_comando) {
            case EJECUTAR_SCRIPT:{
                ejecutar_script(input + 16); 
            }
                break; 
            case INICIAR_PROCESO_CONSOLA:{
                iniciar_proceso(input + 16);
            }
                break;
            case FINALIZAR_PROCESO_CONSOLA:
                finalizar_proceso(input + 18);
                break;
            case DETENER_PLANIFICACION:
                detener_planificacion();
                break;
            case INICIAR_PLANIFICACION:
                iniciar_planificacion();
                break;
            case MULTIPROGRAMACION:
                modificar_multiprogramacion(atoi(input + 18));
                break;
            case PROCESO_ESTADO:
                listar_procesos_por_estado();
                break;
            case APAGAR_SISTEMA:
                apagar_sistema();
                break;
            default:
                printf("Comando no reconocido: %s\n", input);
                break;
        }

}

void ejecutar_consola_kernel() {
    char *input;
    while ((input = readline("Kernel> ")) != NULL) {
        if (strlen(input) > 0) {
            add_history(input);
        }
        ejecutar_comandos(input);

        free(input);
    }
}

void ejecutar_script(const char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("No se pudo abrir el archivo de script");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline character
        line[strcspn(line, "\n")] = '\0';
        printf("Ejecutando comando: %s\n", line);
        // Add your command execution logic here
        ejecutar_comandos(line);
    }
    fclose(file);
}

void iniciar_proceso(const char *path) {
    crear_proceso(path);
}

void finalizar_proceso(char* pid) {
    unsigned long int aux = strtoul(pid, NULL, 10);
    if(pcb_en_ejecucion->pid == (uint32_t) aux){
        t_paquete* paquete = crear_paquete (INTERRUPT_PROC,sizeof(tipo_de_interrupcion));
        tipo_de_interrupcion inter = DESALOJO_POR_USUARIO;
        buffer_add(paquete->buffer,&inter,sizeof(tipo_de_interrupcion));
        enviar_paquete(paquete,sockets.socket_CPU_I);
    }
    else{
        liberar_proceso((uint32_t)aux);
        log_info(logger_kernel,"Finaliza el proceso %lu - Motivo: INTERRUPTED_BY_USER",aux);
    }
}

void detener_planificacion() {
    pausar_planificacion();   
}


void iniciar_planificacion() {
    sem_post(&sem_pausa_planificacion_largo_plazo);
    sem_post(&sem_pausa_planificacion_corto_plazo);
    sem_post(&sem_detener_desalojo);
}

void modificar_multiprogramacion(int valor) {
    int aux = multiprogramacion_actual;
    multiprogramacion_actual = valor;
    if(valor > aux){
        for(int i = 0; i < (valor - aux);i++){
            sem_post(&sem_grado_multiprogramacion);
        }
        cont_salteo_signal = 0;
    }
    else if(valor < aux){
        cont_salteo_signal += aux - valor;
    }
    log_info(logger_kernel,"EL nuevo grado multiprogramacion es %i", valor);
}



// Helper function to get command type
int get_tipo_comando(const char *input) {
    if (strncmp(input, "EJECUTAR_SCRIPT ", 16) == 0) {
        return EJECUTAR_SCRIPT;
    } else if (strncmp(input, "INICIAR_PROCESO ", 16) == 0) {
        return INICIAR_PROCESO_CONSOLA;
    } else if (strncmp(input, "FINALIZAR_PROCESO ", 18) == 0) {
        return FINALIZAR_PROCESO_CONSOLA;
    } else if (strcmp(input, "DETENER_PLANIFICACION") == 0) {
        return DETENER_PLANIFICACION;
    } else if (strcmp(input, "INICIAR_PLANIFICACION") == 0) {
        return INICIAR_PLANIFICACION;
    } else if (strncmp(input, "MULTIPROGRAMACION ", 18) == 0) {
        return MULTIPROGRAMACION;
    } else if (strcmp(input, "PROCESO_ESTADO") == 0) {
        return PROCESO_ESTADO;
    } else if (strcmp(input, "APAGAR") == 0) {
        return APAGAR_SISTEMA;
    } else {
        return COMANDO_DESCONOCIDO;
    }
}

void listar_procesos_por_estado(){
    listar_proceso(cola_new->elements,"NEW");
    listar_proceso(cola_ready->elements,"READY");
    listar_proceso(bloqueado,"BLOQUEADO");
    log_info(logger_kernel, "El siguientes proceso estan en la cola de EXEC: %d", pcb_en_ejecucion->pid);
}

void listar_proceso(t_list* lista, char* estado){
    mensaje_listado = string_new();
    list_iterate(lista, agregar_PID);
    log_info(logger_kernel,"Los siguientes proceso estan en la cola %s: %s", estado, mensaje_listado);
    free(mensaje_listado);
    
}

void apagar_sistema(){
    apagando_sistema = true;
    op_code cod = APAGAR;
    send(sockets.socket_memoria, &cod, sizeof(op_code), 0);
    send(sockets.socket_CPU_I, &cod, sizeof(op_code), 0);
    eliminar_interfaces();
    eliminar_pcbs();
    eliminar_recursos();
    sem_post(&sem_apagar);
}

void eliminar_interfaces(){
    op_code cod = APAGAR;
    t_list* keys = dictionary_keys(dicc_IO);
    for(int i = 0; i < list_size(keys); i++){
       dispositivo_IO* interfaz =  dictionary_get(dicc_IO,list_get(keys,i));
       send(interfaz->socket,&cod, sizeof(op_code),0);
       destruir_dispositivo_IO(interfaz->nombre);
    }
    free(keys);
}

void eliminar_pcbs(){
    t_list* keys = dictionary_keys(dicc_pcb);
    for(int i = 0; i < list_size(keys); i++){
        eliminar_pcb(list_get(keys,i));
    }
    free(keys);
}

void eliminar_recursos(){
    t_list* keys = dictionary_keys(recursos);
    for(int i = 0; i < list_size(keys); i++){
        eliminar_recurso(list_get(keys,i));
    }
    free(keys);
}

