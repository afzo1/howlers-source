// Interfaces de control (capa 3).
#pragma once

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Controlador de una entrada y una salida, de periodo fijo.
/// Implementación prevista: PidController, que reproduce la ecuación discreta
/// de pid_ext (components/control/pid_lib/autotuning_rnn), la misma con la que
/// se entrenaron los modelos LSTM del autotuning.
class IController : public Interface {
public:
    /// Periodo de muestreo y saturación de la salida. Se llama al configurar,
    /// nunca dentro del lazo.
    virtual Status configure(float period_s, float out_min, float out_max) noexcept = 0;

    /// Calcula la salida. Sin memoria dinámica ni bloqueos: va en IRAM.
    virtual float update(float setpoint, float measurement) noexcept = 0;

    /// Borra la historia (integral, errores y salidas anteriores).
    virtual void reset() noexcept = 0;

    /// Cambia las ganancias. Los controladores que no son PID devuelven
    /// Status::Unsupported. Solo se aplica con el robot detenido.
    virtual Status setGains(const PidGains& gains) noexcept = 0;

protected:
    ~IController() = default;
};

/// Estrategia de autosintonización. Implementaciones previstas:
///  - RemoteLstmTuner: protocolo TCP binario de bench_pid_autotuning hacia
///    python_server.py (puerto 8888), sin cambios, para reutilizar los modelos.
///  - OnDeviceLstmTuner (opcional): inferencia en el ESP32-S3 con esp-nn; se
///    decide con los resultados de la sección 5.
/// La usa T_autotune (Core 0). Las muestras las produce T_ctrl a 1 kHz.
class IGainTuner : public Interface {
public:
    enum class State : std::uint8_t { Idle, Collecting, WaitingResult, Done, Failed };

    virtual Status begin() noexcept = 0;
    virtual Status push(const AutotuneSample* samples, std::size_t count) noexcept = 0;

    /// Termina una pasada de la trayectoria de excitación y pide las ganancias.
    virtual Status requestResult() noexcept = 0;

    virtual State state() const noexcept = 0;

    /// Ganancias propuestas. El que llama las valida (rango, finitud) antes de usarlas.
    virtual Status result(WheelGains& out, bool& converged) noexcept = 0;

    virtual void end() noexcept = 0;

protected:
    ~IGainTuner() = default;
};

}  // namespace striker
