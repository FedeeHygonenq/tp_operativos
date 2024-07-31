#ifndef TLB_H
#define TLB_H

#include <utils/estructuras.h>
#include <commons/string.h>
#include <commons/temporal.h>
#include <stdint.h>

// Estructura de entrada de la TLB
typedef struct {
    uint32_t pid; // Identificador del proceso
    uint32_t pagina; // Número de página
    uint32_t marco; // Número de marco
    uint64_t uso; // Contador de uso para LRU FIFO
} t_entrada_TLB;

// Estructura de la TLB
typedef struct {
    t_entrada_TLB* entradas;
    uint32_t capacidad;
    uint32_t count;
    t_temporal* tiempo_actual; // Para llevar el tiempo en FIFO
    char* algoritmo; // Algoritmo de reemplazo ("FIFO" o "LRU")
} t_TLB;

extern t_TLB* tlb;

void inicializar_TLB(uint32_t, char*);
void destruir_TLB();
uint32_t buscar_en_TLB(uint32_t, uint32_t);
void actualizar_TLB(uint32_t, uint32_t, uint32_t);
void reemplazo_en_TLB(t_entrada_TLB);

#endif