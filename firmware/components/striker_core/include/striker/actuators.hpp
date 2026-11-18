// Interfaz de los motores (capa 2 hacia la capa 3).
#pragma once

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Los tres ESC del robot. Implementación prevista: EscMotors, sobre MCPWM
/// (un temporizador por motor, comparadores actualizados en el evento TEZ),
/// con el modelo de pulsos de RobotConfig::esc_pulse.
///
/// Seguridad (no negociable):
///  - Después de init() los ESC solo reciben pulsos de parada.
///  - set() falla con Status::NotReady si los motores no están armados.
///  - stopAll() funciona siempre, incluso desarmado, y no bloquea.
class IMotors : public Interface {
public:
    virtual Status init() noexcept = 0;

    /// Habilita las órdenes de movimiento. Solo la llama el supervisor de modos.
    virtual Status arm() noexcept = 0;

    /// Vuelve a pulsos de parada y bloquea nuevas órdenes.
    virtual void disarm() noexcept = 0;

    virtual bool armed() const noexcept = 0;

    /// Aplica la orden a los tres motores en el mismo ciclo. Satura cada valor
    /// a los límites de RobotConfig. No bloquea; llamada desde T_ctrl a 1 kHz.
    virtual Status set(const MotorCommand& cmd) noexcept = 0;

    /// Pulsos de parada en los tres motores. No bloquea.
    virtual void stopAll() noexcept = 0;

protected:
    ~IMotors() = default;
};

}  // namespace striker
