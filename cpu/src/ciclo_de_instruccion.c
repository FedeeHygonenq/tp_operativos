#include "ciclo_de_instruccion.h"

uint32_t PID;
registros_CPU* contexto_registros;
char* linea_de_instruccion;
char** linea_de_instruccion_tokenizada;
uint32_t* longitud_linea_instruccion;
tipo_de_interrupcion motivo_interrupcion;
uint32_t dir_logica = 0;
uint32_t dir_fisica = 0;
int proceso_enviado = 0;
sem_t mutex_log;

void ciclo_de_instruccion(){
    fetch_instruction();
    decode();
    execute();
    check_interrupt();
}

void fetch_instruction(){
    if (contexto_registros == NULL) {
        log_info(logger_errores_cpu, "El contexto de ejecución está vacío (NULL).");
        return;
    }

    log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", PID, contexto_registros->PC);
    solicitar_instruccion_a_memoria();
    contexto_registros->PC++;
}

void decode(){
    recibir_instruccion_de_memoria();

    if (linea_de_instruccion != NULL) {
        linea_de_instruccion_tokenizada = string_n_split(linea_de_instruccion, *longitud_linea_instruccion, " ");
        log_info(logger_cpu, "La línea de instrucción recibida es %s", linea_de_instruccion);
    } else {
        log_info(logger_errores_cpu, "La línea de instrucción recibida está vacía (NULL).");
    }
}

void execute(){
    loggear_instruccion(*longitud_linea_instruccion);

    if(strcmp(linea_de_instruccion_tokenizada[0], "SET") == 0){
        uint32_t valor = atoi(linea_de_instruccion_tokenizada[2]);
        set(linea_de_instruccion_tokenizada[1], valor);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "MOV_IN") == 0){
	    mov_in(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "MOV_OUT") == 0){
	    mov_out(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "SUM") == 0){
        sum(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "SUB") == 0){
        sub(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "JNZ") == 0){
        uint32_t proxInstruccion = atoi(linea_de_instruccion_tokenizada[2]);
        jump_no_zero(linea_de_instruccion_tokenizada[1], proxInstruccion);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "RESIZE") == 0){
		uint tamanio = atoi(linea_de_instruccion_tokenizada[1]);
		resize(tamanio);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "COPY_STRING") == 0){
		uint32_t tamanio = atoi(linea_de_instruccion_tokenizada[1]);
		copy_string(tamanio);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "WAIT") == 0){
		wait(linea_de_instruccion_tokenizada[1]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "SIGNAL") == 0){
		signal(linea_de_instruccion_tokenizada[1]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_GEN_SLEEP") == 0){
        uint32_t unidades_de_trabajo = atoi(linea_de_instruccion_tokenizada[2]);
        io_gen_sleep(linea_de_instruccion_tokenizada[1], unidades_de_trabajo);
    }
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_STDIN_READ") == 0){
		io_stdin_read(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2], linea_de_instruccion_tokenizada[3]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_STDOUT_WRITE") == 0){
		io_stdout_write(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2], linea_de_instruccion_tokenizada[3]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_FS_CREATE") == 0){
		io_fs_create(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_FS_DELETE") == 0){
		io_fs_delete(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_FS_TRUNCATE") == 0){
		io_fs_truncate(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2], linea_de_instruccion_tokenizada[3]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_FS_WRITE") == 0){
		io_fs_write(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2], linea_de_instruccion_tokenizada[3],
                     linea_de_instruccion_tokenizada[4], linea_de_instruccion_tokenizada[5]);
	}
    else if(strcmp(linea_de_instruccion_tokenizada[0], "IO_FS_READ") == 0){
		io_fs_read(linea_de_instruccion_tokenizada[1], linea_de_instruccion_tokenizada[2], linea_de_instruccion_tokenizada[3],
                     linea_de_instruccion_tokenizada[4], linea_de_instruccion_tokenizada[5]);
	}
    else if(strncmp(linea_de_instruccion_tokenizada[0], "EXIT",4) == 0){// utilizo el strn para manejos de errores en caso de EXIT en lugar que no deberia
		exit_process();
	}
}

//NO SÉ SI LE FALTA ALGO. CHECKEAR.
void check_interrupt(){
    sem_wait(&sem_inter);
    while(llego_interrupcion > 0){
        llego_interrupcion--;
        if(proceso_enviado == 0){
            switch(motivo_interrupcion){
            case DESALOJO_QUANTUM:
                enviar_contexto_a_kernel(FIN_QUANTUM);
                break;

            case DESALOJO_POR_USUARIO:
                enviar_contexto_a_kernel(INTERRUPCION_USUARIO);
                break;

            default:
                log_info(logger_errores_cpu, "Ups! Pasó algo raro. El motivo de interrupción obtenido es: %s", string_itoa(motivo_interrupcion));
                break;
        }
        }
        else{
            log_info(logger_errores_cpu, "El proceso no pudo ser desalojado por %s ya que fue desalojado antes por otra razon", string_itoa(motivo_interrupcion));
        }
    }
    sem_post(&sem_inter);
}

// Función para traducir direcciones lógicas a físicas.
uint32_t mmu(uint32_t pid){
    uint32_t numero_pagina = floor(dir_logica / tamanio_pagina);
    uint32_t desplazamiento = dir_logica - (numero_pagina * tamanio_pagina);
    uint32_t marco = buscar_en_TLB(pid, numero_pagina);

    if(marco == -1){ //TLB Miss
        log_info(logger_tlb, "PID: %u - TLB MISS - Pagina: %u", PID, numero_pagina);
        marco = solicitar_marco_a_memoria(numero_pagina);
        actualizar_TLB(pid, numero_pagina, marco);
        
        if(marco == -1){
            log_info(logger_errores_cpu, "La memoria no envió el MARCO_BUSCADO.");
            exit(EXIT_FAILURE);
       }
    }

    //No sé si este log está bien puesto acá
    log_info(logger_cpu, "PID: %u - OBTENER MARCO - Página: %u - Marco: %u", PID, numero_pagina, marco);
  
    return (marco * tamanio_pagina) + desplazamiento;
}

//Funciones de instrucciones.
void set(char* registro, uint32_t valor){
    if(strcmp(registro, "PC") == 0){
        contexto_registros->PC = valor;
    }
    else if(strcmp(registro, "AX") == 0){
        contexto_registros->AX = valor;
    }
    else if(strcmp(registro, "BX") == 0){
        contexto_registros->BX = valor;
    }
    else if(strcmp(registro, "CX") == 0){
        contexto_registros->CX = valor;
    }
    else if(strcmp(registro, "DX") == 0){
        contexto_registros->DX = valor;
    }
    else if(strcmp(registro, "EAX") == 0){
        contexto_registros->EAX = valor;
    }
    else if(strcmp(registro, "EBX") == 0){
        contexto_registros->EBX = valor;
    }
    else if(strcmp(registro, "ECX") == 0){
        contexto_registros->ECX = valor;
    }
    else if(strcmp(registro, "EDX") == 0){
        contexto_registros->EDX = valor;
    }
    else if(strcmp(registro, "SI") == 0){
        contexto_registros->SI = valor;
    }
    else if(strcmp(registro, "DI") == 0){
        contexto_registros->DI = valor;
    }
}

void mov_in(char* registro_datos, char* registro_direccion){
    if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }
    dir_fisica = mmu(PID);
    uint32_t offset = dir_fisica % tamanio_pagina;
    uint32_t direccion_fisica_inicial = dir_fisica;

    if(!(registro_datos[1] == 'X')){
        t_buffer* buffer_auxiliar = buffer_create(sizeof(uint32_t));
        void* datos_de_memoria =  leer_en_memoria_mas_de_una_pagina(buffer_auxiliar, sizeof(uint32_t), sizeof(uint32_t));
        uint32_t valor = *(uint32_t*)datos_de_memoria;
        log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", PID, direccion_fisica_inicial, valor);

        set(registro_datos, valor);
        free(datos_de_memoria);
    }
    else{
        t_buffer* buffer_auxiliar = buffer_create(sizeof(uint8_t));
        void* datos_de_memoria = (uint8_t*)leer_en_memoria_mas_de_una_pagina(buffer_auxiliar, sizeof(uint8_t), sizeof(uint8_t) );
        
        uint8_t valor = *(uint8_t*)datos_de_memoria;
        log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %u", PID, direccion_fisica_inicial, valor);

        set(registro_datos, valor);
        free(datos_de_memoria);
    }
    
}

void mov_out(char* registro_direccion, char* registro_datos){
    if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }
    
        dir_fisica = mmu(PID);
        uint32_t direccion_fisica_inicial = dir_fisica;


    if(!(registro_datos[1] == 'X')){
        uint32_t* datos_de_registro = obtener_contenido_registro(registro_datos);
        t_buffer* buffer = buffer_create(sizeof(uint32_t));
        
        buffer_add(buffer, datos_de_registro, sizeof(uint32_t));
        buffer->offset = 0;
        escribir_en_memoria_mas_de_una_pagina(buffer, sizeof(uint32_t));

        buffer_destroy(buffer);
        log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", PID, direccion_fisica_inicial, *datos_de_registro);
        //Seguro tenga que usar una función auxiliar para asegurarme que la escritura haya sido exitosa,
        //y ahí mismo hago el loggeo de la escritura.
    }
    else{
        uint8_t* datos_de_registro = obtener_contenido_registro(registro_datos);
        
        t_buffer* buffer = buffer_create(sizeof(uint8_t));

        buffer_add_uint8(buffer, *datos_de_registro);
        buffer->offset = 0;
        escribir_en_memoria_mas_de_una_pagina(buffer, sizeof(uint8_t));
        
        buffer_destroy(buffer);
        log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %u", PID, direccion_fisica_inicial, *datos_de_registro);
        
        //Seguro tenga que usar una función auxiliar para asegurarme que la escritura haya sido exitosa,
        //y ahí mismo hago el loggeo de la escritura.
    }
}

void sum(char* reg_destino, char* reg_origen){
    uint32_t valor_a_sumar = *(uint32_t*)obtener_contenido_registro(reg_origen);

    if(strcmp(reg_destino, "PC") == 0){
        contexto_registros->PC += valor_a_sumar;
    } else if(strcmp(reg_destino, "AX") == 0){
        contexto_registros->AX += valor_a_sumar;
    } else if(strcmp(reg_destino, "BX") == 0){
        contexto_registros->BX += valor_a_sumar;
    } else if(strcmp(reg_destino, "CX") == 0){
        contexto_registros->CX += valor_a_sumar;
    } else if(strcmp(reg_destino, "DX") == 0){
        contexto_registros->DX += valor_a_sumar;
    } else if(strcmp(reg_destino, "EAX") == 0){
        contexto_registros->EAX += valor_a_sumar;
    } else if(strcmp(reg_destino, "EBX") == 0){
        contexto_registros->EBX += valor_a_sumar;
    } else if(strcmp(reg_destino, "ECX") == 0){
        contexto_registros->ECX += valor_a_sumar;
    } else if(strcmp(reg_destino, "EDX") == 0){
        contexto_registros->EDX += valor_a_sumar;
    } else if(strcmp(reg_destino, "SI") == 0){
        contexto_registros->SI += valor_a_sumar;
    } else if(strcmp(reg_destino, "DI") == 0){
        contexto_registros->DI += valor_a_sumar;
    }
}

void sub(char* reg_destino, char* reg_origen){
    uint32_t valor_a_restar = *(uint32_t*)obtener_contenido_registro(reg_origen);

    if(strcmp(reg_destino, "PC") == 0){
        contexto_registros->PC -= valor_a_restar;
    } else if(strcmp(reg_destino, "AX") == 0){
        contexto_registros->AX -= valor_a_restar;
    } else if(strcmp(reg_destino, "BX") == 0){
        contexto_registros->BX -= valor_a_restar;
    } else if(strcmp(reg_destino, "CX") == 0){
        contexto_registros->CX -= valor_a_restar;
    } else if(strcmp(reg_destino, "DX") == 0){
        contexto_registros->DX -= valor_a_restar;
    } else if(strcmp(reg_destino, "EAX") == 0){
        contexto_registros->EAX -= valor_a_restar;
    } else if(strcmp(reg_destino, "EBX") == 0){
        contexto_registros->EBX -= valor_a_restar;
    } else if(strcmp(reg_destino, "ECX") == 0){
        contexto_registros->ECX -= valor_a_restar;
    } else if(strcmp(reg_destino, "EDX") == 0){
        contexto_registros->EDX -= valor_a_restar;
    } else if(strcmp(reg_destino, "SI") == 0){
        contexto_registros->SI -= valor_a_restar;
    } else if(strcmp(reg_destino, "DI") == 0){
        contexto_registros->DI -= valor_a_restar;
    }
}

void jump_no_zero(char* registro, uint32_t nro_instruccion){
    uint32_t valor_actual;

    //No tendría sentido contemplar el caso de PC = 0...
    if(strcmp(registro, "AX") == 0){
        valor_actual = contexto_registros->AX;
    } else if(strcmp(registro, "BX") == 0){
        valor_actual = contexto_registros->BX;
    } else if(strcmp(registro, "CX") == 0){
        valor_actual = contexto_registros->CX;
    } else if(strcmp(registro, "DX") == 0){
        valor_actual = contexto_registros->DX;
    } else if(strcmp(registro, "EAX") == 0){
        valor_actual = contexto_registros->EAX;
    } else if(strcmp(registro, "EBX") == 0){
        valor_actual = contexto_registros->EBX;
    } else if(strcmp(registro, "ECX") == 0){
        valor_actual = contexto_registros->ECX;
    } else if(strcmp(registro, "EDX") == 0){
        valor_actual = contexto_registros->EDX;
    } else if(strcmp(registro, "SI") == 0){
        valor_actual = contexto_registros->SI;
    } else if(strcmp(registro, "DI") == 0){
        valor_actual = contexto_registros->DI;
    }

    if(valor_actual != 0){
        contexto_registros->PC = nro_instruccion;
    }
}

void resize(uint32_t tamanio){
    t_paquete* paquete = crear_paquete(AJUSTAR_TAMANIO, sizeof(uint32_t) * 2);
    buffer_add_uint32(paquete->buffer, PID);
    buffer_add_uint32(paquete->buffer, tamanio);
    
    enviar_paquete(paquete, sockets.socket_memoria);

    recibir_respuesta_resize_memoria(PID);
}

void copy_string(uint32_t tamanio){
    uint32_t*contenido_auxiliar = (uint32_t*)obtener_contenido_registro("SI");
    dir_logica = *contenido_auxiliar;
    dir_fisica = mmu(PID);

    t_buffer* buffer = buffer_create(tamanio);
    uint32_t direccion_fisica_inicial = dir_fisica;

    void* datos_de_memoria = leer_en_memoria_mas_de_una_pagina(buffer, tamanio, tamanio); // Uno voy a ir modificandolo y el otro tamanio abs
    log_info(logger_cpu, "PID: %d - Acción: LEER - Dirección Física: %d - Valor: %s", PID, direccion_fisica_inicial,(char*)datos_de_memoria);
    buffer_destroy(buffer);
    
    t_buffer* buffer_auxiliar = buffer_create(tamanio);
    
    buffer_add(buffer_auxiliar, datos_de_memoria, tamanio);
    buffer_auxiliar->offset =0;
    contenido_auxiliar = (uint32_t*)obtener_contenido_registro("DI");

    dir_logica = *contenido_auxiliar;
    dir_fisica = mmu(PID);
    
    direccion_fisica_inicial = dir_fisica;

    escribir_en_memoria_mas_de_una_pagina(buffer, tamanio);
    log_info(logger_cpu, "PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", PID, direccion_fisica_inicial, (char*) datos_de_memoria);
    buffer_destroy(buffer_auxiliar);

    free(datos_de_memoria);
}

void wait(char* nombre_recurso){
    ind_contexto_kernel = 0;

    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(registros_CPU)
                                        + string_length(nombre_recurso)+1+sizeof(uint32_t));
    motivo_desalojo mot = PETICION_RECURSO;
    buffer_add(paquete->buffer, &mot, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add_string(paquete->buffer, string_length(nombre_recurso)+1, nombre_recurso);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void signal(char* nombre_recurso){
    ind_contexto_kernel = 0;

    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(registros_CPU)
                                        + string_length(nombre_recurso)+1+sizeof(uint32_t));
    motivo_desalojo mot = SIGNAL_RECURSO;
    buffer_add(paquete->buffer, &mot, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    buffer_add_string(paquete->buffer, string_length(nombre_recurso)+1, nombre_recurso);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void io_gen_sleep(char* nombre_interfaz, uint32_t unidades_de_trabajo){
    ind_contexto_kernel = 0;
    // si le saco 8 a motivo de desalojo se traduce la isntruccion (por el enum)
    t_paquete* paquete = crear_paquete(CONTEXTO_EXEC, sizeof(motivo_desalojo) + sizeof(t_instruccion) +
                                        sizeof(registros_CPU) + string_length(nombre_interfaz)+1 + sizeof(uint32_t)*2);
    motivo_desalojo mot_des = PETICION_IO;
    buffer_add(paquete->buffer, &mot_des, sizeof(motivo_desalojo));
    buffer_add(paquete->buffer, contexto_registros, sizeof(registros_CPU));
    t_instruccion mot_io = GEN_SLEEP;                                    
    buffer_add(paquete->buffer, &mot_io, sizeof(t_instruccion));
    buffer_add_string(paquete->buffer, string_length(nombre_interfaz)+1, nombre_interfaz);
    buffer_add_uint32(paquete->buffer, unidades_de_trabajo);

    enviar_paquete(paquete, sockets.socket_kernel_D);
    proceso_enviado = 1;
}

void io_stdin_read(char* nombre_interfaz, char* registro_direccion, char* registro_tamanio){
    ind_contexto_kernel = 0;
    if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }

    if(!(registro_tamanio[1] == 'X')){
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint32_t));
        
        buffer_add_uint32(buffer, *(uint32_t*)tamanio);        
        buffer->offset=0;
        envios_de_std_a_kernel(STDIN_READ, nombre_interfaz, sizeof(uint32_t), buffer);
        
        buffer_destroy(buffer);
        
    }
    else{
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint8_t));

        buffer_add_uint8(buffer, *(uint8_t*)tamanio);        
        buffer->offset=0;
        envios_de_std_a_kernel(STDIN_READ, nombre_interfaz, sizeof(uint8_t), buffer);
        
        buffer_destroy(buffer);
        
    }

    
}

void io_stdout_write(char* nombre_interfaz, char* registro_direccion, char* registro_tamanio){
    ind_contexto_kernel = 0;
      if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }
    
    if(!(registro_tamanio[1] == 'X')){
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint32_t));
        
        buffer_add_uint32(buffer, *(uint32_t*)tamanio);        
        buffer->offset=0;
        envios_de_std_a_kernel(STDOUT_WRITE, nombre_interfaz, sizeof(uint32_t), buffer);
        
        buffer_destroy(buffer);
       
    }
    else{
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint8_t));

        buffer_add_uint8(buffer, *(uint8_t*)tamanio);        
        buffer->offset=0;
        envios_de_std_a_kernel(STDOUT_WRITE, nombre_interfaz, sizeof(uint8_t), buffer);
        
        buffer_destroy(buffer);
        
 }

   
}

void io_fs_create(char* nombre_interfaz, char* nombre_archivo){
    ind_contexto_kernel = 0;

    solicitar_create_delete_fs_a_kernel(FS_CREATE, nombre_interfaz, nombre_archivo);
}

void io_fs_delete(char* nombre_interfaz, char* nombre_archivo){
    ind_contexto_kernel = 0;

    solicitar_create_delete_fs_a_kernel(FS_DELETE, nombre_interfaz, nombre_archivo);
}

void io_fs_truncate(char* nombre_interfaz, char* nombre_archivo, char* registro_tamanio){
    ind_contexto_kernel = 0;

    if(!(registro_tamanio[1] == 'X')){
        uint32_t tamanio = *(uint32_t*)obtener_contenido_registro(registro_tamanio);

        solicitar_truncate_fs_a_kernel(FS_TRUNCATE, nombre_interfaz, nombre_archivo, tamanio);

        
    }
    else{
        uint8_t tamanio = *(uint8_t*)obtener_contenido_registro(registro_tamanio);

        solicitar_truncate_fs_a_kernel(FS_TRUNCATE, nombre_interfaz, nombre_archivo, tamanio);

        
    }
}


void io_fs_write(char* nombre_interfaz, char* nombre_archivo, char* registro_direccion,
                char* registro_tamanio, char* registro_puntero_archivo){

    ind_contexto_kernel = 0;
    if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }

    dir_fisica = mmu(PID);

    if(!(registro_tamanio[1] == 'X')){
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint32_t));
        
        buffer_add_uint32(buffer, *(uint32_t*)tamanio);
        buffer->offset=0;
        if(!(registro_puntero_archivo[1] == 'X')){
            uint32_t* puntero_registro = (uint32_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_WRITE, nombre_interfaz, nombre_archivo, buffer, sizeof(uint32_t),
                                             *puntero_registro);
        
            
        }
        else{
            uint8_t* puntero_registro = (uint8_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_WRITE, nombre_interfaz, nombre_archivo, buffer, sizeof(uint32_t),
                                             *puntero_registro);
        
            
        }
        
        buffer_destroy(buffer);
        
    }
    else{
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint8_t));

        buffer_add_uint8(buffer, *(uint8_t*)tamanio);
        buffer->offset=0;
        if(!(registro_puntero_archivo[1] == 'X')){
            uint32_t* puntero_registro = (uint32_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_WRITE, nombre_interfaz, nombre_archivo, buffer, sizeof(uint8_t),
                                             *puntero_registro);
        
            
        }
        else{
            uint8_t* puntero_registro = (uint8_t*)obtener_contenido_registro(registro_puntero_archivo);


            solicitudes_fs_a_kernel(FS_WRITE, nombre_interfaz, nombre_archivo, buffer, sizeof(uint8_t),
                                             *puntero_registro);

            
        }

        buffer_destroy(buffer);
        
    }

    
}

void io_fs_read(char* nombre_interfaz, char* nombre_archivo, char* registro_direccion,
                char* registro_tamanio, char* registro_puntero_archivo){
    ind_contexto_kernel = 0;
    if(!(registro_direccion[1] == 'X')){    
        uint32_t* contenido_auxiliar = (uint32_t*)obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }

    }else{
        uint8_t* contenido_auxiliar = (uint8_t*) obtener_contenido_registro(registro_direccion);
        dir_logica = *contenido_auxiliar;
        if(contenido_auxiliar == NULL){
            log_info(logger_errores_cpu, "El registro ingresado no fue correcto. Se obtuvo NULL.");
            return;
        }
    }

    dir_fisica = mmu(PID);

    if(!(registro_tamanio[1] == 'X')){
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint32_t));
        buffer_add_uint32(buffer, *(uint32_t*)tamanio);
        buffer->offset=0;

        if(!(registro_puntero_archivo[1] == 'X')){
            uint32_t* puntero_registro = (uint32_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_READ, nombre_interfaz, nombre_archivo, buffer, sizeof(uint32_t),
                                            *puntero_registro);
        
            
        }
        else{
            uint8_t* puntero_registro = (uint8_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_READ, nombre_interfaz, nombre_archivo, buffer, sizeof(uint32_t),
                                            *puntero_registro);
        
            
        }

        buffer_destroy(buffer);
        
    }
    else{
        void* tamanio = obtener_contenido_registro(registro_tamanio);
        t_buffer* buffer = buffer_create(sizeof(uint8_t));

        buffer_add_uint8(buffer, *(uint8_t*)tamanio);
        buffer->offset=0;
        if(!(registro_puntero_archivo[1] == 'X')){
            uint32_t* puntero_registro = (uint32_t*)obtener_contenido_registro(registro_puntero_archivo);
            t_buffer* buffer = buffer_create(sizeof(uint32_t));
            buffer_add_uint32(buffer, *(uint32_t*)tamanio);

            solicitudes_fs_a_kernel(FS_READ, nombre_interfaz, nombre_archivo, buffer, sizeof(uint8_t),
                                            *puntero_registro);
        
            
        }
        else{
            uint8_t* puntero_registro = (uint8_t*)obtener_contenido_registro(registro_puntero_archivo);

            solicitudes_fs_a_kernel(FS_READ, nombre_interfaz, nombre_archivo, buffer, sizeof(uint8_t),
                                            *puntero_registro);

        }
        
        buffer_destroy(buffer);
        
    }

   
}

void exit_process(){
    ind_contexto_kernel = 0;

    enviar_contexto_a_kernel(PROCESS_EXIT);
}

//FUNCIONES AUXILIARES.
void* obtener_contenido_registro(const char* registro) {
    if (strncmp(registro, "PC",2 )== 0) {
        return &(contexto_registros->PC);
    }
    else if (strncmp(registro, "AX",2 )== 0) {
        return &(contexto_registros->AX);
    }
    else if (strncmp(registro, "BX",2 )== 0) {
        return &(contexto_registros->BX);
    }
    else if (strncmp(registro, "CX",2) == 0) {
        return &(contexto_registros->CX);
    }
    else if (strncmp(registro, "DX",2) == 0) {
        return &(contexto_registros->DX);
    }
    else if (strncmp(registro, "EAX",3) == 0) {
        return &(contexto_registros->EAX);
    }
    else if (strncmp(registro, "EBX",3) == 0) {
        return &(contexto_registros->EBX);
    }
    else if (strncmp(registro, "ECX",3) == 0) {
        return &(contexto_registros->ECX);
    }
    else if (strncmp(registro, "EDX",3) == 0) {
        return &(contexto_registros->EDX);
    }
    else if (strncmp(registro, "SI",2) == 0) {
        return &(contexto_registros->SI);
    }
    else if (strncmp(registro, "DI",2) == 0) {
        return &(contexto_registros->DI);
    }
    else {
        return NULL; // Retornar NULL si el registro no es encontrado
    }
}

void loggear_instruccion(uint32_t longitud_linea_instruccion){
    char* instruccion_completa = string_duplicate(linea_de_instruccion);

    //Obtengo la instrucción.
    strtok(instruccion_completa, " ");
    //Obtengo, del string que quedó, los parámetros.
    char* parametros = strtok(NULL, "");

    // Si no hay parámetros, se pone el string en "".
    if(parametros == NULL){
        parametros = "";
    }
    char* instruccion = string_duplicate(instruccion_completa);
    char* param = string_duplicate(parametros);
    log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s", PID, instruccion, param);
    free(instruccion);
    free(param);
    free(instruccion_completa);
}