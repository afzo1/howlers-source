/**
 * \file        bldc_pwm.c
 * \brief
 * \details
 * \author      MaverickST
 * \version     0.0.3
 * \date        05/10/2023
 * \copyright   Unlicensed
 */

#include "bldc_pwm.h"

static const char *BLDC_TAG = "bdc_motor_mcpwm";

esp_err_t bldc_init(bldc_pwm_motor_t *motor, uint8_t pwm_gpio_num, uint8_t rev_gpio_num, uint32_t pwm_freq_hz, 
    uint32_t group_id, uint32_t resolution_hz, uint16_t pwm_bottom_duty, uint16_t pwm_top_duty)
{
    motor->rev_gpio_num = rev_gpio_num;
    motor->pwm_gpio_num = pwm_gpio_num;
    motor->pwm_freq_hz = pwm_freq_hz;
    motor->group_id = group_id;
    motor->resolution_hz = resolution_hz;
    motor->max_cmp = resolution_hz / pwm_freq_hz;
    motor->pwm_bottom_duty = pwm_bottom_duty;
    motor->pwm_top_duty = pwm_top_duty;
    
    // mcpwm timer
    mcpwm_timer_config_t timer_config = {
        .group_id = group_id,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .period_ticks = resolution_hz / pwm_freq_hz,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    mcpwm_new_timer(&timer_config, &motor->timer);

    motor->operator = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = group_id,
    };
    mcpwm_new_operator(&operator_config, &motor->operator);

    mcpwm_operator_connect_timer(motor->operator, motor->timer);

    motor->cmp = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    mcpwm_new_comparator(motor->operator, &comparator_config, &motor->cmp);
    mcpwm_new_comparator(motor->operator, &comparator_config, &motor->cmp_rev);
    mcpwm_comparator_set_compare_value(motor->cmp, 0);
    mcpwm_comparator_set_compare_value(motor->cmp_rev, 0);

    motor->gen = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = pwm_gpio_num,
    };
    mcpwm_new_generator(motor->operator, &generator_config, &motor->gen);
    generator_config.gen_gpio_num = rev_gpio_num;
    mcpwm_new_generator(motor->operator, &generator_config, &motor->gen_rev);

    ///< Set generation actions for the pwm signal
    mcpwm_generator_set_actions_on_timer_event(motor->gen,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
            MCPWM_GEN_TIMER_EVENT_ACTION_END());
    mcpwm_generator_set_actions_on_compare_event(motor->gen,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, motor->cmp, MCPWM_GEN_ACTION_LOW),
            MCPWM_GEN_COMPARE_EVENT_ACTION_END());

    ///< Set generation actions for the reverse signal
    mcpwm_generator_set_actions_on_timer_event(motor->gen_rev,
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH),
            MCPWM_GEN_TIMER_EVENT_ACTION_END());
    mcpwm_generator_set_actions_on_compare_event(motor->gen_rev,
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, motor->cmp_rev, MCPWM_GEN_ACTION_LOW),
            MCPWM_GEN_COMPARE_EVENT_ACTION_END());

    return ESP_OK;
}

esp_err_t bldc_enable(bldc_pwm_motor_t *motor)
{
    ESP_RETURN_ON_ERROR(mcpwm_timer_enable(motor->timer), BLDC_TAG, "enable timer failed");
    ESP_RETURN_ON_ERROR(mcpwm_timer_start_stop(motor->timer, MCPWM_TIMER_START_NO_STOP), BLDC_TAG, "start timer failed");
    return ESP_OK;
}

esp_err_t bldc_disable(bldc_pwm_motor_t *motor)
{
    ESP_RETURN_ON_ERROR(mcpwm_timer_start_stop(motor->timer, MCPWM_TIMER_STOP_EMPTY), BLDC_TAG, "stop timer failed");
    ESP_RETURN_ON_ERROR(mcpwm_timer_disable(motor->timer), BLDC_TAG, "disable timer failed");
    return ESP_OK;
}

esp_err_t bldc_set_duty(bldc_pwm_motor_t *motor, int duty)
{
    if (duty > 1000) {
        // ESP_LOGE(BLDC_TAG, "duty %d is greater than 1000", duty);
        return ESP_FAIL;
    }
    motor->duty_cycle = duty;
    uint32_t nw_cmp = duty*motor->max_cmp/1000;
    if (nw_cmp > motor->max_cmp) {
        // ESP_LOGE(BLDC_TAG, "speed %d is greater than max speed %d", speed, motor->max_speed_hz);
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(motor->cmp, nw_cmp), BLDC_TAG, "set compare value failed");
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(motor->cmp_rev, nw_cmp), BLDC_TAG, "set compare value failed");

    return ESP_OK;
}

esp_err_t bldc_set_duty_motor(bldc_pwm_motor_t *motor, float duty)
{
    ///< Checks if the duty is in the range of -100 to 100
    if (duty > 100 || duty < -100) {
        return ESP_FAIL;
    }

    ///< Checks if reverse is needed
    float reverse = 30; ///< 40% means not reverse
    if (duty < 0) {
        reverse = 70; ///< 70% means reverse
        duty = -duty; ///< Set the duty to positive value
    }

    ///< Maps the duty value to the range of 0 to 1000 considering the bottom and top duty cycle
    int mapped_duty = MAP(duty, 0, 100, motor->pwm_bottom_duty, motor->pwm_top_duty);
    int mapped_duty_rev = MAP(reverse, 0, 100, motor->pwm_bottom_duty, motor->pwm_top_duty);
    uint32_t cmp_value =  mapped_duty * motor->max_cmp / 1000; ///< Maps the duty value to the range of 0 to max_cmp
    uint32_t cmp_value_rev =  mapped_duty_rev * motor->max_cmp / 1000; ///< Maps the duty value to the range of 0 to max_cmp

    ///< Set the compare value for the forward and reverse signals
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(motor->cmp, cmp_value), BLDC_TAG, "set compare value failed");
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(motor->cmp_rev, cmp_value_rev), BLDC_TAG, "set compare value failed");

    return ESP_OK;
}

esp_err_t bldc_calibrate(bldc_pwm_motor_t *motor, uint16_t bottom_duty, uint16_t top_duty)
{
    ESP_RETURN_ON_FALSE(bottom_duty < top_duty, ESP_ERR_INVALID_ARG, BLDC_TAG, "bottom duty must be less than top duty");

    ///< Calibration of the motor
    ESP_RETURN_ON_ERROR(bldc_set_duty(motor, top_duty), BLDC_TAG, "set top duty failed");
    vTaskDelay(pdMS_TO_TICKS(6000)); ///< Wait for 6 seconds to calibrate the motor
    ESP_RETURN_ON_ERROR(bldc_set_duty(motor, bottom_duty), BLDC_TAG, "set bottom duty failed");
    vTaskDelay(pdMS_TO_TICKS(2000)); ///< Wait for 2 seconds to calibrate the motor

    motor->pwm_bottom_duty = bottom_duty;
    motor->pwm_top_duty = top_duty;

    return ESP_OK;
}
