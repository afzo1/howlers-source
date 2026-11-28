/*
 * Striker v3 - firmware unificado para Striker 1 y Striker 2 (ESP32-S3).
 *
 * Estado actual: esqueleto con todas las tareas de la Entrega 1, el lazo de
 * 1 kHz despertado por el gptimer y la medicion de tiempos. No configura
 * ningun pin de motor: nada se mueve.
 */
#include <stdio.h>

#include "consola.h"
#include "tareas.h"

void app_main(void)
{
    printf("\nStriker v3 (esqueleto): ningun pin de motor configurado.\n");
    printf("Escriba 'help' para ver los comandos.\n");
    tareas_crear();
    consola_iniciar();
}
