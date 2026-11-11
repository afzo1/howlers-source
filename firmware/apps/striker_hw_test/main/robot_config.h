/*
 * Configuracion de pines y senales de cada robot.
 *
 * Todos los valores se tomaron del firmware original de cada robot, para que
 * este programa use cada pin exactamente en el mismo papel que ya tenia:
 *  - Striker 1: firmware/apps/striker_v1/robot-mov (control_main.h, main.c) y
 *    components/actuators/bldc_pwm/v1_striker1.
 *  - Striker 2: firmware/apps/striker_v2/Robot-Control (include/types.h) y
 *    components/actuators/bldc_pwm/v2_striker2.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ROBOT_NONE = 0, // sin seleccionar: no se configura ningun pin de salida
    ROBOT_S1 = 1,
    ROBOT_S2 = 2,
} robot_variant_t;

typedef enum {
    IMU_NONE = 0,
    IMU_BNO055_I2C, // Striker 2
    IMU_TM151_UART, // Striker 1
} imu_type_t;

typedef struct {
    const char *name;  // nombre para mostrar
    int pwm_gpio;      // senal de acelerador hacia el ESC
    int rev_gpio;      // senal de sentido de giro hacia el ESC
    int mcpwm_group;   // grupo MCPWM (igual que en el firmware original)
} motor_cfg_t;

typedef struct {
    const char *desc;
    int sda;
    int scl;
    int release_gpio;  // pin de reset/apagado del sensor que se pone en alto; -1 si no hay
} i2c_bus_cfg_t;

typedef struct {
    robot_variant_t id;
    const char *name;
    motor_cfg_t motor[3];
    int enc_gpio[3];       // salida analogica de cada AS5600 (ADC1)
    // Modelo de pulsos del ESC (microsegundos, periodo de 20 ms = 50 Hz)
    int rev_forward_us;    // pulso en la linea de sentido para "adelante"
    int rev_reverse_us;    // pulso en la linea de sentido para "reversa"
    // Buses I2C que el firmware original usa como I2C
    i2c_bus_cfg_t i2c[3];
    int n_i2c;
    // IMU
    imu_type_t imu;
    int imu_i2c_bus;       // indice en i2c[] (BNO055)
    int imu_reset_gpio;    // nRESET del BNO055 (activo en bajo)
    int imu_uart_tx;       // TX del ESP32 hacia la IMU (TM151)
    int imu_uart_rx;       // RX del ESP32 desde la IMU (TM151)
} robot_config_t;

/** Configuracion del robot elegido, o NULL si no hay ninguno. */
const robot_config_t *robot_config_get(robot_variant_t v);

/** Ancho del pulso de acelerador (us) para un porcentaje 0..100, igual que el driver original. */
int robot_throttle_pulse_us(const robot_config_t *cfg, int pct);

/** Variante guardada en NVS (ROBOT_NONE si no hay). */
robot_variant_t robot_variant_load(void);

/** Guarda la variante en NVS. */
bool robot_variant_save(robot_variant_t v);

/** Variante activa en esta ejecucion. */
robot_variant_t robot_variant_active(void);
void robot_variant_set_active(robot_variant_t v);
