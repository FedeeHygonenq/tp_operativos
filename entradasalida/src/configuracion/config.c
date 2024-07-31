#include "config.h"




t_cfg configuracion;
t_config* config;
t_list* config_list;
t_log* logger_entrada_salida;
t_log* logger_conexiones;
t_log* logger_salida;
t_log* logger_fs;
t_log* logger_errores;
char * tipo_interfaz_string;
t_bitarray* bitmap = NULL;
int tamanio_bitmap_bytes = 0;
int tamanio_memoria_bloques = 0;
int fd_bitmap;
int fd_bloques;
int fd_indice;
void* bitmap_memoria = NULL;
void* bloques_memoria = NULL;
void ** bloques;
char** indice;
char* path_indice;



t_interfaz config_string_a_enum(char* str){
    if (strcmp(str,"GENERICA")== 0 ){
        return GENERICA;
    }
    else if (strcmp(str,"STDIN")== 0 ){
        return STDIN;
    }
    else if (strcmp(str,"STDOUT")== 0 ){
        return STDOUT;
    }
    else if (strcmp(str,"DIALFS")== 0 ){
        return DIALFS;
    }
    else{
        return -1;
    }

}

void obtener_config(char* path_config){

    config = config_create(path_config);
    tipo_interfaz_string = config_get_string_value(config,"TIPO_INTERFAZ");
    configuracion.TIPO_INTERFAZ = config_string_a_enum(tipo_interfaz_string);
    switch (configuracion.TIPO_INTERFAZ) {
        case GENERICA: 
            configuracion.TIEMPO_UNIDAD_TRABAJO = config_get_int_value(config,"TIEMPO_UNIDAD_TRABAJO");
            configuracion.IP_KERNEL = config_get_string_value(config,"IP_KERNEL");
            configuracion.PUERTO_KERNEL = config_get_int_value(config,"PUERTO_KERNEL");
            configuracion.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
            configuracion.PUERTO_MEMORIA = config_get_int_value(config,"PUERTO_MEMORIA");
            break;
        case STDIN:
            configuracion.IP_KERNEL = config_get_string_value(config,"IP_KERNEL");
            configuracion.PUERTO_KERNEL = config_get_int_value(config,"PUERTO_KERNEL");
            configuracion.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
            configuracion.PUERTO_MEMORIA = config_get_int_value(config,"PUERTO_MEMORIA");
            break;
        case STDOUT:
            configuracion.IP_KERNEL = config_get_string_value(config,"IP_KERNEL");
            configuracion.PUERTO_KERNEL = config_get_int_value(config,"PUERTO_KERNEL");
            configuracion.TIEMPO_UNIDAD_TRABAJO = config_get_int_value(config,"TIEMPO_UNIDAD_TRABAJO");
            configuracion.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
            configuracion.PUERTO_MEMORIA = config_get_int_value(config,"PUERTO_MEMORIA");
            break;
        case DIALFS:
            configuracion.IP_KERNEL = config_get_string_value(config,"IP_KERNEL");
            configuracion.PUERTO_KERNEL = config_get_int_value(config,"PUERTO_KERNEL");
            configuracion.IP_MEMORIA = config_get_string_value(config,"IP_MEMORIA");
            configuracion.PUERTO_MEMORIA = config_get_int_value(config,"PUERTO_MEMORIA");
            configuracion.TIEMPO_UNIDAD_TRABAJO = config_get_int_value(config,"TIEMPO_UNIDAD_TRABAJO");
            configuracion.PATH_BASE_DIALFS = config_get_string_value(config,"PATH_BASE_DIALFS");
            configuracion.BLOCK_SIZE = config_get_int_value(config,"BLOCK_SIZE");
            configuracion.BLOCK_COUNT = config_get_int_value(config,"BLOCK_COUNT"); 
            configuracion.RETRASO_COMPACTACION = config_get_int_value(config,"RETRASO_COMPACTACION"); 
            levantar_fs(configuracion.PATH_BASE_DIALFS,configuracion.BLOCK_SIZE,configuracion.BLOCK_COUNT);           
            break;
    }
    list_add(config_list,config);
}

void iniciar_logger(){
    logger_entrada_salida = log_create("logs/entrada salida.log","Entrada Salida",0,LOG_LEVEL_INFO);
    logger_conexiones = log_create("logs/conexiones.log","Conexion",0,LOG_LEVEL_INFO);
    logger_salida = log_create("logs/salidas.log","SALIDA",0,LOG_LEVEL_INFO);
    logger_fs = log_create("logs/fs.log","FS",0,LOG_LEVEL_INFO);
    logger_errores = log_create("logs/errores.log","ERROR",0,LOG_LEVEL_INFO);
}


void levantar_fs(char* path_fs, uint8_t tamanio_bloques, uint16_t cantidad_bloques) {
    //leer archivos de bloques y bitmap, si no existen, crearlos
    //agregar /bloques.dat y /bitmap.dat al path a la hora de hacer el fopen
    //lock para que mas de 1 interfaz no utilice mismos archivos en simultaneo
    char* path_bloques = string_new();
    string_append(&path_bloques,path_fs);
    string_append(&path_bloques,"/bloques.dat");
    char* path_bitmap = string_new();
    string_append(&path_bitmap,path_fs);
    string_append(&path_bitmap,"/bitmap.dat");
    path_indice = string_new();
    string_append(&path_indice,path_fs);
    string_append(&path_indice,"/indice.dat");
    if(cantidad_bloques%8 !=0) {
        tamanio_bitmap_bytes = (cantidad_bloques/8 )+1;
    } else {
        tamanio_bitmap_bytes = cantidad_bloques/8;
    }
    tamanio_memoria_bloques = tamanio_bloques * cantidad_bloques;
    //generar tamanio indice
    FILE* archivo_bloques = fopen(path_bloques,"r+");
    FILE* archivo_bitmap = fopen(path_bitmap,"r+");
    FILE* archivo_indice = fopen(path_indice,"r+");
    if(!archivo_bloques) { //si no existe el archivo aun, lo creo y corro ftruncate para inicializar bloques
        inicializar_fs(path_bloques, path_bitmap, path_indice, tamanio_bloques,  cantidad_bloques);
    } else { 
        // si los archivos existen, los mapeo a las variables 
        int fd_bloques = fileno(archivo_bloques);
        int fd_bitmap = fileno(archivo_bitmap);
        fd_indice = fileno(archivo_indice);
        bloques = mapear_archivo_bloques(fd_bloques,tamanio_bloques,cantidad_bloques);
        log_info(logger_entrada_salida, "Archivo bloques.dat mapeado");
        bitmap = mapear_archivo_bitmap(fd_bitmap,tamanio_bitmap_bytes);
        log_info(logger_entrada_salida, "Archivo bitmap.dat mapeado");
        
        fclose(archivo_indice);
    }


}

void inicializar_fs(char * path_bloques, char* path_bitmap, char* path_indice, uint8_t tamanio_bloques, uint16_t cantidad_bloques ){
    bloques = crear_bloques(path_bloques,tamanio_bloques,cantidad_bloques);
    bitmap = crear_bitmap(path_bitmap,cantidad_bloques);
    fd_indice = crear_indice(path_indice,cantidad_bloques);
}

void* crear_bloques(char* path_bloques, uint32_t tamanio_bloques, uint32_t cantidad_bloques ){
    fd_bloques = open(path_bloques, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_bloques == -1) {
        log_info(logger_entrada_salida, "Fallo la creacion del archivo de bloques");
        return NULL;
    }
    if (ftruncate(fd_bloques, tamanio_memoria_bloques) == -1) {
        log_info(logger_entrada_salida, "Error al truncar el archivo de bloques");
        close(fd_bloques);
        return NULL;
    }
    bloques_memoria = mapear_archivo_bloques(fd_bloques, tamanio_bloques,  cantidad_bloques);
    log_info(logger_entrada_salida, "Archivo bloques.dat creado");
    return bloques_memoria;
}

void* mapear_archivo_bloques(int fd_bloques, uint32_t tamanio_bloques, uint32_t cantidad_bloques){
    bloques_memoria = mmap(NULL, tamanio_memoria_bloques,PROT_WRITE, MAP_SHARED, fd_bloques, 0);
    if (bloques_memoria == MAP_FAILED) {
        log_info(logger_entrada_salida, "Fallo el mmap de bloques");
        close(fd_bloques);
        return NULL;
    }
    if (close(fd_bloques) == -1) {
        log_info(logger_entrada_salida, "Error al cerrar el archivo bloques.dat");
        exit(EXIT_FAILURE);
    }
    void **bloques = malloc(cantidad_bloques * sizeof(void *));
    for (int i = 0; i < cantidad_bloques; ++i) {
        bloques[i] = (char *)bloques_memoria + i * tamanio_bloques;
    }
    return bloques;
}

t_bitarray* crear_bitmap(char* path_bitmap, uint32_t cantidad_bloques ){
    //tengo en cuenta tamaño bitmap, si cantidad de bloques/8 no da redondo, redondear para arriba
    //crea file descriptor a archivo generado bitmap.dat
    fd_bitmap = open(path_bitmap, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_bitmap == -1) {
        log_info(logger_entrada_salida, "Fallo la creacion del archivo bitmap");
        return NULL;
    }
    //defino tamaño con ftruncate
    if (ftruncate(fd_bitmap, tamanio_bitmap_bytes) == -1) {
        log_info(logger_entrada_salida, "Error al truncar el archivo de bitmap");
        close(fd_bitmap);
        return NULL;
    }
    // Mapear el archivo en memoria
    bitmap = mapear_archivo_bitmap(fd_bitmap,tamanio_bitmap_bytes);
    log_info(logger_entrada_salida, "Archivo bitmap.dat creado");
    return bitmap;
}

t_bitarray* mapear_archivo_bitmap (int fd_bitmap, int tamanio_bitmap_bytes) {
    bitmap_memoria = mmap(NULL, tamanio_bitmap_bytes,PROT_WRITE, MAP_SHARED, fd_bitmap, 0);
    if (bitmap_memoria == MAP_FAILED) {
        log_info(logger_entrada_salida, "Fallo el mmap de bitmap");
        close(fd_bitmap);
        return NULL;
    }
    //creo el bitarray con puntero a el espacio de memoria mapeado
    bitmap = bitarray_create_with_mode(bitmap_memoria,tamanio_bitmap_bytes,LSB_FIRST);
    if (close(fd_bitmap) == -1) {
        log_info(logger_entrada_salida, "Error al cerrar el archivo bitmap.dat");
        return NULL;
    }
    log_info(logger_entrada_salida, "Bitmap de %li bits", bitarray_get_max_bit(bitmap));
    log_info(logger_entrada_salida, "Bits libres: %i", contar_bloques_libres(bitmap));
    return bitmap;
}

int crear_indice(char* path_indice, uint32_t cantidad_bloques) {
    int fd_indice = open(path_indice, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_indice == -1) {
        log_error(logger_errores, "Fallo la creacion del archivo indice");
        return -1;
    }
    //no mapeo el archivo a memoria ya que tendria que reservar un filepath por bloque o tener que remapear a memoria cada vez que se amplia el tamaño
    log_info(logger_fs, "Archivo indice.dat creado");
    return fd_indice;
}

void destruir_config(){
    log_destroy(logger_entrada_salida);
    log_destroy(logger_conexiones);
    log_destroy(logger_salida);
    log_destroy(logger_fs);
    log_destroy(logger_errores);
    list_destroy_and_destroy_elements(config_list, (void*)config_destroy);
    free(tipo_interfaz_string);
    free(path_indice);
    free(indice);
    free(bloques);
}