/**
 * \file        led.c
 * \brief
 * \details
 * \author      MaverickST
 * \version     0.0.3
 * \date        05/10/2023
 * \copyright   Unlicensed
 */

#include "led.h"

static const char* TAG_led = "led";

void led_init(led_rgb_t *led, uint8_t lsb_rgb, uint32_t time, bool is_rgb)
{
    led->gpio_num = lsb_rgb;
    led->lsb_rgb = lsb_rgb;
    led->state = true;
    led->blink = false;
    led->cnt_blink = 0;
    led->color = 0x00;
    led->time = time;

    ///< Configure the GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = is_rgb? (7ULL << led->lsb_rgb): (1ULL << led->gpio_num),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

}

void led_timer_callback(void *arg) 
{
    led_rgb_t *led = (led_rgb_t *)arg;
    led->state = !led->state;
    // ESP_LOGI(TAG_led, "led_timer_callback: %d  color: %02x  time: %d", led->state, led->color, (int)(esp_rtc_get_time_us() - led->tv_now));
    if (led->state) {
        led->cnt_blink++;
        led_turn_color(led, led->color);
    }else{
        // ESP_LOGI(TAG_led, "led_timer_callback: turn off");
        led_turn_color(led, 0x00);
        if (!led->blink) ESP_ERROR_CHECK(esp_timer_delete(led->oneshot_timer));
    }

    if (led->blink){
        if (led->cnt_blink < led->time_blink){
            ESP_ERROR_CHECK(esp_timer_start_once(led->oneshot_timer, led->time));
        }
        else{
            led->blink = false;
            led->cnt_blink = 0;
            led_turn_color(led, 0x00);
            ESP_ERROR_CHECK(esp_timer_delete(led->oneshot_timer));
        }
    }
}

void led_setup_timer(led_rgb_t *led, uint8_t color, uint32_t time)
{
    led_turn_color(led, color);
    led->color = color;
    led->state = true;

    ///< Configure the timer
    const esp_timer_create_args_t oneshot_timer_args = {
            .callback = &led_timer_callback,
            .arg = (void*) led, ////< argument specified here will be passed to timer callback function
            .name = "led-one-shot" ///< name is optional, but may help identify the timer when debugging
    };
    esp_timer_handle_t oneshot_timer;
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
    led->oneshot_timer = oneshot_timer;

    led->tv_now = esp_rtc_get_time_us();
    // ESP_LOGI(TAG_led, "led_setup_timer: %d", (int)led->tv_now);
    ESP_ERROR_CHECK(esp_timer_start_once(led->oneshot_timer, time));
}