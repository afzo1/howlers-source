/*
 * striker_hw_test: firmware de pruebas de hardware para Striker 1 y Striker 2.
 *
 * Reglas de seguridad de este programa:
 *  1. Sin robot elegido ("robot ninguno") no configura ningun pin como salida.
 *  2. Con un robot elegido usa cada pin en el mismo papel que el firmware
 *     original de ese robot, y al arrancar solo envia pulsos de PARADA a los ESC.
 *  3. Los motores solo se mueven con un comando explicito, despues de "armar",
 *     con potencia limitada (CONFIG_STRIKER_HW_MAX_PCT) y duracion limitada
 *     (CONFIG_STRIKER_HW_MAX_MS). Al terminar vuelven solos a PARADA.
 *
 * Instrucciones de uso: docs/striker_v3/guia_pruebas_robot.md
 */
#include <stdio.h>

#include "cmds.h"
#include "encoders.h"
#include "esc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "robot_config.h"

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        printf("Aviso: NVS no disponible (%s); no se recordara el robot elegido.\n", esp_err_to_name(err));
    }
}

void app_main(void)
{
    // Menos mensajes automaticos para que la consola se lea mejor
    esp_log_level_set("*", ESP_LOG_WARN);

    init_nvs();
    robot_variant_t v = robot_variant_load();
    robot_variant_set_active(v);
    const robot_config_t *cfg = robot_config_get(v);

    printf("\n==============================================================\n");
    printf(" Striker - firmware de pruebas de hardware\n");
    printf("==============================================================\n");

    if (cfg == NULL) {
        printf(" Robot: NINGUNO. No se configura ningun pin de salida.\n");
        printf(" 1) Usa 'identificar' para ayudarte a saber si es Striker 1 o 2.\n");
        printf(" 2) Luego 'robot s1' o 'robot s2' (se guarda y reinicia).\n");
    } else {
        printf(" Robot: %s\n", cfg->name);
        if (esc_init(cfg) == ESP_OK) {
            printf(" ESC: enviando pulsos de PARADA a los 3 motores.\n");
        }
        if (encoders_init(cfg->enc_gpio) != ESP_OK) {
            printf(" Aviso: los encoders no quedaron listos.\n");
        }
        printf(" Motores DESARMADOS. Para moverlos: ruedas en el aire y 'armar'.\n");
        printf(" Potencia maxima: %d %%. Duracion maxima: %d ms.\n",
               CONFIG_STRIKER_HW_MAX_PCT, CONFIG_STRIKER_HW_MAX_MS);
    }
    printf(" Escribe 'help' para ver los comandos.\n");
    printf("==============================================================\n\n");

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "striker>";
    repl_config.max_cmdline_length = 128;
    repl_config.task_stack_size = 8192;
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    esp_console_register_help_command();
    register_basic_cmds();
    register_sensor_cmds();
    register_motor_cmds();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
