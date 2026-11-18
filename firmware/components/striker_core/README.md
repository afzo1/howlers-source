# striker_core — contrato de Striker v3

Tipos de datos e interfaces C++ que separan la capa de drivers (2) de la capa de dominio de control y estimación (3) del firmware unificado Striker v3. Es un componente ESP-IDF **solo de encabezados**: no tiene implementaciones. Es parte de la Entrega 1 (sección 2, [`02_arquitectura_capas.md`](../../../docs/striker_v3/02_arquitectura_capas.md)); las implementaciones se escriben en la Entrega 2.

| Archivo | Contenido |
|---|---|
| [`types.hpp`](include/striker/types.hpp) | Tipos comunes en unidades SI: ruedas, cuerpo, pose, muestras de sensores, órdenes a los ESC, ganancias, muestra de autotuning (64 B), rutas |
| [`interface.hpp`](include/striker/interface.hpp) | Base de todas las interfaces: no copiable y con destructor protegido |
| [`sensors.hpp`](include/striker/sensors.hpp) | `ISensorIMU`, `IWheelEncoders`, `IRangeSensor`, `IBatteryMonitor` |
| [`actuators.hpp`](include/striker/actuators.hpp) | `IMotors` |
| [`control.hpp`](include/striker/control.hpp) | `IController`, `IGainTuner` |
| [`estimation.hpp`](include/striker/estimation.hpp) | `IStateEstimator` |
| [`motion.hpp`](include/striker/motion.hpp) | `IKinematics`, `ITrajectory`, `IPathTracker` |
| [`storage.hpp`](include/striker/storage.hpp) | `IParamStore` |
| [`modes.hpp`](include/striker/modes.hpp) | Modos, eventos y función de transición `constexpr` |
| [`robot_config.hpp`](include/striker/robot_config.hpp) | `RobotConfig`: pines, modelo del ESC, geometría y límites de cada robot |
| [`striker.hpp`](include/striker/striker.hpp) | Incluye todo lo anterior |
| [`test/verificar_contrato.cpp`](test/verificar_contrato.cpp) | Prueba de compilación del contrato |

## Reglas

- Unidades del SI con sufijo en el nombre; solo `float`.
- Sin excepciones ni RTTI: todo devuelve `Status` y es `noexcept`.
- Sin memoria dinámica: tamaños fijos; los datos son trivialmente copiables.
- Las interfaces no se copian ni se destruyen a través de un puntero a la interfaz.

## Verificación

Desde la raíz del repositorio, con ESP-IDF activo:

```bash
docs/striker_v3/herramientas/verificar_interfaces.sh --idf
```

Compila la prueba con el g++ del PC (C++17 y C++20) y con el compilador del ESP32-S3, con todos los avisos como errores, y comprueba que el objeto no use `malloc`, `operator new`, RTTI ni excepciones. Con `--idf` además compila un proyecto ESP-IDF mínimo que usa este componente (un par de minutos la primera vez).
