/**
 * \file        bldc_pwm.h
 * \brief
 * \details
 * \author      MaverickST
 * \version     0.0.3
 * \date        05/10/2023
 * \copyright   Unlicensed
 */

#ifndef __PWM_MOTOR_H__
#define __PWM_MOTOR_H__


#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"

#define MAP(val, in_min, in_max, out_min, out_max) ((val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min) /*!< Map function */

typedef struct {
    uint8_t rev_gpio_num;   ///< GPIO number
    uint8_t pwm_gpio_num;   ///< GPIO number
    uint32_t pwm_freq_hz;   ///< PWM frequency in Hz
    uint32_t pwm_duty_us;   ///< PWM duty cycle in microseconds
    uint32_t max_cmp;  ///< Maximum speed in Hz
    uint16_t pwm_bottom_duty; ///< Bottom duty cycle in percentage from 0 to 1000
    uint16_t pwm_top_duty;    ///< Top duty cycle in percentage from 0 to 1000

    uint16_t duty_cycle;    ///< Duty cycle in percentage
    int group_id;           ///< MCPWM group number 
    uint32_t resolution_hz; ///< MCPWM timer frequency

    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t operator;
    mcpwm_cmpr_handle_t cmp;
    mcpwm_cmpr_handle_t cmp_rev;
    mcpwm_gen_handle_t gen;
    mcpwm_gen_handle_t gen_rev;

} bldc_pwm_motor_t;

/**
 * @brief Initialize the BLDC motor
 * 
 * @param motor instance of the motor
 * @param pwm_gpio_num 
 * @param pwm_freq_hz 
 * @param group_id 
 * @param resolution_hz 
 */
esp_err_t bldc_init(bldc_pwm_motor_t *motor, uint8_t pwm_gpio_num, uint8_t rev_gpio_num, 
    uint32_t pwm_freq_hz, uint32_t group_id, uint32_t resolution_hz, uint16_t pwm_bottom_duty, uint16_t pwm_top_duty);

/**
 * @brief Enable the motor
 * 
 * @param motor instance of the motor
 */
esp_err_t bldc_enable(bldc_pwm_motor_t *motor);

/**
 * @brief Disable the motor
 * 
 * @param motor instance of the motor
 */
esp_err_t bldc_disable(bldc_pwm_motor_t *motor);

/**
 * @brief Set the duty which will receive the ESC. The duty is a value between 0 and 1000.
 * @note This value is the duty of the real PWM signal, not considering the range of the BLDC motor.
 * 
 * From the ESC data sheet, the default throttle range of this ESC is from 1100µs to 1940µs, respecto to 50Hz (20ms).
 * That means that the duty cycle is from 5.5% to 9.7%. 
 * But with the calibration process, that range change depending to the bottom and top duty cycle values.
 * 
 * For the reverse signal, the channel range of 0-50% (0-7.1% of real duty) is the default motor direction, 
 * and the channel range of 50-100% (7.1-9.7% of real duty) will cause the motor to spin counterclockwise.
 * 
 * @param motor instance of the motor
 * @param duty range from 0 to 1000, where 0 is 0% and 1000 is 100%
 */
esp_err_t bldc_set_duty(bldc_pwm_motor_t *motor, int duty);

/**
 * @brief Set the duty which will receive the ESC. The duty is a value between 0 and 100.
 * @note This function considers the range of the BLDC motor, setted in the bldc_init function (bottom and top duty),
 * so that the user can set the duty in a range from 0 to 100, like and abstraction of that range.
 * 
 * @param motor 
 * @param duty 
 * @return esp_err_t 
 */
esp_err_t bldc_set_duty_motor(bldc_pwm_motor_t *motor, float duty);

/**
 * @brief Calibrate the BLDC motor.
 * 
 * @param motor 
 * @param bottom_duty 
 * @param top_duty 
 * @return esp_err_t 
 */
esp_err_t bldc_calibrate(bldc_pwm_motor_t *motor, uint16_t bottom_duty, uint16_t top_duty);

#endif // __PWM_MOTOR_H__