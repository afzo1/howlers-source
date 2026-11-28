/*
 * Striker v3 - medicion de tiempos sin instrumentos externos.
 *
 * Medidores con un solo escritor y lectura por seqlock, y el pin de medida:
 * T_ctrl lo sube al empezar y lo baja al terminar, y un canal de captura del
 * MCPWM anota por hardware el instante de cada flanco (resolucion de 12,5 ns).
 * Asi se mide el periodo real y la duracion de T_ctrl sin analizador logico.
 */
#include <string.h>

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "medicion.h"

#define GRUPO_CAPTURA       1
#define HZ_CAPTURA_ESPERADO 80000000u /* 80 ticks por us */

/* Pines que no usan Striker 1 ni Striker 2 y no son de arranque, USB, flash,
 * PSRAM ni UART0. En el robot, confirmar antes que el pin elegido este libre. */
static const int PINES_PERMITIDOS[] = {9, 10, 11, 12, 13, 14};

static volatile uint32_t s_generacion = 1;

medidor_t g_med_lat_isr = MEDIDOR("lat_isr", "Latencia de la ISR del gptimer", 10, 0);
medidor_t g_med_activacion = MEDIDOR("activacion", "Retraso de T_ctrl frente al instante ideal (su variacion es el jitter)", 10, 0);
medidor_t g_med_periodo = MEDIDOR("periodo", "Periodo entre ISR del gptimer (contador de ciclos)", 240, 900);
medidor_t g_med_duracion = MEDIDOR("duracion", "Duracion del trabajo de T_ctrl (contador de ciclos)", 240, 0);
medidor_t g_med_cap_periodo = MEDIDOR("cap_periodo", "Periodo en el pin de medida (captura MCPWM)", 80, 900);
medidor_t g_med_cap_ancho = MEDIDOR("cap_ancho", "Ancho del pulso en el pin de medida (captura MCPWM)", 80, 0);

volatile uint32_t g_activaciones;
volatile uint32_t g_perdidas;
volatile uint32_t g_pin_mascara;
volatile uint32_t g_carga_ciclos;

static mcpwm_cap_timer_handle_t s_cap_timer;
static mcpwm_cap_channel_handle_t s_cap_canal;
static int s_pin = -1;
static uint32_t s_cap_subida;
static bool s_cap_hay_subida;

void IRAM_ATTR med_registrar(medidor_t *m, uint32_t ticks)
{
    uint32_t gen = s_generacion;
    m->seq++;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (m->gen != gen) {
        m->gen = gen;
        m->n = 0;
        m->min = UINT32_MAX;
        m->max = 0;
        m->suma = 0;
        m->bajo = 0;
        for (int i = 0; i <= MED_CASILLAS; i++) {
            m->hist[i] = 0;
        }
    }
    m->n++;
    m->suma += ticks;
    if (ticks < m->min) {
        m->min = ticks;
    }
    if (ticks > m->max) {
        m->max = ticks;
    }
    int32_t us = (int32_t)(ticks / m->ticks_por_us) - m->base_us;
    if (us < 0) {
        m->bajo++;
    } else {
        m->hist[(us < MED_CASILLAS) ? us : MED_CASILLAS]++;
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    m->seq++;
}

void med_copiar(const medidor_t *m, medidor_t *copia)
{
    uint32_t s1;
    uint32_t s2;
    do {
        s1 = m->seq;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        memcpy(copia, (const void *)m, sizeof(*copia));
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        s2 = m->seq;
    } while (s1 != s2 || (s1 & 1u));

    if (copia->gen != s_generacion) {
        copia->n = 0;
        copia->min = 0;
        copia->max = 0;
        copia->suma = 0;
        copia->bajo = 0;
        memset(copia->hist, 0, sizeof(copia->hist));
    }
}

void med_reiniciar_todo(void)
{
    s_generacion++;
}

/* ISR de la captura (Core 0): flanco de subida = inicio de T_ctrl. */
static bool IRAM_ATTR al_capturar(mcpwm_cap_channel_handle_t canal, const mcpwm_capture_event_data_t *ev, void *ctx)
{
    (void)canal;
    (void)ctx;
    uint32_t valor = ev->cap_value;
    if (ev->cap_edge == MCPWM_CAP_EDGE_POS) {
        if (s_cap_hay_subida) {
            med_registrar(&g_med_cap_periodo, valor - s_cap_subida);
        }
        s_cap_subida = valor;
        s_cap_hay_subida = true;
    } else if (s_cap_hay_subida) {
        med_registrar(&g_med_cap_ancho, valor - s_cap_subida);
    }
    return false;
}

static void captura_apagar(void)
{
    g_pin_mascara = 0;
    vTaskDelay(pdMS_TO_TICKS(5)); /* T_ctrl deja de mover el pin */
    if (s_cap_timer) {
        mcpwm_capture_timer_stop(s_cap_timer);
    }
    if (s_cap_canal) {
        mcpwm_capture_channel_disable(s_cap_canal);
        mcpwm_del_capture_channel(s_cap_canal);
        s_cap_canal = NULL;
    }
    if (s_cap_timer) {
        mcpwm_capture_timer_disable(s_cap_timer);
        mcpwm_del_capture_timer(s_cap_timer);
        s_cap_timer = NULL;
    }
    if (s_pin >= 0) {
        gpio_reset_pin(s_pin);
        s_pin = -1;
    }
    s_cap_hay_subida = false;
}

bool med_pin_configurar(int pin)
{
    if (pin < 0) {
        captura_apagar();
        return true;
    }
    bool permitido = false;
    for (size_t i = 0; i < sizeof(PINES_PERMITIDOS) / sizeof(PINES_PERMITIDOS[0]); i++) {
        if (PINES_PERMITIDOS[i] == pin) {
            permitido = true;
        }
    }
    if (!permitido) {
        return false;
    }

    captura_apagar();
    s_pin = pin; /* desde aqui, captura_apagar() lo deja como estaba */

    mcpwm_capture_timer_config_t cfg_timer = {
        .group_id = GRUPO_CAPTURA,
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
    };
    mcpwm_capture_channel_config_t cfg_canal = {
        .gpio_num = pin,
        .prescale = 1,
        .flags.pos_edge = true,
        .flags.neg_edge = true,
    };
    mcpwm_capture_event_callbacks_t callbacks = {.on_cap = al_capturar};
    uint32_t resolucion = 0;

    if (mcpwm_new_capture_timer(&cfg_timer, &s_cap_timer) != ESP_OK ||
        mcpwm_capture_timer_get_resolution(s_cap_timer, &resolucion) != ESP_OK ||
        resolucion != HZ_CAPTURA_ESPERADO ||
        mcpwm_new_capture_channel(s_cap_timer, &cfg_canal, &s_cap_canal) != ESP_OK ||
        mcpwm_capture_channel_register_event_callbacks(s_cap_canal, &callbacks, NULL) != ESP_OK ||
        mcpwm_capture_channel_enable(s_cap_canal) != ESP_OK) {
        captura_apagar();
        return false;
    }
    /* La captura deja el pin como entrada. Se habilita tambien la salida (GPIO
     * simple) para que T_ctrl lo mueva y la captura lea ese mismo pin. */
    gpio_set_level(pin, 0);
    if (gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT) != ESP_OK ||
        mcpwm_capture_timer_enable(s_cap_timer) != ESP_OK ||
        mcpwm_capture_timer_start(s_cap_timer) != ESP_OK) {
        captura_apagar();
        return false;
    }
    g_pin_mascara = 1u << pin;
    return true;
}

int med_pin_actual(void)
{
    return s_pin;
}
