// Tipos de datos comunes de Striker v3.
//
// Contrato de diseño de la Entrega 1 (docs/striker_v3/02_arquitectura_capas.md).
// Reglas que cumple todo este archivo:
//  - Unidades del SI en todos los campos, indicadas con un sufijo:
//    _m, _m_s, _m_s2, _rad, _rad_s, _s, _us, _v, _pct.
//  - Solo float (32 bits): la FPU del ESP32-S3 no hace double (hallazgo N1).
//  - Todas las estructuras son trivialmente copiables, para poder pasarlas por
//    colas de FreeRTOS (que copian bytes) y guardarlas en NVS.
//  - Nada usa memoria dinámica.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace striker {

/// Número de ruedas del robot omnidireccional.
inline constexpr std::size_t kNumWheels = 3;

/// Arreglo con un valor por rueda. El índice i corresponde a la rueda i+1
/// definida en RobotConfig (posición angular, motor, encoder).
template <typename T>
using PerWheel = std::array<T, kNumWheels>;

/// Tiempo en microsegundos desde el arranque (esp_timer_get_time()).
using TimeUs = std::int64_t;

/// Resultado de una operación. Reemplaza a las excepciones (-fno-exceptions).
enum class Status : std::uint8_t {
    Ok = 0,
    NotReady,         ///< El recurso todavía no está inicializado o armado.
    Timeout,          ///< La E/S no respondió a tiempo.
    Stale,            ///< El dato existe pero es demasiado viejo.
    InvalidArgument,  ///< Parámetro fuera de rango.
    BusError,         ///< Error de I2C, UART o ADC.
    Unsupported,      ///< La implementación no ofrece esa operación.
    Fault,            ///< Falla que obliga a detener el robot.
};

// ---------------------------------------------------------------------------
// Movimiento
// ---------------------------------------------------------------------------

/// Velocidad angular de cada rueda, medida en el eje de la rueda (después de
/// la reducción, si existe). Positiva según el sentido definido en RobotConfig.
struct WheelSpeeds {
    PerWheel<float> rad_s{};
};

/// Velocidad del cuerpo en su propio marco: x hacia el frente, y hacia la
/// izquierda, giro positivo antihorario visto desde arriba.
struct BodyTwist {
    float vx_m_s{};
    float vy_m_s{};
    float wz_rad_s{};
};

/// Pose en el marco del campo.
struct Pose2D {
    float x_m{};
    float y_m{};
    float theta_rad{};  ///< Normalizado a [-pi, pi).
};

/// Punto de referencia de una trayectoria en el instante t.
struct TrajectoryPoint {
    float t_s{};
    Pose2D pose{};             ///< Pose deseada (marco del campo).
    BodyTwist twist_field{};   ///< Velocidad deseada (marco del campo), para prealimentación.
    bool last{};               ///< true en el último punto de la ruta.
};

// ---------------------------------------------------------------------------
// Sensores
// ---------------------------------------------------------------------------

/// Lectura simultánea de los tres encoders AS5600 (misma ronda del ULP).
struct EncoderReading {
    PerWheel<float> angle_rad{};  ///< Ángulo del eje donde está el imán, [0, 2pi).
    PerWheel<bool> valid{};
    std::uint32_t round{};        ///< Contador de rondas del ULP (detecta datos repetidos).
    TimeUs t_us{};
};

/// Muestra de la IMU (BNO055 en Striker 2, TM151 en Striker 1).
struct ImuSample {
    float yaw_rad{};          ///< Rumbo, normalizado a [-pi, pi).
    float yaw_rate_rad_s{};   ///< Velocidad de giro alrededor de z.
    float ax_m_s2{};          ///< Aceleración lineal en x del cuerpo, sin gravedad.
    float ay_m_s2{};          ///< Aceleración lineal en y del cuerpo, sin gravedad.
    std::uint8_t calibration{};  ///< 0 (sin calibrar) a 3 (calibrada); 3 si el sensor no lo reporta.
    TimeUs t_us{};
};

/// Distancia del VL53L1X.
struct RangeSample {
    float distance_m{};
    bool valid{};
    TimeUs t_us{};
};

/// Estado de la batería que publica el ULP.
struct BatteryStatus {
    float voltage_v{};
    float cell_voltage_v{};
    bool low{};       ///< Por debajo del umbral de advertencia: no se inician rutas ni autotuning.
    bool critical{};  ///< Por debajo del umbral crítico: modo Fallo.
    TimeUs t_us{};
};

// ---------------------------------------------------------------------------
// Actuadores y control
// ---------------------------------------------------------------------------

/// Orden a los tres ESC, en porcentaje de acelerador con signo (-100 a 100).
/// El signo es el sentido de giro. Es la misma escala de salida del PID del banco
/// de autotuning (saturado a ±80), con la que se entrenaron los modelos LSTM.
struct MotorCommand {
    PerWheel<float> duty_pct{};
};

struct PidGains {
    float kp{};
    float ki{};
    float kd{};
};

/// Ganancias de los tres PID de rueda, con versión para detectar cambios.
struct WheelGains {
    PerWheel<PidGains> wheel{};
    std::uint32_t version{};
    std::uint8_t source{};  ///< 0 = por defecto, 1 = manual, 2 = autotuning.
};

/// Muestra de 1 kHz para el servidor de autotuning. Mismo formato de 64 bytes
/// que robot_sample_t de bench_pid_autotuning (telemetry.h), para no cambiar
/// el protocolo TCP ni los modelos LSTM entrenados.
struct AutotuneSample {
    std::uint32_t timestamp_ms{};
    float motor_state[3]{};     ///< rad/s, orden [R, L, B] del banco.
    float motor_setpoint[3]{};  ///< rad/s.
    float errors[3][3]{};       ///< [rueda][e(k), e(k-1), e(k-2)].
};
static_assert(sizeof(AutotuneSample) == 64, "debe coincidir con robot_sample_t (64 B)");

// ---------------------------------------------------------------------------
// Rutas parametrizables (las envía la app web; el formato de red es de la sección 5)
// ---------------------------------------------------------------------------

enum class RouteType : std::uint8_t { Line, Circle, FigureEight, Waypoints };

struct Waypoint {
    float x_m{};
    float y_m{};
    float theta_rad{};
    float t_s{};  ///< Instante en que se debe alcanzar, desde el inicio de la ruta.
};

inline constexpr std::size_t kMaxWaypoints = 32;

/// Especificación de una ruta. Tamaño fijo: no hay memoria dinámica.
struct RouteSpec {
    RouteType type{RouteType::Line};
    float speed_m_s{0.2f};    ///< Velocidad de recorrido (Line, Circle, FigureEight).
    float length_m{};         ///< Line.
    float heading_rad{};      ///< Line: dirección en el marco del campo.
    float radius_m{};         ///< Circle y FigureEight.
    bool clockwise{};         ///< Circle.
    std::uint8_t laps{1};     ///< Circle y FigureEight.
    std::uint8_t num_waypoints{};
    std::array<Waypoint, kMaxWaypoints> waypoints{};
};

}  // namespace striker
