// Interfaces de movimiento: cinemática, trayectorias y seguimiento (capa 3).
#pragma once

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Cinemática del robot omnidireccional de 3 ruedas. Implementación prevista:
/// OmniKinematics, con la geometría de RobotConfig (radio de rueda, radio de la
/// base, ángulo de cada rueda y reducción). Reemplaza a las dos versiones
/// incompatibles de los firmwares viejos (hallazgo N2).
class IKinematics : public Interface {
public:
    /// Inversa: velocidad del cuerpo -> velocidad de cada rueda.
    virtual void toWheels(const BodyTwist& body, WheelSpeeds& wheels) const noexcept = 0;

    /// Directa: velocidad de cada rueda -> velocidad del cuerpo (odometría).
    virtual void toBody(const WheelSpeeds& wheels, BodyTwist& body) const noexcept = 0;

protected:
    ~IKinematics() = default;
};

/// Trayectoria parametrizada en el tiempo. Implementaciones previstas:
/// LineTrajectory, CircleTrajectory, FigureEightTrajectory y WaypointTrajectory,
/// construidas a partir de un RouteSpec en un almacenamiento estático (sin new).
class ITrajectory : public Interface {
public:
    /// Valida y carga la especificación. InvalidArgument si los parámetros no
    /// son alcanzables con los límites del robot.
    virtual Status load(const RouteSpec& spec, const Pose2D& start) noexcept = 0;

    /// Referencia en el instante t (segundos desde el inicio de la ruta).
    virtual Status reference(float t_s, TrajectoryPoint& out) const noexcept = 0;

    virtual float duration_s() const noexcept = 0;

protected:
    ~ITrajectory() = default;
};

/// Seguidor de trayectoria en lazo cerrado: a partir de la referencia y de la
/// pose estimada calcula la velocidad que debe tener el cuerpo.
/// Implementación prevista: prealimentación de la velocidad de referencia más
/// corrección proporcional del error de pose, expresada en el marco del cuerpo.
class IPathTracker : public Interface {
public:
    virtual void reset() noexcept = 0;
    virtual BodyTwist command(const TrajectoryPoint& reference, const Pose2D& estimate) noexcept = 0;

protected:
    ~IPathTracker() = default;
};

}  // namespace striker
