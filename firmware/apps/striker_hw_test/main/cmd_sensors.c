/*
 * Comandos de sensores: encoders, i2c, imu.
 */
#include <stdio.h>
#include <string.h>

#include "cmds.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "encoders.h"
#include "imu_tm151.h"
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------------ encoders */

static int cmd_encoders(int argc, char **argv)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return 1;
    }
    if (!encoders_ready()) {
        printf("Los encoders no estan listos (ver mensajes al arrancar).\n");
        return 1;
    }
    int secs = 20;
    if (argc >= 2 && (!parse_int(argv[1], &secs) || secs < 1 || secs > 120)) {
        printf("Uso: encoders [segundos 1..120]\n");
        return 1;
    }
    printf("Angulo de cada encoder (0..360 grados) 5 veces por segundo durante %d s.\n", secs);
    printf("Gira cada rueda con la mano. Presiona Enter para terminar.\n");
    console_flush_keys();
    for (int k = 0; k < secs * 5; k++) {
        for (int i = 0; i < 3; i++) {
            int mv = 0;
            float a = encoder_angle_deg(i, &mv);
            if (a < 0) {
                printf("E%d (GPIO%-2d):  error     ", i + 1, cfg->enc_gpio[i]);
            } else {
                printf("E%d (GPIO%-2d): %6.1f gr %4d mV   ", i + 1, cfg->enc_gpio[i], a, mv);
            }
        }
        printf("\n");
        if (console_key_pressed()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return 0;
}

/* ----------------------------------------------------------------------- i2c */

static const char *i2c_name(uint8_t addr)
{
    switch (addr) {
    case 0x28: return "BNO055 (direccion alternativa)";
    case 0x29: return "BNO055 o VL53L1X";
    case 0x36: return "AS5600";
    case 0x39: return "APDS9960";
    default: return "desconocido";
    }
}

static void release_gpio(int gpio)
{
    if (gpio < 0) {
        return;
    }
    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, 1);
}

static esp_err_t new_bus(const i2c_bus_cfg_t *b, i2c_master_bus_handle_t *bus)
{
    i2c_master_bus_config_t bcfg = {
        .i2c_port = -1,
        .sda_io_num = b->sda,
        .scl_io_num = b->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bcfg, bus);
}

static int cmd_i2c(int argc, char **argv)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return 1;
    }
    for (int b = 0; b < cfg->n_i2c; b++) {
        const i2c_bus_cfg_t *bc = &cfg->i2c[b];
        printf("\nBus %s (SDA GPIO%d, SCL GPIO%d):\n", bc->desc, bc->sda, bc->scl);
        if (bc->release_gpio >= 0) {
            release_gpio(bc->release_gpio);
            vTaskDelay(pdMS_TO_TICKS(700)); // el BNO055 tarda ~650 ms en arrancar
        }
        i2c_master_bus_handle_t bus = NULL;
        if (new_bus(bc, &bus) != ESP_OK) {
            printf("  ERROR: no se pudo crear el bus.\n");
            continue;
        }
        int found = 0;
        bool stuck = false;
        for (uint8_t addr = 0x08; addr < 0x78; addr++) {
            esp_err_t r = i2c_master_probe(bus, addr, 20);
            if (r == ESP_OK) {
                printf("  0x%02X  responde  (%s)\n", addr, i2c_name(addr));
                found++;
            } else if (r == ESP_ERR_TIMEOUT) {
                stuck = true;
                break;
            }
        }
        if (stuck) {
            printf("  El bus no responde (tiempo agotado): puede faltar alimentacion, pull-ups o un cable.\n");
        } else if (found == 0) {
            printf("  Ningun dispositivo respondio.\n");
        }
        i2c_del_master_bus(bus);
    }
    return 0;
}

/* ----------------------------------------------------------------------- imu */

#define BNO055_CHIP_ID 0x00
#define BNO055_EUL_HEADING_LSB 0x1A
#define BNO055_CALIB_STAT 0x35
#define BNO055_OPR_MODE 0x3D
#define BNO055_MODE_CONFIG 0x00
#define BNO055_MODE_NDOF 0x0C

static esp_err_t bno_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, 100);
}

static esp_err_t bno_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t n)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, n, 100);
}

static int imu_bno055(const robot_config_t *cfg, int secs)
{
    const i2c_bus_cfg_t *bc = &cfg->i2c[cfg->imu_i2c_bus];
    // Reinicio limpio del sensor por su pin nRESET (activo en bajo)
    gpio_reset_pin(cfg->imu_reset_gpio);
    gpio_set_direction(cfg->imu_reset_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(cfg->imu_reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(cfg->imu_reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(800));

    i2c_master_bus_handle_t bus = NULL;
    if (new_bus(bc, &bus) != ESP_OK) {
        printf("ERROR: no se pudo crear el bus I2C.\n");
        return 1;
    }
    uint8_t addr = 0;
    if (i2c_master_probe(bus, 0x29, 50) == ESP_OK) {
        addr = 0x29; // direccion que usa Striker 2
    } else if (i2c_master_probe(bus, 0x28, 50) == ESP_OK) {
        addr = 0x28;
    }
    if (addr == 0) {
        printf("El BNO055 no responde en 0x29 ni en 0x28. Revisa cables y alimentacion.\n");
        i2c_del_master_bus(bus);
        return 1;
    }
    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev = NULL;
    if (i2c_master_bus_add_device(bus, &dcfg, &dev) != ESP_OK) {
        i2c_del_master_bus(bus);
        return 1;
    }
    uint8_t id = 0;
    if (bno_read(dev, BNO055_CHIP_ID, &id, 1) != ESP_OK) {
        printf("ERROR: no se pudo leer el BNO055.\n");
    } else {
        printf("BNO055 en 0x%02X, identificador 0x%02X (%s).\n", addr, id,
               id == 0xA0 ? "correcto" : "INESPERADO, deberia ser 0xA0");
        bno_write(dev, BNO055_OPR_MODE, BNO055_MODE_CONFIG);
        vTaskDelay(pdMS_TO_TICKS(30));
        bno_write(dev, BNO055_OPR_MODE, BNO055_MODE_NDOF);
        vTaskDelay(pdMS_TO_TICKS(100));
        printf("Orientacion 5 veces por segundo durante %d s. Gira el robot con la mano.\n", secs);
        printf("Calibracion: 0 = sin calibrar ... 3 = calibrado (mueve el robot en forma de 8 para el magnetometro).\n");
        printf("Presiona Enter para terminar.\n");
        console_flush_keys();
        for (int k = 0; k < secs * 5; k++) {
            uint8_t e[6] = {0}, cal = 0;
            if (bno_read(dev, BNO055_EUL_HEADING_LSB, e, 6) == ESP_OK &&
                bno_read(dev, BNO055_CALIB_STAT, &cal, 1) == ESP_OK) {
                float heading = (int16_t)(e[0] | (e[1] << 8)) / 16.0f;
                float roll = (int16_t)(e[2] | (e[3] << 8)) / 16.0f;
                float pitch = (int16_t)(e[4] | (e[5] << 8)) / 16.0f;
                printf("rumbo %7.1f  alabeo %7.1f  cabeceo %7.1f   calib sis/gir/acc/mag = %d/%d/%d/%d\n",
                       heading, roll, pitch, (cal >> 6) & 3, (cal >> 4) & 3, (cal >> 2) & 3, cal & 3);
            } else {
                printf("lectura fallida\n");
            }
            if (console_key_pressed()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    i2c_master_bus_rm_device(dev);
    i2c_del_master_bus(bus);
    return 0;
}

static int imu_tm151(const robot_config_t *cfg, int secs)
{
    console_flush_keys();
    return tm151_test(cfg->imu_uart_tx, cfg->imu_uart_rx, secs, console_key_pressed);
}

static int cmd_imu(int argc, char **argv)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return 1;
    }
    int secs = 20;
    if (argc >= 2 && (!parse_int(argv[1], &secs) || secs < 1 || secs > 120)) {
        printf("Uso: imu [segundos 1..120]\n");
        return 1;
    }
    if (cfg->imu == IMU_BNO055_I2C) {
        return imu_bno055(cfg, secs);
    }
    if (cfg->imu == IMU_TM151_UART) {
        return imu_tm151(cfg, secs);
    }
    printf("Este robot no tiene IMU configurada.\n");
    return 1;
}

/* ----------------------------------------------------------------- registro */

void register_sensor_cmds(void)
{
    const esp_console_cmd_t cmds[] = {
        {.command = "encoders", .help = "Muestra el angulo de los 3 encoders (gira las ruedas con la mano)",
         .hint = "[segundos]", .func = &cmd_encoders},
        {.command = "i2c", .help = "Busca dispositivos en los buses I2C del robot", .func = &cmd_i2c},
        {.command = "imu", .help = "Prueba la IMU (BNO055 en Striker 2, TM151 en Striker 1)",
         .hint = "[segundos]", .func = &cmd_imu},
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
