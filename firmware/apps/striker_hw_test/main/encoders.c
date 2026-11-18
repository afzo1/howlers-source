#include "encoders.h"

#include <stdio.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// Rango de salida del AS5600 en modo analogico 10 %..90 % de 3,3 V, igual que
// el firmware original (MAP de 330..2970 mV a 0..360 grados).
#define AS5600_MV_MIN 330
#define AS5600_MV_MAX 2970

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;
static adc_channel_t s_enc_ch[3];
static bool s_enc_ready = false;

esp_err_t adc_init_once(void)
{
    if (s_adc != NULL) {
        return ESP_OK;
    }
    adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = ADC_UNIT_1};
    esp_err_t err = adc_oneshot_new_unit(&ucfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }
    adc_cali_curve_fitting_config_t ccfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&ccfg, &s_cali) != ESP_OK) {
        s_cali = NULL; // sin calibracion: se usa una conversion aproximada
        printf("Aviso: el ADC no tiene calibracion de fabrica; las tensiones son aproximadas.\n");
    }
    return ESP_OK;
}

esp_err_t adc_attach_gpio(int gpio, adc_channel_t *out_channel)
{
    esp_err_t err = adc_init_once();
    if (err != ESP_OK) {
        return err;
    }
    adc_unit_t unit;
    adc_channel_t ch;
    err = adc_oneshot_io_to_channel(gpio, &unit, &ch);
    if (err != ESP_OK || unit != ADC_UNIT_1) {
        return ESP_ERR_INVALID_ARG;
    }
    adc_oneshot_chan_cfg_t cfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    err = adc_oneshot_config_channel(s_adc, ch, &cfg);
    if (err == ESP_OK) {
        *out_channel = ch;
    }
    return err;
}

int adc_read_mv(adc_channel_t ch)
{
    int raw = 0;
    if (adc_oneshot_read(s_adc, ch, &raw) != ESP_OK) {
        return -1;
    }
    if (s_cali != NULL) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) {
            return mv;
        }
    }
    return raw * 3100 / 4095; // aproximacion sin calibrar (atenuacion de 12 dB)
}

float as5600_angle_from_mv(int mv)
{
    if (mv < AS5600_MV_MIN) {
        mv = AS5600_MV_MIN;
    }
    if (mv > AS5600_MV_MAX) {
        mv = AS5600_MV_MAX;
    }
    float a = (float)(mv - AS5600_MV_MIN) * 360.0f / (float)(AS5600_MV_MAX - AS5600_MV_MIN);
    return a >= 360.0f ? 0.0f : a;
}

esp_err_t encoders_init(const int gpio[3])
{
    for (int i = 0; i < 3; i++) {
        esp_err_t err = adc_attach_gpio(gpio[i], &s_enc_ch[i]);
        if (err != ESP_OK) {
            printf("ERROR: GPIO%d no se pudo usar como entrada analogica (%s)\n", gpio[i], esp_err_to_name(err));
            return err;
        }
    }
    s_enc_ready = true;
    return ESP_OK;
}

float encoder_angle_deg(int idx, int *out_mv)
{
    int mv = adc_read_mv(s_enc_ch[idx]);
    if (out_mv) {
        *out_mv = mv;
    }
    if (mv < 0) {
        return -1.0f;
    }
    return as5600_angle_from_mv(mv);
}

bool encoders_ready(void)
{
    return s_enc_ready;
}
