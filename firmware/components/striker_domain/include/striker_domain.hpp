/*
 * striker_domain: calculo de Striker v3 (C++17, sin ESP-IDF).
 *
 * Aqui van las implementaciones de las interfaces de striker_core que no tocan
 * hardware: cinematica de 3 ruedas, PID por rueda, EKF, trayectorias y
 * seguidor de ruta. Solo float, sin memoria dinamica ni excepciones.
 * Ver docs/striker_v3/02_arquitectura_capas.md.
 */
#pragma once

#include "striker/striker.hpp"
