/*
 * Striker v3 - consola de depuracion por UART0 (conector COM).
 *
 * Comandos: estado, stats, stats reset, hist, pin_medida, carga_ctrl, conteo.
 * Ninguno mueve motores. La consola corre en el Core 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "consola.h"
#include "medicion.h"
#include "tareas.h"

#define MAX_TAREAS_SISTEMA 40

static medidor_t *const MEDIDORES[] = {
    &g_med_lat_isr, &g_med_activacion, &g_med_periodo,
    &g_med_duracion, &g_med_cap_periodo, &g_med_cap_ancho,
};
#define NUM_MEDIDORES ((int)(sizeof(MEDIDORES) / sizeof(MEDIDORES[0])))

static medidor_t s_copia; /* grande para la pila: se deja estatica */
static TaskStatus_t s_carga_a[MAX_TAREAS_SISTEMA];
static TaskStatus_t s_carga_b[MAX_TAREAS_SISTEMA];
static UBaseType_t s_carga_na;
static configRUN_TIME_COUNTER_TYPE s_carga_total_a;

/* ---------------------------------------------------------------------- */

static void imprimir_medidor(const medidor_t *m)
{
    med_copiar(m, &s_copia);
    const medidor_t *c = &s_copia;
    if (c->n == 0) {
        printf("%-12s %9s\n", c->nombre, "sin datos");
        return;
    }
    float tpu = (float)c->ticks_por_us;
    float min = (float)c->min / tpu;
    float max = (float)c->max / tpu;
    float prom = (float)((double)c->suma / (double)c->n) / tpu;

    /* Percentil 99,9: borde superior de la casilla donde el acumulado llega al 99,9 %. */
    uint32_t objetivo = c->n - c->n / 1000;
    uint32_t acumulado = c->bajo;
    int casilla = MED_CASILLAS;
    for (int i = 0; i <= MED_CASILLAS; i++) {
        acumulado += c->hist[i];
        if (acumulado >= objetivo) {
            casilla = i;
            break;
        }
    }
    char p999[16];
    if (casilla >= MED_CASILLAS) {
        snprintf(p999, sizeof(p999), ">%ld", (long)(c->base_us + MED_CASILLAS));
    } else {
        snprintf(p999, sizeof(p999), "<=%ld", (long)(c->base_us + casilla + 1));
    }
    printf("%-12s %9lu %9.2f %9.2f %9.2f %9s %9.2f\n", c->nombre, (unsigned long)c->n, min, prom, max, p999, max - min);
}

static void carga_marcar(void)
{
    s_carga_na = uxTaskGetSystemState(s_carga_a, MAX_TAREAS_SISTEMA, &s_carga_total_a);
}

static void carga_imprimir(void)
{
    configRUN_TIME_COUNTER_TYPE total_b = 0;
    UBaseType_t nb = uxTaskGetSystemState(s_carga_b, MAX_TAREAS_SISTEMA, &total_b);
    if (s_carga_na == 0 || total_b <= s_carga_total_a) {
        printf("\nCarga de CPU: sin datos (corra 'stats reset' y espere).\n");
        return;
    }
    double dt = (double)(total_b - s_carga_total_a);
    double idle0 = 0.0, idle1 = 0.0, suma = 0.0;
    printf("\nCarga de CPU desde el ultimo 'stats reset' (%% del tiempo de un nucleo; se omiten las de 0 %%):\n");
    for (UBaseType_t i = 0; i < nb; i++) {
        configRUN_TIME_COUNTER_TYPE antes = 0;
        for (UBaseType_t j = 0; j < s_carga_na; j++) {
            if (s_carga_a[j].xHandle == s_carga_b[i].xHandle) {
                antes = s_carga_a[j].ulRunTimeCounter;
                break;
            }
        }
        double pct = 100.0 * (double)(s_carga_b[i].ulRunTimeCounter - antes) / dt;
        suma += pct;
        if (strcmp(s_carga_b[i].pcTaskName, "IDLE0") == 0) {
            idle0 = pct;
        } else if (strcmp(s_carga_b[i].pcTaskName, "IDLE1") == 0) {
            idle1 = pct;
        }
        if (pct >= 0.005) {
            printf("  %-16s %7.2f %%\n", s_carga_b[i].pcTaskName, pct);
        }
    }
    printf("  Carga del Core 0: %.2f %%   Carga del Core 1: %.2f %%   (suma de todas: %.0f %%, debe dar ~200 %%)\n",
           100.0 - idle0, 100.0 - idle1, suma);
}

/* ---------------------------------------------------------------------- */

static int cmd_estado(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int pin = med_pin_actual();
    printf("Striker v3 (esqueleto)\n");
    printf("  modo: Idle (ningun pin de motor configurado)\n");
    printf("  encendido hace: %.1f s\n", (double)esp_timer_get_time() / 1e6);
    printf("  activaciones de T_ctrl: %lu (perdidas: %lu)\n", (unsigned long)g_activaciones, (unsigned long)g_perdidas);
    if (pin >= 0) {
        printf("  pin de medida: GPIO %d\n", pin);
    } else {
        printf("  pin de medida: apagado\n");
    }
    printf("  carga_ctrl: %lu us\n", (unsigned long)(g_carga_ciclos / esp_rom_get_cpu_ticks_per_us()));
    printf("  conteo: %s\n", g_conteo_activo ? "activo" : "apagado");
    return 0;
}

static int cmd_stats(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "reset") == 0) {
        carga_marcar();
        vTaskDelay(pdMS_TO_TICKS(5)); /* lo de arriba no debe caer en las medidas nuevas */
        med_reiniciar_todo();
        printf("Medidas reiniciadas.\n");
        return 0;
    }

    /* Primero se copian las medidas; despues lo que puede perturbar un poco. */
    printf("\nMedidas en us desde el ultimo 'stats reset' (p99.9: el 99,9 %% de las muestras queda por debajo):\n");
    printf("%-12s %9s %9s %9s %9s %9s %9s\n", "medida", "n", "min", "prom", "max", "p99.9", "max-min");
    for (int i = 0; i < NUM_MEDIDORES; i++) {
        imprimir_medidor(MEDIDORES[i]);
    }
    printf("Activaciones de T_ctrl desde el arranque: %lu, perdidas: %lu\n",
           (unsigned long)g_activaciones, (unsigned long)g_perdidas);

    printf("\nTareas periodicas (us; ejec = tiempo de CPU, resp = desde que debia activarse hasta que termina):\n");
    printf("%-13s %3s %4s %7s %9s %9s %9s %9s\n", "tarea", "nuc", "prio", "periodo", "n", "ejec_prom", "ejec_max", "resp_max");
    for (int i = 0; i < tareas_cantidad(); i++) {
        tarea_info_t *t = tareas_info(i);
        if (i == 0) {
            printf("%-13s %3ld %4lu %7s %9s\n", t->nombre, (long)t->nucleo, (unsigned long)t->prioridad, "1 ms",
                   "ver 'duracion' y 'activacion'");
            continue;
        }
        if (t->periodo_ms == 0) {
            printf("%-13s %3ld %4lu %7s\n", t->nombre, (long)t->nucleo, (unsigned long)t->prioridad, "eventos");
            continue;
        }
        med_copiar(&t->ejec, &s_copia);
        uint32_t n = s_copia.n;
        float ejec_prom = n ? (float)((double)s_copia.suma / n) / 240.0f : 0.0f;
        float ejec_max = n ? (float)s_copia.max / 240.0f : 0.0f;
        med_copiar(&t->resp, &s_copia);
        char periodo[12];
        snprintf(periodo, sizeof(periodo), "%lu ms", (unsigned long)t->periodo_ms);
        printf("%-13s %3ld %4lu %7s %9lu %9.2f %9.2f %9lu\n", t->nombre, (long)t->nucleo, (unsigned long)t->prioridad,
               periodo, (unsigned long)n, ejec_prom, ejec_max, (unsigned long)(s_copia.n ? s_copia.max : 0));
    }

    carga_imprimir();

    printf("\nPila libre (minimo desde el arranque):\n");
    for (int i = 0; i < tareas_cantidad(); i++) {
        tarea_info_t *t = tareas_info(i);
        UBaseType_t libre = uxTaskGetStackHighWaterMark(t->handle); /* en bytes en ESP-IDF */
        printf("  %-13s %5lu de %5lu B (%3lu %%)\n", t->nombre, (unsigned long)libre, (unsigned long)t->pila,
               (unsigned long)(100u * libre / t->pila));
    }
    printf("\nMemoria interna libre: %u B (minimo desde el arranque: %u B). PSRAM libre: %u B.\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return 0;
}

static int cmd_hist(int argc, char **argv)
{
    const medidor_t *m = NULL;
    if (argc > 1) {
        for (int i = 0; i < NUM_MEDIDORES; i++) {
            if (strcmp(argv[1], MEDIDORES[i]->nombre) == 0) {
                m = MEDIDORES[i];
            }
        }
    }
    if (m == NULL) {
        printf("Uso: hist <medida>. Medidas:");
        for (int i = 0; i < NUM_MEDIDORES; i++) {
            printf(" %s", MEDIDORES[i]->nombre);
        }
        printf("\n");
        return 1;
    }
    med_copiar(m, &s_copia);
    printf("# %s: %s\n# %lu muestras. Casillas de 1 us (borde inferior). CSV:\nus,muestras\n",
           s_copia.nombre, s_copia.descripcion, (unsigned long)s_copia.n);
    if (s_copia.bajo) {
        printf("<%ld,%lu\n", (long)s_copia.base_us, (unsigned long)s_copia.bajo);
    }
    for (int i = 0; i < MED_CASILLAS; i++) {
        if (s_copia.hist[i]) {
            printf("%ld,%lu\n", (long)(s_copia.base_us + i), (unsigned long)s_copia.hist[i]);
        }
    }
    if (s_copia.hist[MED_CASILLAS]) {
        printf(">=%ld,%lu\n", (long)(s_copia.base_us + MED_CASILLAS), (unsigned long)s_copia.hist[MED_CASILLAS]);
    }
    return 0;
}

static int cmd_pin_medida(int argc, char **argv)
{
    if (argc < 2) {
        printf("Uso: pin_medida <9|10|11|12|13|14|off>. Ahora: %d (-1 = apagado)\n", med_pin_actual());
        return 0;
    }
    if (strcmp(argv[1], "off") == 0) {
        med_pin_configurar(-1);
        printf("Pin de medida apagado.\n");
        return 0;
    }
    int pin = atoi(argv[1]);
    if (!med_pin_configurar(pin)) {
        printf("No se pudo usar GPIO %d. Pines permitidos: 9 a 14 (libres en Striker 1 y Striker 2).\n", pin);
        return 1;
    }
    printf("Pin de medida en GPIO %d: T_ctrl lo sube al empezar y lo baja al terminar.\n", pin);
    printf("La captura MCPWM llena cap_periodo y cap_ancho. En la placa del robot, confirmar antes\n");
    printf("que no haya nada conectado a ese pin.\n");
    return 0;
}

static int cmd_carga(int argc, char **argv)
{
    uint32_t mhz = esp_rom_get_cpu_ticks_per_us();
    if (argc < 2) {
        printf("Uso: carga_ctrl <0..200> (us de trabajo simulado en T_ctrl). Ahora: %lu us\n",
               (unsigned long)(g_carga_ciclos / mhz));
        return 0;
    }
    int us = atoi(argv[1]);
    if (us < 0 || us > 200) {
        printf("Fuera de rango: de 0 a 200 us.\n");
        return 1;
    }
    g_carga_ciclos = (uint32_t)us * mhz;
    printf("T_ctrl simula %d us de trabajo por activacion.\n", us);
    return 0;
}

static int cmd_conteo(int argc, char **argv)
{
    if (argc < 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
        printf("Uso: conteo <on|off>. Con 'on', cada 10 s imprime: conteo <activaciones> <us desde el arranque>\n");
        return 1;
    }
    g_conteo_activo = (strcmp(argv[1], "on") == 0);
    return 0;
}

void consola_iniciar(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = "v3>";
    cfg.max_cmdline_length = 128;
    cfg.task_stack_size = 6144;
    cfg.task_core_id = 0; /* el Core 1 queda para el tiempo real */
    esp_console_dev_uart_config_t uart = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart, &cfg, &repl));

    const esp_console_cmd_t comandos[] = {
        {.command = "estado", .help = "Resumen: modo, activaciones, pin de medida y carga", .func = cmd_estado},
        {.command = "stats", .help = "Medidas de tiempo, carga, pilas y memoria. 'stats reset' las reinicia", .func = cmd_stats},
        {.command = "hist", .help = "Histograma de una medida en CSV: hist <medida>", .func = cmd_hist},
        {.command = "pin_medida", .help = "Pin para la captura MCPWM: pin_medida <9..14|off>", .func = cmd_pin_medida},
        {.command = "carga_ctrl", .help = "Trabajo simulado en T_ctrl: carga_ctrl <0..200 us>", .func = cmd_carga},
        {.command = "conteo", .help = "Imprime activaciones cada 10 s: conteo <on|off>", .func = cmd_conteo},
    };
    for (size_t i = 0; i < sizeof(comandos) / sizeof(comandos[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&comandos[i]));
    }
    esp_console_register_help_command();
    carga_marcar();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
