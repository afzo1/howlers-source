// Interfaz de almacenamiento persistente de parámetros (capa 1/4).
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "striker/interface.hpp"
#include "striker/types.hpp"

namespace striker {

/// Parámetros que se guardan en NVS. Cada uno es un registro con versión y CRC.
enum class ParamKey : std::uint8_t {
    RobotVariant,
    Geometry,
    EncoderCalibration,
    ImuCalibration,
    WheelGains,
    Network,  ///< Credenciales WiFi y dirección del servidor: nunca en el código fuente.
};

/// Almacén de parámetros. Implementación prevista: NvsParamStore.
///
/// Regla de tiempo real: escribir en la flash desactiva la caché y detiene las
/// dos CPU (sección 4). Por eso save() solo lo ejecuta T_storage, y solo cuando
/// el supervisor confirma que los motores están en parada. load() se usa al
/// arrancar y al entrar a un modo, nunca dentro de un lazo periódico.
class IParamStore : public Interface {
public:
    virtual Status load(ParamKey key, void* data, std::size_t size) noexcept = 0;
    virtual Status save(ParamKey key, const void* data, std::size_t size) noexcept = 0;

    /// Versión con tipo: solo acepta estructuras trivialmente copiables.
    template <typename T>
    Status loadAs(ParamKey key, T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "solo datos trivialmente copiables");
        return load(key, &value, sizeof(T));
    }

    template <typename T>
    Status saveAs(ParamKey key, const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "solo datos trivialmente copiables");
        return save(key, &value, sizeof(T));
    }

protected:
    ~IParamStore() = default;
};

}  // namespace striker
