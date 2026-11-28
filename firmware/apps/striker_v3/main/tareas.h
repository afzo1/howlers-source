/*
 * Striker v3 - tareas de FreeRTOS (Entrega 1, seccion 3: nucleos, prioridades
 * y periodos). Todas se crean de forma estatica y fijadas a su nucleo.
 */
#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "medicion.h"

typedef struct {
    const char *nombre;
    BaseType_t nucleo;
    UBaseType_t prioridad;
    uint32_t periodo_ms; /* 0 = la activan eventos */
    uint32_t desfase_ms;
    uint32_t pila;       /* bytes */
    medidor_t ejec;      /* ciclos de CPU de cada activacion */
    medidor_t resp;      /* us desde la liberacion hasta terminar */
    TaskHandle_t handle;
} tarea_info_t;

/* Crea todas las tareas. T_ctrl arranca el gptimer y libera a las demas. */
void tareas_crear(void);

/* Tabla de tareas (para la consola). */
int tareas_cantidad(void);
tarea_info_t *tareas_info(int i);

/* Imprime cada 10 s el numero de activaciones de T_ctrl (comando "conteo"). */
extern volatile bool g_conteo_activo;
