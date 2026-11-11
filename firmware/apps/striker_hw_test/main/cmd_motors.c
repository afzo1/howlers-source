/*
 * Comandos de motores: armar, desarmar, motor, girar, parar.
 *
 * Cada orden de movimiento:
 *  - exige "armar" antes,
 *  - limita la potencia a CONFIG_STRIKER_HW_MAX_PCT,
 *  - dura como maximo CONFIG_STRIKER_HW_MAX_MS y vuelve sola a PARADA,
 *  - se interrumpe al presionar cualquier tecla en el monitor serie,
 *  - muestra la velocidad medida por los 3 encoders, para saber que encoder
 *    corresponde a cada motor y en que sentido gira.
 */
#include <stdio.h>
#include <string.h>

#include "cmds.h"
#include "encoders.h"
#include "esc.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define SAMPLE_MS 1   // lectura de encoders cada 1 ms (evita confundir vueltas rapidas)
#define REPORT_MS 200 // reporte de velocidad cada 200 ms

static bool s_armed = false;

bool motors_armed(void)
{
    return s_armed;
}

static bool check_ready(void)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return false;
    }
    if (!esc_ready()) {
        printf("Las salidas de los motores no estan listas (ver mensajes al arrancar).\n");
        return false;
    }
    if (!s_armed) {
        printf("Motores DESARMADOS. Con las ruedas en el aire, escribe 'armar'.\n");
        return false;
    }
    return true;
}

/*
 * Mantiene la orden durante ms milisegundos midiendo los encoders.
 * active[i] indica que motores tienen orden (solo para el resumen).
 */
static void run_and_measure(int ms, const bool active[3])
{
    bool enc = encoders_ready();
    float prev[3] = {0}, acc_win[3] = {0}, acc_tail[3] = {0};
    if (enc) {
        for (int i = 0; i < 3; i++) {
            prev[i] = encoder_angle_deg(i, NULL);
        }
    }
    int64_t t0 = esp_timer_get_time();
    int64_t last_report = t0;
    int64_t tail_start = t0 + (int64_t)ms * 1000 / 4; // resumen sin el primer cuarto (arranque)
    bool aborted = false;

    while (true) {
        int64_t now = esp_timer_get_time();
        if (now - t0 >= (int64_t)ms * 1000) {
            break;
        }
        if (console_key_pressed()) {
            aborted = true;
            break;
        }
        if (enc) {
            for (int i = 0; i < 3; i++) {
                float a = encoder_angle_deg(i, NULL);
                if (a < 0 || prev[i] < 0) {
                    prev[i] = a;
                    continue;
                }
                float d = a - prev[i];
                if (d > 180.0f) d -= 360.0f;   // la lectura paso de 359 a 0 grados
                if (d < -180.0f) d += 360.0f;  // la lectura paso de 0 a 359 grados
                acc_win[i] += d;
                if (now >= tail_start) {
                    acc_tail[i] += d;
                }
                prev[i] = a;
            }
            if (now - last_report >= REPORT_MS * 1000) {
                float win_s = (float)(now - last_report) / 1e6f;
                printf("t=%4.1f s ", (float)(now - t0) / 1e6f);
                for (int i = 0; i < 3; i++) {
                    printf("  E%d %+7.1f rpm", i + 1, acc_win[i] / 360.0f / win_s * 60.0f);
                    acc_win[i] = 0;
                }
                printf("\n");
                last_report = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
    esc_stop_all();
    int64_t t_end = esp_timer_get_time();
    printf("%s Motores en PARADA.\n", aborted ? "Detenido por el usuario." : "Tiempo cumplido.");

    if (enc && t_end > tail_start) {
        float tail_s = (float)(t_end - tail_start) / 1e6f;
        printf("Velocidad media (sin el arranque):");
        for (int i = 0; i < 3; i++) {
            printf("  E%d %+7.1f rpm", i + 1, acc_tail[i] / 360.0f / tail_s * 60.0f);
        }
        printf("\n");
        (void)active;
    }
}

static int cmd_armar(int argc, char **argv)
{
    const robot_config_t *cfg = require_robot();
    if (!cfg) {
        return 1;
    }
    if (!esc_ready()) {
        printf("Las salidas de los motores no estan listas.\n");
        return 1;
    }
    esc_stop_all();
    s_armed = true;
    printf("Motores ARMADOS. Antes de cualquier orden:\n");
    printf(" - las tres ruedas deben estar en el aire, sin tocar nada;\n");
    printf(" - manos, cables y ropa lejos de las ruedas;\n");
    printf(" - ten a mano el conector de la bateria para desconectarlo.\n");
    printf("Potencia maxima %d %%, duracion maxima %d ms. 'desarmar' para bloquear.\n",
           CONFIG_STRIKER_HW_MAX_PCT, CONFIG_STRIKER_HW_MAX_MS);
    return 0;
}

static int cmd_desarmar(int argc, char **argv)
{
    esc_stop_all();
    s_armed = false;
    printf("Motores en PARADA y DESARMADOS.\n");
    return 0;
}

static int cmd_parar(int argc, char **argv)
{
    esc_stop_all();
    printf("Motores en PARADA.\n");
    return 0;
}

/* Lee [ms] [rev] desde argv[first] en adelante. */
static bool parse_tail(int argc, char **argv, int first, int *ms, bool *rev)
{
    *ms = 1500;
    *rev = false;
    for (int i = first; i < argc; i++) {
        if (strcmp(argv[i], "rev") == 0) {
            *rev = true;
        } else if (!parse_int(argv[i], ms)) {
            return false;
        }
    }
    if (*ms < 100) {
        *ms = 100;
    }
    if (*ms > CONFIG_STRIKER_HW_MAX_MS) {
        printf("Duracion limitada a %d ms.\n", CONFIG_STRIKER_HW_MAX_MS);
        *ms = CONFIG_STRIKER_HW_MAX_MS;
    }
    return true;
}

static bool parse_pct(const char *s, int *pct)
{
    if (!parse_int(s, pct) || *pct < 0) {
        return false;
    }
    if (*pct > CONFIG_STRIKER_HW_MAX_PCT) {
        printf("Potencia limitada a %d %%.\n", CONFIG_STRIKER_HW_MAX_PCT);
        *pct = CONFIG_STRIKER_HW_MAX_PCT;
    }
    return true;
}

static int cmd_motor(int argc, char **argv)
{
    if (!check_ready()) {
        return 1;
    }
    int n = 0, pct = 0, ms = 0;
    bool rev = false;
    if (argc < 3 || !parse_int(argv[1], &n) || n < 1 || n > 3 || !parse_pct(argv[2], &pct) ||
        !parse_tail(argc, argv, 3, &ms, &rev)) {
        printf("Uso: motor <1|2|3> <porcentaje> [milisegundos] [rev]\n");
        printf("Ejemplo: motor 1 10 1500      motor 2 15 2000 rev\n");
        return 1;
    }
    const robot_config_t *cfg = require_robot();
    console_flush_keys();
    int us = esc_set(n - 1, pct, rev);
    if (us < 0) {
        esc_stop_all();
        printf("ERROR al enviar la orden.\n");
        return 1;
    }
    printf("Motor %d (%s): %d %% (pulso %d us), %s, %d ms. Enter para detener.\n", n, cfg->motor[n - 1].name,
           pct, us, rev ? "REVERSA" : "adelante", ms);
    bool active[3] = {false, false, false};
    active[n - 1] = true;
    run_and_measure(ms, active);
    return 0;
}

static int cmd_girar(int argc, char **argv)
{
    if (!check_ready()) {
        return 1;
    }
    int pct = 0, ms = 0;
    bool rev = false;
    if (argc < 2 || !parse_pct(argv[1], &pct) || !parse_tail(argc, argv, 2, &ms, &rev)) {
        printf("Uso: girar <porcentaje> [milisegundos] [rev]\n");
        printf("Los tres motores con la misma orden: en el piso, el robot gira sobre si mismo.\n");
        return 1;
    }
    console_flush_keys();
    int us = 0;
    for (int i = 0; i < 3; i++) {
        us = esc_set(i, pct, rev);
        if (us < 0) {
            esc_stop_all();
            printf("ERROR al enviar la orden.\n");
            return 1;
        }
    }
    printf("Los 3 motores: %d %% (pulso %d us), %s, %d ms. Enter para detener.\n", pct, us,
           rev ? "REVERSA" : "adelante", ms);
    const bool active[3] = {true, true, true};
    run_and_measure(ms, active);
    return 0;
}

void register_motor_cmds(void)
{
    const esp_console_cmd_t cmds[] = {
        {.command = "armar", .help = "Habilita las ordenes de motor (ruedas en el aire)", .func = &cmd_armar},
        {.command = "desarmar", .help = "Bloquea las ordenes de motor y los deja en parada", .func = &cmd_desarmar},
        {.command = "parar", .help = "Pone los 3 motores en parada", .func = &cmd_parar},
        {.command = "motor", .help = "Mueve UN motor: motor <1|2|3> <porcentaje> [ms] [rev]",
         .hint = "<1|2|3> <porcentaje> [ms] [rev]", .func = &cmd_motor},
        {.command = "girar", .help = "Mueve los 3 motores con la misma orden: girar <porcentaje> [ms] [rev]",
         .hint = "<porcentaje> [ms] [rev]", .func = &cmd_girar},
    };
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
}
