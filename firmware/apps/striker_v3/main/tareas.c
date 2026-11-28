/*
 * Striker v3 - tareas de FreeRTOS.
 *
 * Nucleos, prioridades y periodos de la Entrega 1 (seccion 3):
 *   Core 1: T_ctrl 22 (1 ms, la despierta el gptimer), T_imu 21 (10 ms),
 *           T_nav 20 (10 ms, desfasada 6 ms), T_range 18 (50 ms).
 *   Core 0: T_supervisor 19 (20 ms), T_net 15 (eventos), T_telemetry 12
 *           (20 ms), T_autotune 10 (eventos), T_storage 3 (eventos).
 * Por ahora las tareas no hacen trabajo propio: solo se miden. Ninguna toca
 * los pines de los motores.
 */
#include <stdio.h>

#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

#include "tareas.h"

#define GPTIMER_HZ     10000000u /* 10 MHz: 0,1 us por cuenta */
#define GPTIMER_ALARMA 10000u    /* 10 000 cuentas = 1 ms */
#define BIT_ARRANQUE   BIT0      /* barrera de arranque */
#define CONTEO_US      10000000  /* comando "conteo": una linea cada 10 s */

enum { T_CTRL, T_IMU, T_NAV, T_RANGE, T_SUPERVISOR, T_NET, T_TELEMETRY, T_AUTOTUNE, T_STORAGE, NUM_TAREAS };

#define PILA_CTRL  4096
#define PILA_IMU   4096
#define PILA_NAV   6144
#define PILA_RANGE 3072
#define PILA_SUP   4096
#define PILA_NET   4096
#define PILA_TEL   4096
#define PILA_AUTO  4096
#define PILA_STO   3072

#define TAREA(nom, nuc, prio, per, desf, pila_b)                                                         \
    {                                                                                                    \
        .nombre = (nom), .nucleo = (nuc), .prioridad = (prio), .periodo_ms = (per),                      \
        .desfase_ms = (desf), .pila = (pila_b),                                                          \
        .ejec = MEDIDOR(nom, "ciclos de CPU por activacion", 240, 0),                                    \
        .resp = MEDIDOR(nom, "us desde la liberacion hasta terminar", 1, 0),                             \
    }

static tarea_info_t s_tareas[NUM_TAREAS] = {
    [T_CTRL] = TAREA("T_ctrl", 1, 22, 1, 0, PILA_CTRL),
    [T_IMU] = TAREA("T_imu", 1, 21, 10, 0, PILA_IMU),
    [T_NAV] = TAREA("T_nav", 1, 20, 10, 6, PILA_NAV),
    [T_RANGE] = TAREA("T_range", 1, 18, 50, 0, PILA_RANGE),
    [T_SUPERVISOR] = TAREA("T_supervisor", 0, 19, 20, 0, PILA_SUP),
    [T_NET] = TAREA("T_net", 0, 15, 0, 0, PILA_NET),
    [T_TELEMETRY] = TAREA("T_telemetry", 0, 12, 20, 0, PILA_TEL),
    [T_AUTOTUNE] = TAREA("T_autotune", 0, 10, 0, 0, PILA_AUTO),
    [T_STORAGE] = TAREA("T_storage", 0, 3, 0, 0, PILA_STO),
};

static StackType_t s_pila_ctrl[PILA_CTRL];
static StackType_t s_pila_imu[PILA_IMU];
static StackType_t s_pila_nav[PILA_NAV];
static StackType_t s_pila_range[PILA_RANGE];
static StackType_t s_pila_sup[PILA_SUP];
static StackType_t s_pila_net[PILA_NET];
static StackType_t s_pila_tel[PILA_TEL];
static StackType_t s_pila_auto[PILA_AUTO];
static StackType_t s_pila_sto[PILA_STO];
static StackType_t *const s_pilas[NUM_TAREAS] = {
    s_pila_ctrl, s_pila_imu, s_pila_nav, s_pila_range, s_pila_sup,
    s_pila_net, s_pila_tel, s_pila_auto, s_pila_sto,
};
static StaticTask_t s_tcb[NUM_TAREAS];

static StaticEventGroup_t s_eventos_buf;
static EventGroupHandle_t s_eventos;

static gptimer_handle_t s_timer;
static TaskHandle_t s_handle_ctrl;
static volatile uint32_t s_isr_cuenta; /* contador del gptimer al entrar a la ISR */
static volatile uint32_t s_isr_ciclos; /* contador de ciclos al entrar a la ISR */
static int64_t s_origen_us;            /* instante (us de esp_timer) del tick 0 */

volatile bool g_conteo_activo;

/* ISR del gptimer (Core 1). Como el contador vuelve a 0 en cada alarma, su
 * valor aqui es el tiempo desde la alarma hasta que entro la ISR. */
static bool IRAM_ATTR al_alarma(gptimer_handle_t timer, const gptimer_alarm_event_data_t *ev, void *ctx)
{
    (void)ev;
    (void)ctx;
    uint64_t cuenta = 0;
    gptimer_get_raw_count(timer, &cuenta);
    s_isr_ciclos = esp_cpu_get_cycle_count();
    s_isr_cuenta = (uint32_t)cuenta;
    BaseType_t despertar = pdFALSE;
    vTaskNotifyGiveFromISR(s_handle_ctrl, &despertar);
    return despertar == pdTRUE;
}

/* Lazo de T_ctrl, en IRAM (noinline: si se integra en tarea_ctrl, quedaria en
 * flash). Nunca se bloquea salvo esperando al gptimer. */
static void IRAM_ATTR __attribute__((noinline)) lazo_ctrl(void)
{
    uint32_t ciclos_isr_previo = 0;
    bool hay_previo = false;

    for (;;) {
        /* Devuelve cuantos avisos llegaron: mas de 1 = activaciones perdidas. */
        uint32_t avisos = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint64_t cuenta = 0;
        gptimer_get_raw_count(s_timer, &cuenta);
        uint32_t c0 = esp_cpu_get_cycle_count();
        uint32_t mascara = g_pin_mascara;
        if (mascara) {
            REG_WRITE(GPIO_OUT_W1TS_REG, mascara);
        }

        /* Trabajo del lazo (encoders, PID y ESC): llega con la Fase 4. Por
         * ahora solo la carga sintetica opcional del comando carga_ctrl. */
        uint32_t carga = g_carga_ciclos;
        if (carga) {
            while ((esp_cpu_get_cycle_count() - c0) < carga) {
            }
        }

        if (mascara) {
            REG_WRITE(GPIO_OUT_W1TC_REG, mascara);
        }
        uint32_t c1 = esp_cpu_get_cycle_count();

        uint32_t isr_cuenta = s_isr_cuenta;
        uint32_t isr_ciclos = s_isr_ciclos;
        g_activaciones++;
        if (avisos > 1) {
            g_perdidas += avisos - 1;
            hay_previo = false;
        }
        med_registrar(&g_med_lat_isr, isr_cuenta);
        med_registrar(&g_med_activacion, (uint32_t)cuenta);
        if (hay_previo) {
            med_registrar(&g_med_periodo, isr_ciclos - ciclos_isr_previo);
        }
        ciclos_isr_previo = isr_ciclos;
        hay_previo = true;
        med_registrar(&g_med_duracion, c1 - c0);
    }
}

static void tarea_ctrl(void *arg)
{
    (void)arg;
    s_handle_ctrl = xTaskGetCurrentTaskHandle(); /* antes de que la ISR lo use */

    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = GPTIMER_HZ,
    };
    gptimer_event_callbacks_t callbacks = {.on_alarm = al_alarma};
    gptimer_alarm_config_t alarma = {
        .alarm_count = GPTIMER_ALARMA,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&cfg, &s_timer));
    /* Registrar el callback desde el Core 1 deja la ISR en el Core 1. */
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_timer, &callbacks, NULL));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &alarma));
    ESP_ERROR_CHECK(gptimer_enable(s_timer));
    ESP_ERROR_CHECK(gptimer_start(s_timer));

    xEventGroupSetBits(s_eventos, BIT_ARRANQUE);
    lazo_ctrl();
}

/* Trabajo de T_supervisor por ahora: la linea del comando "conteo". */
static void trabajo_supervisor(void)
{
    static int64_t proximo = 0;
    if (!g_conteo_activo) {
        proximo = 0;
        return;
    }
    int64_t ahora = esp_timer_get_time();
    if (ahora >= proximo) {
        printf("conteo %lu %lld\n", (unsigned long)g_activaciones, (long long)ahora);
        proximo = ahora + CONTEO_US;
    }
}

static void tarea_periodica(void *arg)
{
    tarea_info_t *t = (tarea_info_t *)arg;
    xEventGroupWaitBits(s_eventos, BIT_ARRANQUE, pdFALSE, pdTRUE, portMAX_DELAY);

    TickType_t ultimo = xTaskGetTickCount();
    if (t->desfase_ms) {
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(t->desfase_ms));
    }
    for (;;) {
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(t->periodo_ms));
        uint32_t c0 = esp_cpu_get_cycle_count();
        if (t == &s_tareas[T_SUPERVISOR]) {
            trabajo_supervisor();
        }
        uint32_t c1 = esp_cpu_get_cycle_count();
        int64_t fin = esp_timer_get_time();
        /* "ultimo" es el tick en que la tarea debia activarse. */
        int64_t liberacion = s_origen_us + (int64_t)ultimo * portTICK_PERIOD_MS * 1000;
        int64_t respuesta = fin - liberacion;
        med_registrar(&t->ejec, c1 - c0);
        med_registrar(&t->resp, (respuesta > 0) ? (uint32_t)respuesta : 0);
    }
}

static void tarea_por_eventos(void *arg)
{
    (void)arg;
    xEventGroupWaitBits(s_eventos, BIT_ARRANQUE, pdFALSE, pdTRUE, portMAX_DELAY);
    for (;;) {
        /* Sin eventos todavia (red, autotuning y NVS llegan en los paquetes siguientes). */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void tareas_crear(void)
{
    s_eventos = xEventGroupCreateStatic(&s_eventos_buf);

    /* Justo despues de un tick: origen para saber en que instante se libera
     * cada tarea periodica (error de pocos us). */
    vTaskDelay(1);
    TickType_t tick = xTaskGetTickCount();
    s_origen_us = esp_timer_get_time() - (int64_t)tick * portTICK_PERIOD_MS * 1000;

    for (int i = 0; i < NUM_TAREAS; i++) {
        tarea_info_t *t = &s_tareas[i];
        TaskFunction_t funcion = (i == T_CTRL) ? tarea_ctrl : (t->periodo_ms ? tarea_periodica : tarea_por_eventos);
        t->handle = xTaskCreateStaticPinnedToCore(funcion, t->nombre, t->pila, t, t->prioridad,
                                                  s_pilas[i], &s_tcb[i], t->nucleo);
        configASSERT(t->handle != NULL);
    }
}

int tareas_cantidad(void)
{
    return NUM_TAREAS;
}

tarea_info_t *tareas_info(int i)
{
    return (i >= 0 && i < NUM_TAREAS) ? &s_tareas[i] : NULL;
}
