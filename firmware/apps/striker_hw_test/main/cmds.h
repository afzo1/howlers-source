#pragma once

#include <stdbool.h>

#include "robot_config.h"

void register_basic_cmds(void);
void register_sensor_cmds(void);
void register_motor_cmds(void);

/** true si el usuario presiono una tecla en el monitor serie (no bloquea). */
bool console_key_pressed(void);

/** Descarta las teclas pendientes. */
void console_flush_keys(void);

/** Configuracion del robot activo, o NULL con un mensaje si no hay robot elegido. */
const robot_config_t *require_robot(void);

/** Convierte un argumento a entero; devuelve false si no es un numero. */
bool parse_int(const char *s, int *out);
