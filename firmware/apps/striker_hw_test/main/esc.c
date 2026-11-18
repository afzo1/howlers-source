#include "esc.h"

#include <stdio.h>

#include "driver/mcpwm_prelude.h"
#include "sdkconfig.h"

#define ESC_RESOLUTION_HZ 1000000 // 1 tick = 1 us
#define ESC_PERIOD_TICKS 20000    // 20 ms = 50 Hz, igual que el firmware original

typedef struct {
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t oper;
    mcpwm_cmpr_handle_t cmp_thr;
    mcpwm_cmpr_handle_t cmp_rev;
    mcpwm_gen_handle_t gen_thr;
    mcpwm_gen_handle_t gen_rev;
} esc_ch_t;

static esc_ch_t s_ch[3];
static const robot_config_t *s_cfg = NULL;
static bool s_ready = false;

static esp_err_t setup_gen(mcpwm_gen_handle_t gen, mcpwm_cmpr_handle_t cmp)
{
    // Pulso en alto al inicio de cada periodo y en bajo al llegar al comparador
    esp_err_t err = mcpwm_generator_set_action_on_timer_event(gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    if (err != ESP_OK) {
        return err;
    }
    return mcpwm_generator_set_action_on_compare_event(gen,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp, MCPWM_GEN_ACTION_LOW));
}

static esp_err_t init_channel(int i)
{
    const motor_cfg_t *m = &s_cfg->motor[i];
    esc_ch_t *c = &s_ch[i];
    esp_err_t err;

    mcpwm_timer_config_t tcfg = {
        .group_id = m->mcpwm_group,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = ESC_RESOLUTION_HZ,
        .period_ticks = ESC_PERIOD_TICKS,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    if ((err = mcpwm_new_timer(&tcfg, &c->timer)) != ESP_OK) return err;

    mcpwm_operator_config_t ocfg = {.group_id = m->mcpwm_group};
    if ((err = mcpwm_new_operator(&ocfg, &c->oper)) != ESP_OK) return err;
    if ((err = mcpwm_operator_connect_timer(c->oper, c->timer)) != ESP_OK) return err;

    // Los comparadores se actualizan al inicio del periodo (igual que el original)
    mcpwm_comparator_config_t ccfg = {.flags.update_cmp_on_tez = true};
    if ((err = mcpwm_new_comparator(c->oper, &ccfg, &c->cmp_thr)) != ESP_OK) return err;
    if ((err = mcpwm_new_comparator(c->oper, &ccfg, &c->cmp_rev)) != ESP_OK) return err;

    // Valores de parada antes de conectar los pines
    int stop_us = robot_throttle_pulse_us(s_cfg, 0);
    if ((err = mcpwm_comparator_set_compare_value(c->cmp_thr, stop_us)) != ESP_OK) return err;
    if ((err = mcpwm_comparator_set_compare_value(c->cmp_rev, s_cfg->rev_forward_us)) != ESP_OK) return err;

    mcpwm_generator_config_t gcfg = {.gen_gpio_num = m->pwm_gpio};
    if ((err = mcpwm_new_generator(c->oper, &gcfg, &c->gen_thr)) != ESP_OK) return err;
    gcfg.gen_gpio_num = m->rev_gpio;
    if ((err = mcpwm_new_generator(c->oper, &gcfg, &c->gen_rev)) != ESP_OK) return err;
    if ((err = setup_gen(c->gen_thr, c->cmp_thr)) != ESP_OK) return err;
    if ((err = setup_gen(c->gen_rev, c->cmp_rev)) != ESP_OK) return err;

    if ((err = mcpwm_timer_enable(c->timer)) != ESP_OK) return err;
    return mcpwm_timer_start_stop(c->timer, MCPWM_TIMER_START_NO_STOP);
}

esp_err_t esc_init(const robot_config_t *cfg)
{
    s_cfg = cfg;
    for (int i = 0; i < 3; i++) {
        esp_err_t err = init_channel(i);
        if (err != ESP_OK) {
            printf("ERROR: no se pudo configurar el motor %d (%s): %s\n", i + 1,
                   esp_err_to_name(err), cfg->motor[i].name);
            return err;
        }
    }
    s_ready = true;
    return ESP_OK;
}

bool esc_ready(void)
{
    return s_ready;
}

int esc_set(int idx, int pct, bool reverse)
{
    if (!s_ready || idx < 0 || idx > 2) {
        return -1;
    }
    if (pct < 0) {
        pct = 0;
    }
    if (pct > CONFIG_STRIKER_HW_MAX_PCT) {
        pct = CONFIG_STRIKER_HW_MAX_PCT;
    }
    int thr_us = robot_throttle_pulse_us(s_cfg, pct);
    int rev_us = reverse ? s_cfg->rev_reverse_us : s_cfg->rev_forward_us;
    if (mcpwm_comparator_set_compare_value(s_ch[idx].cmp_rev, rev_us) != ESP_OK ||
        mcpwm_comparator_set_compare_value(s_ch[idx].cmp_thr, thr_us) != ESP_OK) {
        return -1;
    }
    return thr_us;
}

void esc_stop_all(void)
{
    if (!s_ready) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        esc_set(i, 0, false);
    }
}
