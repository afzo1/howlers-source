#include "robot_config.h"

#include <stddef.h>

#include "nvs.h"

static const robot_config_t s_s1 = {
    .id = ROBOT_S1,
    .name = "Striker 1",
    // control_main.h: PWM_GPIO_R/L y PWM_REV_GPIO_R/L. main.c: R y L en el
    // grupo MCPWM 0, B en el grupo 1.
    // B va en GPIO38/39 (cambio de cableado de 2026-11): robot-mov lo pone en
    // GPIO45/0, que son de strapping, y el robot lo tenia en GPIO35/36, como
    // bench_pid_autotuning, pero en la N16R8 esos pines son de la PSRAM octal.
    .motor = {
        {.name = "derecho (R)",   .pwm_gpio = 20, .rev_gpio = 21, .mcpwm_group = 0},
        {.name = "izquierdo (L)", .pwm_gpio = 47, .rev_gpio = 48, .mcpwm_group = 0},
        {.name = "trasero (B)",   .pwm_gpio = 38, .rev_gpio = 39, .mcpwm_group = 1},
    },
    // control_main.h: AS5600_OUT_GPIO_RIGHT/LEFT/BACK
    .enc_gpio = {5, 6, 7},
    // bldc_pwm v1: sentido = (uint8)(10 o 90 * 0,85) + 35 milesimas de 20 ms
    .rev_forward_us = 860,
    .rev_reverse_us = 2220,
    // control_main.h: VL53L1X en SDA 41 / SCL 42
    .i2c = {
        {.desc = "VL53L1X (distancia)", .sda = 41, .scl = 42, .release_gpio = -1},
    },
    .n_i2c = 1,
    // control_main.h: TM151 por UART, TX 17 y RX 18
    .imu = IMU_TM151_UART,
    .imu_i2c_bus = -1,
    .imu_reset_gpio = -1,
    .imu_uart_tx = 17,
    .imu_uart_rx = 18,
};

static const robot_config_t s_s2 = {
    .id = ROBOT_S2,
    .name = "Striker 2",
    // types.h: MOTOR_N_MCPWM_GPIO(x) y MOTOR_N_MCPWM_REVERSE_GPIO(x); los tres
    // motores en el grupo MCPWM 1 (functions.c, init_drivers).
    .motor = {
        {.name = "gMotor[0]", .pwm_gpio = 17, .rev_gpio = 18, .mcpwm_group = 1},
        {.name = "gMotor[1]", .pwm_gpio = 8,  .rev_gpio = 3,  .mcpwm_group = 1},
        {.name = "gMotor[2]", .pwm_gpio = 46, .rev_gpio = 9,  .mcpwm_group = 1},
    },
    // types.h: AS5600_N_OUT_GPIO(x)
    .enc_gpio = {6, 7, 1},
    // bldc_pwm v2: sentido = MAP(70 o 30, 0, 100, 50, 120) milesimas de 20 ms
    .rev_forward_us = 1980,
    .rev_reverse_us = 1420,
    // types.h: BNO055 (SDA 12, SCL 11, RST 13), VL53L1X (SDA 39, SCL 38, XSHUT 41)
    // y el bus I2C de los AS5600 (SDA 5, SCL 4)
    .i2c = {
        {.desc = "BNO055 (IMU)",          .sda = 12, .scl = 11, .release_gpio = 13},
        {.desc = "VL53L1X (distancia)",   .sda = 39, .scl = 38, .release_gpio = 41},
        {.desc = "AS5600 (programacion)", .sda = 5,  .scl = 4,  .release_gpio = -1},
    },
    .n_i2c = 3,
    .imu = IMU_BNO055_I2C,
    .imu_i2c_bus = 0,
    .imu_reset_gpio = 13,
    .imu_uart_tx = -1,
    .imu_uart_rx = -1,
};

static robot_variant_t s_active = ROBOT_NONE;

const robot_config_t *robot_config_get(robot_variant_t v)
{
    switch (v) {
    case ROBOT_S1: return &s_s1;
    case ROBOT_S2: return &s_s2;
    default: return NULL;
    }
}

int robot_throttle_pulse_us(const robot_config_t *cfg, int pct)
{
    if (pct < 0) {
        pct = 0;
    }
    if (pct > 100) {
        pct = 100;
    }
    if (cfg->id == ROBOT_S1) {
        // bldc_pwm v1: mapped = (uint8)(duty * (120 - 35) / 100) + 35 milesimas
        int mapped = (int)((float)pct * 85.0f / 100.0f) + 35;
        return 20 * mapped; // 700 us (parado) .. 2400 us (100 %)
    }
    // bldc_pwm v2: mapped = (int)MAP(duty, 0, 100, 50, 120) milesimas
    int mapped = (int)((float)pct * 70.0f / 100.0f + 50.0f);
    return 20 * mapped; // 1000 us (parado) .. 2400 us (100 %)
}

robot_variant_t robot_variant_load(void)
{
    nvs_handle_t h;
    uint8_t v = ROBOT_NONE;
    if (nvs_open("hwtest", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_u8(h, "robot", &v) != ESP_OK) {
            v = ROBOT_NONE;
        }
        nvs_close(h);
    }
    if (v != ROBOT_S1 && v != ROBOT_S2) {
        v = ROBOT_NONE;
    }
    return (robot_variant_t)v;
}

bool robot_variant_save(robot_variant_t v)
{
    nvs_handle_t h;
    if (nvs_open("hwtest", NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = nvs_set_u8(h, "robot", (uint8_t)v) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

robot_variant_t robot_variant_active(void)
{
    return s_active;
}

void robot_variant_set_active(robot_variant_t v)
{
    s_active = v;
}
