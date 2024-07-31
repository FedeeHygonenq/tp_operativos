#include "manejoES.h"

t_dictionary* dicc_IO;

dispositivo_IO* recibir_info_io(int cliente_socket, t_buffer* buffer){
    //log_info(logger_conexiones,"me mandaste la info de entradasalida");
    /*serializo*/
    t_interfaz tipo_interfaz;
    buffer_read(buffer,&tipo_interfaz,sizeof(t_interfaz));
    uint32_t long_nombre;
    char* nombre_interfaz = buffer_read_string(buffer,&long_nombre);
    char* tipo_inter[]= {
        "Generica", "STDIN", "STDOUT", "DIALFS"
    };
    //loggeo
    log_info(logger_conexiones,"Tipo de interfaz recibida %s", tipo_inter[tipo_interfaz]);
    log_info(logger_conexiones,"Nombre de interfaz recibida %s", nombre_interfaz);
    //Inicializo la interfaz
    dispositivo_IO* interfaz = crear_dispositivo_IO(cliente_socket, tipo_interfaz, nombre_interfaz);
    pthread_t hilo;
    pthread_create(&hilo, NULL, (void*) gestionar_interfaces, (void*)interfaz);
    pthread_detach(hilo);
    //libero estructuras
    //free(nombre_interfaz);
    buffer_destroy(buffer);
    return interfaz;
    
}

dispositivo_IO* crear_dispositivo_IO(int cliente_socket, t_interfaz tipo_interfaz, char* nombre){
    dispositivo_IO* interfaz = malloc(sizeof(dispositivo_IO));
    interfaz->nombre = nombre;
    interfaz->socket = cliente_socket;
    interfaz->tipo_interfaz = tipo_interfaz;
    sem_init(&interfaz->esta_libre,0,1);
    sem_init(&interfaz->pidieron_interfaz,0,0);
    interfaz->proceso_okupa = NULL;
    interfaz->cola = queue_create();
    interfaz->inactiva = false;
    dictionary_put(dicc_IO,nombre, interfaz);
    return interfaz;
}

void destruir_dispositivo_IO(char* nombre_interfaz){
    dispositivo_IO* interfaz = dictionary_remove(dicc_IO, nombre_interfaz);
    interfaz->inactiva = true;
    sem_post(&(interfaz->esta_libre));
    sem_post(&(interfaz->pidieron_interfaz));
    usleep(200);
    for(int i = 0; i < queue_size(interfaz->cola);i++){
        proceso_en_cola* proceso = queue_pop(interfaz->cola);
        eliminar_paquete(proceso->paquete);
        log_info(logger_kernel,"Finaliza el proceso %u - Motivo: REMOVING_INTERFACE", proceso->proceso->pid);
        log_info(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", proceso->proceso->pid);
        if(proceso->proceso != NULL){
            liberar_proceso(proceso->proceso->pid);
        }
        free(proceso);
    }
    free(interfaz->nombre);
    queue_destroy(interfaz->cola);
    sem_destroy(&(interfaz->esta_libre));
    sem_destroy(&(interfaz->pidieron_interfaz));
    if(interfaz->proceso_okupa != NULL){
        if(dictionary_has_key(dicc_pcb,string_from_format("%u",interfaz->proceso_okupa->pid))){
            liberar_proceso(interfaz->proceso_okupa->pid);
        }
    }
    free(interfaz);
}

void procesar_peticion_IO(char* io, t_instruccion* tipo_instruccion, uint32_t pid, t_buffer* buffer){

    dispositivo_IO* interfaz;
    interfaz = dictionary_get(dicc_IO,io);
    //verifico la interfaz exista y este conectada
    if( interfaz == NULL){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no existe o no esta conectada", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
    }
    
    switch (*tipo_instruccion)
    {
    case GEN_SLEEP:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != GENERICA){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        //preparo y envio el paquete a la interfaz
        uint32_t u_trabajo = 0;
        buffer_read(buffer, &u_trabajo, sizeof(int));
        t_paquete* paquete = crear_paquete(ENTRADASALIDA,sizeof(t_instruccion)+sizeof(uint32_t)*2);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add_uint32(paquete->buffer,u_trabajo);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);


        break;
    }
    
    case STDIN_READ:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != STDIN){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
   
        //preparo y envio el paquete a la interfaz
        int cant_paginas;
        buffer_read(buffer,&cant_paginas,sizeof(int));
        uint32_t tam_direccion_fisica = buffer_read_uint32(buffer);
        int direccion_fisica[cant_paginas];
        buffer_read(buffer,direccion_fisica,tam_direccion_fisica);
        uint32_t size_escribir = buffer_read_uint32(buffer);
        int tam_a_escribir[cant_paginas];
        buffer_read(buffer,tam_a_escribir,size_escribir);

        t_paquete* paquete = crear_paquete(ENTRADASALIDA,sizeof(t_instruccion)+ sizeof(uint32_t)*3 + sizeof(int)+size_escribir+tam_direccion_fisica);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add(paquete->buffer, &cant_paginas,sizeof(int));
        buffer_add_uint32(paquete->buffer,tam_direccion_fisica);
        buffer_add(paquete->buffer, direccion_fisica,tam_direccion_fisica);
        buffer_add_uint32(paquete->buffer,size_escribir);
        buffer_add(paquete->buffer, tam_a_escribir,size_escribir);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);
        

        break;
    }
    case STDOUT_WRITE:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != STDOUT){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }

        
        //preparo y envio el paquete a la interfaz
        int cant_paginas;
        buffer_read(buffer,&cant_paginas,sizeof(int));
        uint32_t tam_direccion_fisica = buffer_read_uint32(buffer);
        int direccion_fisica[cant_paginas];
        buffer_read(buffer,direccion_fisica,tam_direccion_fisica);
        uint32_t size_escribir = buffer_read_uint32(buffer);
        int tam_a_escribir[cant_paginas];
        buffer_read(buffer,tam_a_escribir,size_escribir);
        
        t_paquete* paquete = crear_paquete(ENTRADASALIDA,sizeof(t_instruccion)+ sizeof(uint32_t)*3 + sizeof(int)+size_escribir+tam_direccion_fisica);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add(paquete->buffer, &cant_paginas,sizeof(int));
        buffer_add_uint32(paquete->buffer,tam_direccion_fisica);
        buffer_add(paquete->buffer, direccion_fisica,tam_direccion_fisica);
        buffer_add_uint32(paquete->buffer,size_escribir);
        buffer_add(paquete->buffer, tam_a_escribir,size_escribir);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);
        
    }        
    break;

    case FS_CREATE:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != DIALFS){

        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        uint32_t len = 0;
        char* nom_archivo = buffer_read_string(buffer,&len);
        t_paquete* paquete = crear_paquete(ENTRADASALIDA, sizeof(t_instruccion)+ sizeof(uint32_t)*2 + len);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add_string(paquete->buffer,len,nom_archivo);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);
        free(nom_archivo);
        break;
    }

    case FS_DELETE:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != DIALFS){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        uint32_t len = 0;
        char* nom_archivo = buffer_read_string(buffer,&len);
        t_paquete* paquete = crear_paquete(ENTRADASALIDA, sizeof(t_instruccion) + sizeof(uint32_t)*2 + len);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add_string(paquete->buffer,len,nom_archivo);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);
        free(nom_archivo);
        break;
    }
    
    case FS_TRUNCATE:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != DIALFS){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        uint32_t len = 0;
        char* nombre_archivo = buffer_read_string(buffer,&len);
        uint32_t tamanio_fs= buffer_read_uint32(buffer);

        t_paquete* paquete = crear_paquete(ENTRADASALIDA,sizeof(t_instruccion) + sizeof(uint32_t)*3 + len);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add_string(paquete->buffer, len, nombre_archivo);
        buffer_add_uint32(paquete->buffer, tamanio_fs);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola,procs);
        free(nombre_archivo);
        break;
    }

    case FS_WRITE:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != DIALFS){
        
        liberar_proceso(pid);
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        //preparo y envio el paquete a la interfaz
     
        uint32_t len = 0;
        char* nombre_archivo = buffer_read_string(buffer,&len);
        uint32_t cant_paginas = buffer_read_uint32(buffer);
        uint32_t tam_direccion_fisica = buffer_read_uint32(buffer);
        int direccion_fisica[cant_paginas];
        buffer_read(buffer,direccion_fisica,tam_direccion_fisica);
        uint32_t size_escribir = buffer_read_uint32(buffer);
        int tam_a_escribir[cant_paginas];
        buffer_read(buffer,tam_a_escribir,size_escribir);
        uint32_t puntero_archivo = buffer_read_uint32(buffer);
        
        
        t_paquete* paquete = crear_paquete(ENTRADASALIDA, sizeof(t_instruccion) + sizeof(uint32_t)*6 + tam_direccion_fisica 
                                            + size_escribir + string_length(nombre_archivo)+1);
        buffer_add(paquete->buffer, tipo_instruccion, sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer, pid);
        buffer_add_string(paquete->buffer, len, nombre_archivo);
        buffer_add_uint32(paquete->buffer, cant_paginas);
        buffer_add_uint32(paquete->buffer, tam_direccion_fisica);
        buffer_add(paquete->buffer, direccion_fisica, tam_direccion_fisica);
        buffer_add_uint32(paquete->buffer, size_escribir);
        buffer_add(paquete->buffer, tam_a_escribir, size_escribir);
        buffer_add_uint32(paquete->buffer, puntero_archivo);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola, procs);

        free(nombre_archivo);
        
        


        break;
    }

    case FS_READ:{
        //verifico que se acepte ese tipo de instruccion para el tipo de interfaz
        if(interfaz->tipo_interfaz != DIALFS){
        liberar_proceso(pid);
        
        log_error(logger_error,"Interfaz %s no acepta esta operacion", io);
        log_error(logger_kernel,"Finaliza el proceso %u - Motivo: INVALID_INTERFACE_INSTRUCTION", pid);
        log_error(logger_kernel, "PID: %u - Estado Anterior: EXEC - Estado Actual: EXIT", pid);
        return;
        }
        uint32_t len = 0;
        char* nombre_archivo = buffer_read_string(buffer,&len);
        uint32_t cant_paginas = buffer_read_uint32(buffer);
        uint32_t tam_direccion_fisica = buffer_read_uint32(buffer);
        int direccion_fisica[cant_paginas];
        buffer_read(buffer,direccion_fisica,tam_direccion_fisica);
        uint32_t size_leer = buffer_read_uint32(buffer);
        int tam_a_escribir[cant_paginas];
        buffer_read(buffer,tam_a_escribir,size_leer);
        uint32_t puntero_archivo = buffer_read_uint32(buffer);
        
        
        t_paquete* paquete = crear_paquete(ENTRADASALIDA, sizeof(t_instruccion) + sizeof(uint32_t)*6 + tam_direccion_fisica 
                                                            + size_leer + string_length(nombre_archivo)+1);
        buffer_add(paquete->buffer,tipo_instruccion,sizeof(t_instruccion));
        buffer_add_uint32(paquete->buffer,pid);
        buffer_add_string(paquete->buffer, len, nombre_archivo);
        buffer_add_uint32(paquete->buffer, cant_paginas);
        buffer_add_uint32(paquete->buffer,tam_direccion_fisica);
        buffer_add(paquete->buffer, direccion_fisica,tam_direccion_fisica);
        buffer_add_uint32(paquete->buffer,size_leer);
        buffer_add(paquete->buffer, tam_a_escribir,size_leer);
        buffer_add_uint32(paquete->buffer,puntero_archivo);
        proceso_en_cola* procs = malloc(sizeof(proceso_en_cola));
        procs->paquete=paquete;
        procs->proceso = dictionary_get(dicc_pcb, string_from_format("%u", pid));
        queue_push(interfaz->cola, procs);
        
        free(nombre_archivo);


        break;
    }

    default:
        break;
    }
    sem_post(&interfaz->pidieron_interfaz);
    

}

void gestionar_interfaces(dispositivo_IO* interfaz){
   
    while(1){
        sem_wait(&interfaz->pidieron_interfaz);
        sem_wait(&interfaz->esta_libre);
        if(apagando_sistema || interfaz->inactiva){
            return;
        }
        proceso_en_cola* proceso = queue_pop(interfaz->cola);
        enviar_paquete(proceso->paquete, interfaz->socket);
        interfaz->proceso_okupa = proceso->proceso;
        
        free(proceso);


    }
}


