/*
 * Striker v3 - medicion de tiempos sin instrumentos externos.
 *
 * Cada medidor guarda numero de muestras, minimo, maximo, suma e histograma
 * (casillas de 1 us). Tiene un solo escritor (una tarea o una ISR) y se lee
 * desde la consola con un seqlock, sin mutex: el escritor nunca se bloquea.
 * No usa float, porque tambien se llama desde ISR.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MED_CASILLAS 200 /* casillas de 1 us; la ultima junta todo lo que se pasa */

typedef struct {
    const char *nombre;      /* nombre corto para la consola */
    const char *descripcion;
    uint32_t ticks_por_us;   /* 10 = gptimer, 240 = ciclos de CPU, 80 = captura, 1 = us */
    int32_t base_us;         /* valor (en us) de la casilla 0 */
    volatile uint32_t seq;   /* seqlock: impar mientras el escritor actualiza */
    uint32_t gen;            /* generacion del ultimo "stats reset" aplicado */
    uint32_t n;
    uint32_t min;
    uint32_t max;
    uint64_t suma;
    uint32_t bajo;           /* muestras por debajo de base_us */
    uint32_t hist[MED_CASILLAS + 1];
} medidor_t;

#define MEDIDOR(nom, desc, tpu, base) \
    { .nombre = (nom), .descripcion = (desc), .ticks_por_us = (tpu), .base_us = (base) }

/* Agrega una muestra (en ticks del medidor). Solo la llama su escritor. */
void med_registrar(medidor_t *m, uint32_t ticks);

/* Copia coherente para leer desde otra tarea (si no hay muestras desde el
 * ultimo reinicio, la copia queda con n = 0). */
void med_copiar(const medidor_t *m, medidor_t *copia);

/* Pide reiniciar todos los medidores: cada escritor borra los suyos en su
 * siguiente muestra. */
void med_reiniciar_todo(void);

/* Medidores de T_ctrl (los escribe T_ctrl) y de la captura MCPWM (los escribe
 * la ISR de la captura, en el Core 0). */
extern medidor_t g_med_lat_isr;
extern medidor_t g_med_activacion;
extern medidor_t g_med_periodo;
extern medidor_t g_med_duracion;
extern medidor_t g_med_cap_periodo;
extern medidor_t g_med_cap_ancho;

/* Contadores de T_ctrl desde el arranque. */
extern volatile uint32_t g_activaciones;
extern volatile uint32_t g_perdidas;

/* Mascara del pin de medida que T_ctrl sube y baja (0 = apagado). */
extern volatile uint32_t g_pin_mascara;

/* Carga sintetica de T_ctrl, en ciclos de CPU (para validar las medidas). */
extern volatile uint32_t g_carga_ciclos;

/* Pin de medida con captura MCPWM: solo acepta pines libres en Striker 1 y 2.
 * pin < 0 lo apaga. Devuelve false si el pin no esta permitido o falla. */
bool med_pin_configurar(int pin);
int med_pin_actual(void);
