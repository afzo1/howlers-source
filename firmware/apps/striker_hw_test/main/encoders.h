/*
 * Lectura de tensiones con el ADC1 (salida analogica de los AS5600 y prueba
 * pasiva de pines para identificar el robot).
 */
#pragma once

#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

/** Inicializa el ADC1 y su calibracion (una sola vez). */
esp_err_t adc_init_once(void);

/** Configura un GPIO como entrada del ADC1. Solo lee: no aplica tension al pin. */
esp_err_t adc_attach_gpio(int gpio, adc_channel_t *out_channel);

/** Tension en mV del canal, o -1 si la lectura fallo (no reinicia el chip). */
int adc_read_mv(adc_channel_t ch);

/** Angulo (0..360 grados) a partir de la tension del AS5600 (10 %..90 % de 3,3 V). */
float as5600_angle_from_mv(int mv);

/** Prepara los tres encoders del robot activo. */
esp_err_t encoders_init(const int gpio[3]);

/** Angulo en grados del encoder idx (0..2), o -1 si fallo la lectura. */
float encoder_angle_deg(int idx, int *out_mv);

/** true si encoders_init() se ejecuto con exito. */
bool encoders_ready(void);
