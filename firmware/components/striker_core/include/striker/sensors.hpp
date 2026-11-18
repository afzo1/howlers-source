// Interfaces de sensores (capa 2 hacia la capa 3).
#pragma once

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Sensor inercial. Implementaciones previstas:
///  - Bno055Imu (Striker 2, I2C), sobre components/sensors/bno055.
///  - Tm151Imu  (Striker 1, UART), sobre components/sensors/tm151 (EasyProfile).
/// La tarea de navegación solo conoce esta interfaz: cambiar de IMU no la modifica.
class ISensorIMU : public Interface {
public:
    /// Configura el sensor. Se llama una vez, antes de iniciar las tareas
    /// periódicas; puede bloquear (reinicio del sensor, cambio de modo).
    virtual Status init() noexcept = 0;

    /// Lee una muestra. Bloquea como máximo el tiempo de una transacción de E/S
    /// (I2C o UART) y nunca reserva memoria. Llamada desde T_imu (Core 1, 100 Hz).
    virtual Status read(ImuSample& out) noexcept = 0;

    /// Nombre para registros y telemetría.
    virtual const char* name() const noexcept = 0;

protected:
    ~ISensorIMU() = default;
};

/// Encoders de las tres ruedas. La implementación de v3 (UlpEncoders) no toca
/// el ADC: copia la última ronda que publicó el ULP en la memoria RTC, con un
/// protocolo de secuencia (seqlock) que no bloquea. Por eso los tres ángulos
/// vienen de la misma ronda y read() tarda menos de un microsegundo.
class IWheelEncoders : public Interface {
public:
    virtual Status init() noexcept = 0;

    /// No bloquea. Devuelve Status::Stale si el ULP no publicó una ronda nueva
    /// desde hace más de 2 ms (el lazo de ruedas pasa a Fallo).
    virtual Status read(EncoderReading& out) noexcept = 0;

protected:
    ~IWheelEncoders() = default;
};

/// Sensor de distancia (VL53L1X).
class IRangeSensor : public Interface {
public:
    virtual Status init() noexcept = 0;
    virtual Status read(RangeSample& out) noexcept = 0;

protected:
    ~IRangeSensor() = default;
};

/// Monitor de batería. La implementación de v3 (UlpBattery) solo lee el estado
/// que calcula el ULP; la CPU principal no toca el ADC.
class IBatteryMonitor : public Interface {
public:
    virtual Status read(BatteryStatus& out) noexcept = 0;

protected:
    ~IBatteryMonitor() = default;
};

}  // namespace striker
