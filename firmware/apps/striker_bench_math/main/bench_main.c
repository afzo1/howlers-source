/*
 * Striker v3 - benchmark de matematicas (Entrega 2, paquete P2).
 *
 * Mide cuantos ciclos de CPU cuesta cada variante de los calculos del
 * firmware (PID, filtro del encoder, matrices y un paso del EKF) en double,
 * float, punto fijo Q16.16 y con esp-dsp, y cuanto error agrega el punto fijo.
 * Corre una sola vez al arrancar, en una tarea fijada al Core 1, con el WiFi
 * apagado. No configura ningun pin.
 *
 * Uso: docs en firmware/apps/striker_bench_math/README.md.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_cpu.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bench.h"

#define CALENTAMIENTO 100
#define LLAMADAS      1000
#define REPETICIONES  5
#define MAX_FILAS     64

volatile float bench_sumidero_f;
volatile double bench_sumidero_d;
volatile int32_t bench_sumidero_i;

typedef struct {
    const char *grupo;
    const char *caso;
    const char *variante;
    float ciclos;
    float speedup;
} fila_t;

static fila_t s_filas[MAX_FILAS];
static int s_num_filas;
static const char *s_grupo = "";
static uint32_t s_mhz = 240;
static uint32_t s_azar = 12345u;

uint32_t bench_azar(void)
{
    s_azar = s_azar * 1664525u + 1013904223u;
    return s_azar;
}

float bench_azar_f(float min, float max)
{
    return min + (max - min) * (float)(bench_azar() >> 8) / 16777216.0f;
}

static int comparar_float(const void *a, const void *b)
{
    float x = *(const float *)a;
    float y = *(const float *)b;
    return (x > y) - (x < y);
}

float bench_medir(bench_caso_t caso)
{
    float muestras[REPETICIONES];

    caso(CALENTAMIENTO);
    for (int r = 0; r < REPETICIONES; r++) {
        uint32_t t0 = esp_cpu_get_cycle_count();
        caso(LLAMADAS);
        uint32_t t1 = esp_cpu_get_cycle_count();
        muestras[r] = (float)(t1 - t0) / (float)LLAMADAS;
        /* Deja correr a la tarea Idle de este nucleo (vigilante de tareas). */
        vTaskDelay(1);
    }
    qsort(muestras, REPETICIONES, sizeof(float), comparar_float);
    return muestras[REPETICIONES / 2];
}

void bench_grupo(const char *titulo)
{
    s_grupo = titulo;
    printf("\n[%s]\n", titulo);
    printf("%-30s %-28s %10s %9s %8s\n", "caso", "variante", "ciclos", "us", "speedup");
}

void bench_fila(const char *caso, const char *variante, float ciclos, float ciclos_ref)
{
    float speedup = ciclos_ref / ciclos;
    printf("%-30s %-28s %10.1f %9.3f %8.2f\n", caso, variante, ciclos, ciclos / (float)s_mhz, speedup);
    if (s_num_filas < MAX_FILAS) {
        s_filas[s_num_filas++] = (fila_t){s_grupo, caso, variante, ciclos, speedup};
    }
}

void bench_nota(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("  > ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static void imprimir_csv(void)
{
    printf("\n--- CSV (copiar a una hoja de calculo; separador coma, decimal punto) ---\n");
    printf("grupo,caso,variante,ciclos,us,speedup\n");
    for (int i = 0; i < s_num_filas; i++) {
        const fila_t *f = &s_filas[i];
        printf("\"%s\",\"%s\",\"%s\",%.1f,%.3f,%.2f\n", f->grupo, f->caso, f->variante,
               f->ciclos, f->ciclos / (float)s_mhz, f->speedup);
    }
    printf("--- fin del CSV ---\n");
}

static void tarea_bench(void *arg)
{
    (void)arg;
    s_mhz = esp_rom_get_cpu_ticks_per_us();

    printf("\n=== Striker v3: benchmark de matematicas ===\n");
    printf("CPU a %lu MHz, nucleo %d, WiFi apagado.\n", (unsigned long)s_mhz, xPortGetCoreID());
    printf("Cada valor: mediana de %d repeticiones de %d llamadas (ciclos por llamada).\n",
           REPETICIONES, LLAMADAS);
    printf("us = ciclos / %lu. speedup = ciclos de la referencia del grupo / ciclos de la fila.\n",
           (unsigned long)s_mhz);

    casos_pid();
    casos_matrices();
    casos_ekf();
    imprimir_csv();

    printf("\nFIN del benchmark. Presione RST para repetirlo.\n");
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* Prioridad 5: por encima de las tareas comunes, por debajo de las del
     * sistema. En el Core 1 no corre nada mas (el WiFi no se inicia). */
    xTaskCreatePinnedToCore(tarea_bench, "bench", 8192, NULL, 5, NULL, 1);
}
