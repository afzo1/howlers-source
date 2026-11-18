// Base común de las interfaces de Striker v3.
#pragma once

namespace striker {

/// Toda interfaz hereda de esta clase. Garantiza tres cosas:
///  1. No se puede copiar un objeto a través de su interfaz (evita el "slicing").
///  2. El destructor es protegido y no virtual: nadie puede hacer `delete` a
///     través de un puntero a la interfaz. Los objetos de v3 son estáticos y
///     viven todo el programa, así que nunca se destruyen por ese camino.
///  3. No hay RTTI ni excepciones: el polimorfismo es solo por funciones
///     virtuales, que funcionan con -fno-rtti y -fno-exceptions.
class Interface {
public:
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;

protected:
    Interface() = default;
    ~Interface() = default;
};

}  // namespace striker
