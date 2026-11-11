/*
 * Comandos generales: info, robot, pines, identificar, wifi.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmds.h"
#include "driver/uart.h"
#include "encoders.h"
#include "esc.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

bool motors_armed(void); // cmd_motors.c

/* ---------------------------------------------------------------- utilidades */

bool console_key_pressed(void)
{
    uint8_t c;
    return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, &c, 1, 0) > 0;
}

void console_flush_keys(void)
{
    uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
}

const robot_config_t *require_robot(void)
{
    const robot_config_t *cfg = robot_config_get(robot_variant_active());
    if (cfg == NULL) {
        printf("Primero elige el robot: 'robot s1' o 'robot s2' (usa 'identificar' si no sabes cual es).\n");
    }
    return cfg;
}

bool parse_int(const char *s, int *out)
{
    if (s == NULL || *s == '\0') {
        return false;
    }
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    *out = (int)v;
    return true;
}

static const char *reset_reason_text(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON: return "encendido normal";
    case ESP_RST_EXT: return "pin de reset externo";
    case ESP_RST_SW: return "reinicio por software";
    case ESP_RST_PANIC: return "ERROR del programa (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG (el programa se bloqueo)";
    case ESP_RST_DEEPSLEEP: return "salida de deep sleep";
    case ESP_RST_BROWNOUT: return "CAIDA DE TENSION (brownout): revisar alimentacion";
    case ESP_RST_USB: return "reinicio por USB";
    default: return "otra";
    }
}

/* ---------------------------------------------------------------------- info */

static int cmd_info(int argc, char **argv)
{
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint32_t flash = 0;
    esp_flash_get_size(NULL, &flash);
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    printf("Chip:            %s, revision v%d.%d, %d nucleos\n", CONFIG_IDF_TARGET,
           ci.revision / 100, ci.revision % 100, ci.cores);
    printf("ESP-IDF:         %s\n", esp_get_idf_version());
    printf("Flash:           %lu MB (esperado en la YD-ESP32-S3 N16R8: 16 MB)\n",
           (unsigned long)(flash / (1024 * 1024)));
    if (esp_psram_is_initialized()) {
        printf("PSRAM:           %u MB (esperado: 8 MB)\n", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    } else {
        printf("PSRAM:           NO DETECTADA (esperado: 8 MB). Ver la guia, seccion de problemas.\n");
    }
    printf("RAM interna libre: %u KB\n", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    printf("MAC WiFi:        %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("Ultimo reinicio: %s\n", reset_reason_text(esp_reset_reason()));
    const robot_config_t *cfg = robot_config_get(robot_variant_active());
    printf("Robot elegido:   %s\n", cfg ? cfg->name : "NINGUNO");
    if (cfg) {
        printf("Motores:         %s\n", motors_armed() ? "ARMADOS" : "desarmados");
    }
    return 0;
}

/* --------------------------------------------------------------------- robot */

static int cmd_robot(int argc, char **argv)
{
    if (argc != 2) {
        printf("Uso: robot s1 | robot s2 | robot ninguno\n");
        return 1;
    }
    robot_variant_t v;
    if (strcmp(argv[1], "s1") == 0) {
        v = ROBOT_S1;
    } else if (strcmp(argv[1], "s2") == 0) {
        v = ROBOT_S2;
    } else if (strcmp(argv[1], "ninguno") == 0) {
        v = ROBOT_NONE;
    } else {
        printf("Valor no valido: '%s'. Usa s1, s2 o ninguno.\n", argv[1]);
        return 1;
    }
    esc_stop_all();
    if (!robot_variant_save(v)) {
        printf("ERROR: no se pudo guardar la eleccion.\n");
        return 1;
    }
    printf("Guardado. Reiniciando para aplicar la configuracion...\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return 0;
}

/* --------------------------------------------------------------------- pines */

static const char *pin_note(int gpio)
{
    switch (gpio) {
    case 0: case 3: case 45: case 46: return "  <- pin de strapping (ver guia)";
    case 19: case 20: return "  <- linea del USB nativo";
    case 48: return "  <- compartido con el LED RGB si el puente esta soldado";
    default: return "";
    }
}

static int cmd_pines(int argc, char **argv)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return 1;
    }
    printf("Pines de %s (tomados de su firmware original):\n", cfg->name);
    for (int i = 0; i < 3; i++) {
        printf(" Motor %d %-14s acelerador GPIO%-2d%s\n", i + 1, cfg->motor[i].name,
               cfg->motor[i].pwm_gpio, pin_note(cfg->motor[i].pwm_gpio));
        printf("                         sentido    GPIO%-2d%s\n", cfg->motor[i].rev_gpio,
               pin_note(cfg->motor[i].rev_gpio));
    }
    for (int i = 0; i < 3; i++) {
        printf(" Encoder %d (AS5600 analogico)       GPIO%d\n", i + 1, cfg->enc_gpio[i]);
    }
    for (int i = 0; i < cfg->n_i2c; i++) {
        printf(" I2C %-22s SDA GPIO%d, SCL GPIO%d", cfg->i2c[i].desc, cfg->i2c[i].sda, cfg->i2c[i].scl);
        if (cfg->i2c[i].release_gpio >= 0) {
            printf(", reset/XSHUT GPIO%d", cfg->i2c[i].release_gpio);
        }
        printf("\n");
    }
    if (cfg->imu == IMU_TM151_UART) {
        printf(" IMU TM151 por UART: TX del ESP32 GPIO%d, RX del ESP32 GPIO%d\n", cfg->imu_uart_tx, cfg->imu_uart_rx);
    }
    printf(" Pulsos ESC (50 Hz): parada %d us, 100 %% = %d us; sentido: adelante %d us, reversa %d us\n",
           robot_throttle_pulse_us(cfg, 0), robot_throttle_pulse_us(cfg, 100),
           cfg->rev_forward_us, cfg->rev_reverse_us);
    return 0;
}

/* --------------------------------------------------------------- identificar */

#define ID_NPINS 4
static const int s_id_gpio[ID_NPINS] = {1, 5, 6, 7};

static const char *classify(int mn, int mx, int mean)
{
    if (mean < 150 && mx < 300) {
        return "sin senal (cerca de 0 V)";
    }
    if (mean > 3050) {
        return "en alto (3,3 V): no es un encoder";
    }
    if (mx - mn > 300) {
        return "CAMBIA al girar: probablemente un encoder";
    }
    if (mean >= 250) {
        return "tension fija en el rango de un AS5600";
    }
    return "senal baja";
}

static bool in_as5600_range(int mean)
{
    return mean >= 250 && mean <= 3050;
}

static int cmd_identificar(int argc, char **argv)
{
    int secs = 15;
    if (argc >= 2 && (!parse_int(argv[1], &secs) || secs < 3 || secs > 60)) {
        printf("Uso: identificar [segundos 3..60]\n");
        return 1;
    }
    adc_channel_t ch[ID_NPINS];
    for (int i = 0; i < ID_NPINS; i++) {
        if (adc_attach_gpio(s_id_gpio[i], &ch[i]) != ESP_OK) {
            printf("ERROR: no se pudo leer GPIO%d.\n", s_id_gpio[i]);
            return 1;
        }
    }
    printf("Prueba PASIVA (solo se mide tension, no se envia nada a los pines).\n");
    printf("Durante %d s gira con la mano cada rueda, una vuelta lenta cada una.\n", secs);
    printf("Presiona Enter para terminar antes.\n");
    console_flush_keys();

    int mn[ID_NPINS], mx[ID_NPINS];
    long long sum[ID_NPINS] = {0};
    int n = 0;
    for (int i = 0; i < ID_NPINS; i++) {
        mn[i] = 100000;
        mx[i] = -1;
    }
    int64_t samples = (int64_t)secs * 50;
    for (int64_t k = 0; k < samples; k++) {
        for (int i = 0; i < ID_NPINS; i++) {
            int mv = adc_read_mv(ch[i]);
            if (mv < 0) {
                continue;
            }
            if (mv < mn[i]) mn[i] = mv;
            if (mv > mx[i]) mx[i] = mv;
            sum[i] += mv;
        }
        n++;
        if (k % 50 == 0) {
            printf(".");
            fflush(stdout);
        }
        if (console_key_pressed()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    printf("\n\n GPIO   min mV   max mV   media mV   lectura\n");
    int mean[ID_NPINS];
    for (int i = 0; i < ID_NPINS; i++) {
        mean[i] = n ? (int)(sum[i] / n) : 0;
        printf(" %4d   %6d   %6d   %8d   %s\n", s_id_gpio[i], mn[i], mx[i], mean[i], classify(mn[i], mx[i], mean[i]));
    }
    // GPIO1 es encoder solo en Striker 2; GPIO5 es encoder solo en Striker 1.
    bool g1 = in_as5600_range(mean[0]);
    bool g5 = in_as5600_range(mean[1]);
    printf("\nOrientacion (confirmar con la inspeccion visual de la guia):\n");
    if (g5 && !g1) {
        printf(" -> Parece STRIKER 1 (encoders en GPIO5, 6 y 7).\n");
    } else if (g1 && !g5) {
        printf(" -> Parece STRIKER 2 (encoders en GPIO6, 7 y 1).\n");
    } else {
        printf(" -> No concluyente. Usa la inspeccion visual (IMU y cables) para decidir.\n");
    }
    return 0;
}

/* ---------------------------------------------------------------------- wifi */

static int cmd_wifi(int argc, char **argv)
{
    static bool started = false;
    if (!started) {
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        if (esp_wifi_init(&wcfg) != ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
            esp_wifi_start() != ESP_OK) {
            printf("ERROR: no se pudo iniciar el WiFi.\n");
            return 1;
        }
        started = true;
    }
    printf("Buscando redes WiFi (unos 3 s)...\n");
    if (esp_wifi_scan_start(NULL, true) != ESP_OK) {
        printf("ERROR: fallo la busqueda.\n");
        return 1;
    }
    static wifi_ap_record_t recs[20];
    uint16_t num = 20;
    esp_wifi_scan_get_ap_records(&num, recs);
    if (num == 0) {
        printf("No se encontro ninguna red. Si hay redes cerca, revisa la antena del modulo.\n");
        return 0;
    }
    printf(" Senal(dBm)  Canal  Red\n");
    for (int i = 0; i < num; i++) {
        printf(" %10d  %5d  %s\n", recs[i].rssi, recs[i].primary, (const char *)recs[i].ssid);
    }
    printf("(Mas cerca de 0 es mejor: -50 excelente, -70 aceptable, -85 debil.)\n");
    return 0;
}

/* ----------------------------------------------------------------- registro */

void register_basic_cmds(void)
{
    const esp_console_cmd_t cmds[] = {
        {.command = "info", .help = "Datos del chip, memoria, ultimo reinicio y robot elegido", .func = &cmd_info},
        {.command = "robot", .help = "Elige el robot: 'robot s1', 'robot s2' o 'robot ninguno' (reinicia)",
         .hint = "s1|s2|ninguno", .func = &cmd_robot},
        {.command = "pines", .help = "Muestra los pines que usa el robot elegido", .func = &cmd_pines},
        {.command = "identificar", .help = "Prueba pasiva para saber si el robot es Striker 1 o 2",
         .hint = "[segundos]", .func = &cmd_identificar},
        {.command = "wifi", .help = "Busca redes WiFi para comprobar que el radio funciona", .func = &cmd_wifi},
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
