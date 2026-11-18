/*
 * Senales hacia los tres ESC: dos pulsos de 50 Hz por motor (acelerador y
 * sentido), generados con MCPWM igual que en los drivers originales.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "robot_config.h"

/** Configura los 3 motores y empieza a enviar pulsos de PARADA. */
esp_err_t esc_init(const robot_config_t *cfg);

/** true si esc_init() se ejecuto con exito. */
bool esc_ready(void);

/**
 * Fija la orden de un motor (idx 0..2). pct se limita a 0..CONFIG_STRIKER_HW_MAX_PCT.
 * Devuelve el ancho de pulso de acelerador aplicado (us), o -1 si hay error.
 */
int esc_set(int idx, int pct, bool reverse);

/** Pulsos de parada en los tres motores. */
void esc_stop_all(void);
