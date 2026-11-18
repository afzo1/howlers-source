/**
 * \file        led.h
 * \brief
 * \details
 * \author      MaverickST
 * \version     0.0.3
 * \date        05/10/2023
 * \copyright   Unlicensed
 */

#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

typedef struct
{
    uint8_t gpio_num;
    bool state;
    bool blink; ///< If true, the LED will blink. It has to be set before calling led_setup_timer
    bool is_rgb; ///< If true, the LED is RGB
    uint8_t time_blink; ///< number of times the LED will blink
    uint8_t cnt_blink;  ///< counter of the number of times the LED has blinked

    // GPIO pinout for RGB LED
    uint8_t lsb_rgb;    ///< LSB of the RGB LED
    uint8_t color;      ///< Value of the RGB LED
    uint32_t time;      ///< Time (us) for the RGB LED
    uint32_t tv_now;
    esp_timer_handle_t oneshot_timer;  ///< Timer for the RGB LED

}led_rgb_t;

/**
 * @brief This function initializes the GPIOs for the RGB LED
 * 
 * @param led 
 * @param lsb_rgb or the GPIO number for a single LED
 * @param time in microseconds
 */
void led_init(led_rgb_t *led, uint8_t lsb_rgb, uint32_t time, bool is_rgb);

/**
 * @brief Callback function for the RGB LED.
 * It will turn off the LED, but if the blink flag is set, it will set the alarm again
 * 
 * @param arg 
 */
void led_timer_callback(void *arg);

/**
 * @brief Set the needed parameters for the RGB LED to blink
 * 
 * @param led 
 * @param blink 
 * @param time_blink 
 */
static inline void led_set_blink(led_rgb_t *led, bool blink, uint8_t time_blink)
{
    led->blink = blink;
    led->cnt_blink = 0;
    led->time_blink = time_blink;
}

/**
 * @brief This function turns on the LED depending on the color
 * 
 * @param led 
 * @param color just the three LSBs are used: 0b111 means white, 0b000 means off
 */
static inline void led_turn_color(led_rgb_t *led, uint8_t color)
{
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num + 0, (color >> 0) & 0x01));
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num + 1, (color >> 1) & 0x01));
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num + 2, (color >> 2) & 0x01));
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color and set the alarm
 * 
 * @param led 
 * @param color 
 * @param time in microseconds
 */
void led_setup_timer(led_rgb_t *led, uint8_t color, uint32_t time);

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to red and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_red(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x04, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to green and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_green(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x02, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to blue and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_blue(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x01, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to white and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_white(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x07, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to yellow and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_yellow(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x06, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to purple and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_purple(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x05, time);
}

/**
 * @brief Configure the RGB LED: turn on the LED, set the color to orange and set the alarm
 * 
 * @param led 
 */
static inline void led_setup_orange(led_rgb_t *led, uint32_t time)
{
    led_setup_timer(led, 0x03, time);
}

// ----------------------------------------------------------------------------
// ------------------------- NO-RGB LED FUNCTIONS -----------------------------
// ----------------------------------------------------------------------------

/**
 * @brief This function turns on the LED depending on the color (struct parameter)
 * 
 * @param led 
 */
static inline void led_on(led_rgb_t *led) 
{
    led->state = true;
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num, 1));
}

/**
 * @brief This function turns off the LED. Equivalent to led_on(led, 0b000).
 * 
 * @param led 
 */
static inline void led_off(led_rgb_t *led)
{
    led->state = false;
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num, 0));
}

static inline void led_toggle(led_rgb_t *led, uint8_t color)
{
    led->state = !led->state;
    ESP_ERROR_CHECK(gpio_set_level(led->gpio_num, led->state));
}

#endif