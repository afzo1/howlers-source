// Descripción del robot: pines, geometría y modelo del ESC (capa 1/2).
//
// Un solo binario sirve para Striker 1 y Striker 2: la variante se guarda en
// NVS (por defecto, la de Kconfig) y al arrancar se elige la tabla
// correspondiente. Hasta saber qué robot es, ningún pin se configura como
// salida (igual que en firmware/apps/striker_hw_test).
#pragma once

#include <cstdint>

#include "striker/types.hpp"

namespace striker {

enum class RobotVariant : std::uint8_t { Unknown, S1, S2 };
enum class ImuType : std::uint8_t { Tm151Uart, Bno055I2c };

inline constexpr std::int8_t kNoPin = -1;

struct EscPins {
    std::int8_t throttle_gpio{kNoPin};   ///< Pulso de acelerador.
    std::int8_t direction_gpio{kNoPin};  ///< Pulso de sentido.
};

/// Modelo de pulsos del ESC, en microsegundos. Los valores de cada robot salen
/// de su firmware original (tabla en la sección 2 del informe).
struct EscPulseModel {
    std::uint16_t stop_us{};
    std::uint16_t full_us{};
    std::uint16_t dir_forward_us{};
    std::uint16_t dir_reverse_us{};
    std::uint16_t pwm_hz{50};
};

struct I2cPins {
    std::int8_t sda{kNoPin};
    std::int8_t scl{kNoPin};
};

struct UartPins {
    std::int8_t tx{kNoPin};
    std::int8_t rx{kNoPin};
};

/// Geometría en unidades SI. Las cifras de los firmwares viejos son
/// incoherentes entre sí (hallazgo N2): se confirman en el modo Calibración.
struct WheelGeometry {
    float wheel_radius_m{};
    float base_radius_m{};             ///< Del centro del robot al punto de contacto.
    PerWheel<float> wheel_angle_rad{}; ///< Posición angular de cada rueda.
    float gear_ratio{1.0f};            ///< Vueltas del eje con encoder por vuelta de rueda.
    PerWheel<std::int8_t> motor_sign{{1, 1, 1}};
    PerWheel<std::int8_t> encoder_sign{{1, 1, 1}};
};

struct MotionLimits {
    float max_wheel_rad_s{};
    float max_body_m_s{};
    float max_body_rad_s{};
    float max_accel_m_s2{};
    float max_duty_pct{};  ///< Saturación de la orden al ESC.
};

struct RobotConfig {
    RobotVariant variant{RobotVariant::Unknown};
    PerWheel<EscPins> esc{};
    EscPulseModel esc_pulse{};
    PerWheel<std::uint8_t> encoder_adc1_channel{};  ///< Canal de ADC1, lo lee el ULP.
    ImuType imu{ImuType::Bno055I2c};
    UartPins imu_uart{};
    I2cPins imu_i2c{};
    std::int8_t imu_reset_gpio{kNoPin};
    I2cPins range_i2c{};
    std::int8_t range_xshut_gpio{kNoPin};
    std::uint8_t battery_adc1_channel{};            ///< Canal de ADC1, lo lee el ULP.
    float battery_divider_ratio{};                  ///< V_batería / V_pin.
    WheelGeometry geometry{};
    MotionLimits limits{};
};

}  // namespace striker
