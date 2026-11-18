// Interfaz de estimación de estado (capa 3).
#pragma once

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Entrada de un paso del estimador (100 Hz, T_nav).
struct EstimatorInput {
    float dt_s{};               ///< Medido, no supuesto.
    BodyTwist odometry{};       ///< Velocidad del cuerpo por cinemática directa de las ruedas.
    bool odometry_valid{};
    ImuSample imu{};
    bool imu_valid{};
};

struct EstimatorOutput {
    Pose2D pose{};
    BodyTwist twist_body{};
    float position_std_m{};     ///< Raíz de la traza de la covarianza de posición.
    float heading_std_rad{};
    bool healthy{};             ///< false si la covarianza dejó de ser definida positiva.
};

/// Estimador de la pose del robot. Implementaciones previstas, sobre el código C
/// de la suite de estimación (MaverickST/mobile-robot-estimation-suite):
///  - EkfEstimator (por defecto): estado [x, y, psi, vx, vy, omega].
///  - UkfEstimator: el mismo modelo con transformada unscented.
/// Toda la memoria (estado, covarianza, temporales) es estática y se reserva
/// en DRAM interna al construir el objeto.
class IStateEstimator : public Interface {
public:
    virtual void reset(const Pose2D& initial) noexcept = 0;

    /// Predicción y corrección en una llamada. Sin memoria dinámica.
    virtual Status step(const EstimatorInput& in, EstimatorOutput& out) noexcept = 0;

    virtual const char* name() const noexcept = 0;

protected:
    ~IStateEstimator() = default;
};

}  // namespace striker
