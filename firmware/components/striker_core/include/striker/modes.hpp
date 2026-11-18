// Máquina de estados de la aplicación (capa 4).
//
// La transición es una función pura y constexpr: se puede verificar en tiempo
// de compilación con static_assert y probar en el PC sin el robot. El gestor de
// modos (T_supervisor, Core 0) la evalúa y ejecuta las acciones de entrada y
// salida de cada modo (armar o desarmar los motores, avisar a las tareas por
// el grupo de eventos de modo).
#pragma once

#include <cstdint>

namespace striker {

enum class Mode : std::uint8_t {
    Boot,         ///< Inicialización y autodiagnóstico. Motores desarmados.
    Idle,         ///< Quieto, desarmado. Único modo en que se escribe en NVS.
    Teleop,       ///< Velocidad del cuerpo enviada desde la app web.
    Route,        ///< Ruta parametrizada en lazo cerrado con la pose estimada.
    Autotune,     ///< Trayectoria de excitación y sintonización de los PID.
    Calibration,  ///< Encoders (sentido y cero), IMU, prueba de motores.
    Fault,        ///< Falla: motores en parada hasta que el operador la reinicie.
};

enum class Event : std::uint8_t {
    InitOk,
    InitFailed,
    CmdTeleop,
    CmdRoute,
    CmdAutotune,
    CmdCalibrate,
    CmdStop,
    CmdResetFault,
    RouteDone,
    AutotuneDone,
    AutotuneFailed,
    CalibrationDone,
    LinkTimeout,       ///< Sin latido de la app web dentro del plazo.
    BatteryCritical,   ///< Aviso del ULP.
    SensorFault,       ///< IMU o encoders sin datos válidos.
    ControlOverrun,    ///< T_ctrl no terminó antes de su plazo varias veces seguidas.
    UlpLost,           ///< El ULP dejó de publicar rondas.
};

/// Condiciones que el gestor de modos evalúa antes de cada transición.
struct Guards {
    bool battery_ok{};       ///< Sin aviso de batería baja.
    bool sensors_ok{};       ///< Encoders e IMU con datos recientes.
    bool link_ok{};          ///< Latido de la app web vigente.
    bool estimator_ready{};  ///< Estimador inicializado y sano.
    bool fault_cleared{};    ///< La causa de la falla ya no está presente.
};

constexpr bool isFault(Event e) noexcept
{
    return e == Event::BatteryCritical || e == Event::SensorFault ||
           e == Event::ControlOverrun || e == Event::UlpLost;
}

/// true en los modos en que los motores están armados.
constexpr bool motorsArmed(Mode m) noexcept
{
    return m == Mode::Teleop || m == Mode::Route || m == Mode::Autotune || m == Mode::Calibration;
}

/// Función de transición. Un evento que no aplica a un modo no cambia el modo.
constexpr Mode next(Mode m, Event e, const Guards& g) noexcept
{
    if (isFault(e) && m != Mode::Fault) {
        return Mode::Fault;
    }
    const bool can_move = g.battery_ok && g.sensors_ok && g.link_ok;
    switch (m) {
    case Mode::Boot:
        if (e == Event::InitOk) return Mode::Idle;
        if (e == Event::InitFailed) return Mode::Fault;
        return Mode::Boot;
    case Mode::Idle:
        if (e == Event::CmdTeleop && can_move) return Mode::Teleop;
        if (e == Event::CmdRoute && can_move && g.estimator_ready) return Mode::Route;
        if (e == Event::CmdAutotune && can_move) return Mode::Autotune;
        if (e == Event::CmdCalibrate && g.battery_ok && g.link_ok) return Mode::Calibration;
        return Mode::Idle;
    case Mode::Teleop:
        if (e == Event::CmdStop || e == Event::LinkTimeout) return Mode::Idle;
        return Mode::Teleop;
    case Mode::Route:
        if (e == Event::CmdStop || e == Event::LinkTimeout || e == Event::RouteDone) return Mode::Idle;
        return Mode::Route;
    case Mode::Autotune:
        if (e == Event::CmdStop || e == Event::LinkTimeout || e == Event::AutotuneDone ||
            e == Event::AutotuneFailed) {
            return Mode::Idle;
        }
        return Mode::Autotune;
    case Mode::Calibration:
        if (e == Event::CmdStop || e == Event::LinkTimeout || e == Event::CalibrationDone) return Mode::Idle;
        return Mode::Calibration;
    case Mode::Fault:
        if (e == Event::CmdResetFault && g.fault_cleared && g.battery_ok) return Mode::Idle;
        return Mode::Fault;
    }
    return Mode::Fault;  // No se alcanza: todos los modos están cubiertos.
}

}  // namespace striker
