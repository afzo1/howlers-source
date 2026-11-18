#pragma once

#include <stdbool.h>

/**
 * Pide los angulos a la IMU TM151 (Striker 1) durante secs segundos y los imprime.
 * stop_requested() se consulta en cada ciclo para terminar antes.
 * Devuelve 0 si la prueba se pudo ejecutar (aunque la IMU no responda).
 */
int tm151_test(int tx_gpio, int rx_gpio, int secs, bool (*stop_requested)(void));
