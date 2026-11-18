# Striker v3 — Entrega 1: plan de ingeniería sustentado

**Howlers FC · Grupo de Investigación Robosyn · Facultad de Ingeniería, Universidad de Antioquia**
**Proyecto:** firmware unificado `firmware/apps/striker_v3` para el robot omnidireccional de 3 ruedas (ESP32-S3, ESP-IDF v5.5.1, FreeRTOS SMP)
**Autor:** Andrés Felipe Zuluaga Ortiz
**Fecha:** 18 de noviembre de 2026 · rama `entrega-1` de [`afzo1/howlers-source`](https://github.com/afzo1/howlers-source)

Este es el informe de la Entrega 1 que pide la *Guía de Proyecto para Estudiantes: Diseño e Implementación del Nuevo Firmware Unificado Striker v3*. Cada sección es un documento; esta página es la portada y el índice. Quien llega por primera vez al proyecto puede empezar por [00_contexto_proyecto.md](00_contexto_proyecto.md).

---

## Índice

| Sección de la Entrega 1 | Documento | Estado |
|---|---|---|
| 1. Diagnóstico de referencia (Striker 1 y Striker 2) | [01_diagnostico_striker1_striker2.md](01_diagnostico_striker1_striker2.md) | Hecho |
| 1 (anexo). Correcciones mínimas para compilar las apps de referencia | [01-1_correcciones_compilacion.md](01-1_correcciones_compilacion.md) | Hecho |
| 2. Arquitectura por capas y diagrama de clases C/C++ | [02_arquitectura_capas.md](02_arquitectura_capas.md) | Hecho |
| 3. Diseño FreeRTOS y tabla de tiempo real | [03_freertos_tiempo_real.md](03_freertos_tiempo_real.md) | Hecho |
| 4. Mapa de memoria ESP32-S3 y diseño ULP | [04_memoria_ulp.md](04_memoria_ulp.md) | Hecho |
| 5. Estrategia matemática y protocolo Web/UDP | — | **Se entrega después**: sus pruebas de ciclos de CPU (`float`, punto fijo, SIMD) se miden en el ESP32-S3 |

Los hallazgos del bench de autotuning y de la suite de estimación que cambian el diseño están en la sección 2 ([2.4.5 y 2.4.6](02_arquitectura_capas.md#245-la-suite-de-estimación-lista-para-usar-salvo-la-memoria-del-ukf)).

---

## Resumen ejecutivo

1. **Diagnóstico.** Los firmwares de Striker 1 y Striker 2 tienen problemas de concurrencia (datos compartidos sin protección, red y control en el mismo núcleo), de tiempo real (periodos relativos, *tick* de 100 Hz, sin medición), de memoria (sin decisiones de ubicación, PSRAM sin usar) y de seguridad (sin tiempos de espera). Cada hallazgo tiene un código, una severidad y un enlace al código.
2. **Arquitectura.** Cuatro capas con dependencias en un solo sentido. El contrato entre capas ya existe como componente ESP-IDF ([`striker_core`](../../firmware/components/striker_core/README.md)): tipos en unidades SI, interfaces C++ sin excepciones, sin RTTI y sin memoria dinámica, y una máquina de estados cuyas reglas de seguridad se comprueban al compilar. Un mismo binario sirve para los dos robots.
3. **FreeRTOS.** El Core 1 tiene solo el tiempo real (`T_ctrl` a 1 kHz, IMU, navegación, distancia); el Core 0, la red, la supervisión y el autotuning. El lazo de ruedas lo despierta un `gptimer` por notificación directa; las tareas se comunican por buzones, colas, *stream/message buffers* y grupos de eventos; los recursos compartidos usan mutex con herencia de prioridad, sin cruzar de núcleo.
4. **Tiempo real.** Prioridades por Deadline Monotonic. El análisis de tiempo de respuesta da R ≤ D para todas las tareas en los dos robots; el lazo de ruedas responde en 75 µs con un plazo de 500 µs, y el cómputo podría crecer entre 1,7 y 5,3 veces antes de perder un plazo. La Entrega 2 reemplaza los presupuestos por WCET medidos con un plan de instrumentación ya definido.
5. **Memoria.** Lazo rápido e ISR en IRAM, estado y pilas en DRAM, historiales y red en PSRAM. Con la ejecución desde PSRAM, escribir la flash ya no detiene los núcleos. La base de software medida ocupa el 30 % de la SRAM interna disponible y v3 llegaría a un 54 %.
6. **ULP.** El ULP-RISC-V es el único dueño del ADC1: muestrea los encoders cada 250 µs, supervisa la batería y vigila que el lazo de ruedas siga vivo, con capacidad de reiniciar el chip si los dos núcleos se bloquean.

---

## Dónde se cubre cada criterio de la rúbrica (Entrega 1)

| Criterio | Peso | Dónde |
|---|---:|---|
| 1. Diseño FreeRTOS (tareas, sincronización, recursos) | 20 % | Sección 3.2 a 3.5 y 3.10 |
| 2. Arquitectura por capas, reutilización y C++ | 15 % | Sección 2 completa; contrato en `firmware/components/striker_core` |
| 3. Tiempo real (planificación, WCET, verificación) | 20 % | Sección 3.6 a 3.9 |
| 4. Arquitectura ESP32-S3 (IRAM, PSRAM y ULP) | 15 % | Sección 4 completa |
| 5. Precisión y optimización matemática | 15 % | Sección 5 (después); hallazgos N1–N5 en la sección 1 |
| 6. Modo Autotuning + rutas desde la app web | 15 % | Modos Autotune y Route, `RouteSpec`, `IGainTuner`, `ITrajectory` (sección 2); tareas, *buffers* y supervisión del autotuning (sección 3); el protocolo Web/UDP va en la sección 5 |

---

## Cómo verificar este informe

Todo lo que el informe afirma sobre el código, las interfaces, los tiempos y la memoria se puede comprobar con las herramientas de [`herramientas/`](herramientas/) (con ESP-IDF v5.5.1 cargado: `. ~/esp/esp-idf/export.sh`):

```bash
# Contrato de interfaces: compila con g++ (C++17 y C++20), con el compilador del ESP32-S3
# y como componente de un proyecto ESP-IDF; sin excepciones ni RTTI, y sin malloc/new.
docs/striker_v3/herramientas/verificar_interfaces.sh --idf

# Tabla de tiempo real (sección 3.8) para cada robot.
python3 docs/striker_v3/herramientas/analisis_tiempo_real.py docs/striker_v3/herramientas/tareas_v3.json --variante S1
python3 docs/striker_v3/herramientas/analisis_tiempo_real.py docs/striker_v3/herramientas/tareas_v3.json --variante S2

# Línea base de memoria (sección 4.4): compila ejemplos de ESP-IDF con la configuración de v3.
docs/striker_v3/herramientas/medir_memoria_base.sh

# Enlaces y líneas citadas: del repositorio, de ESP-IDF (con $IDF_PATH en v5.5.1)
# y de otros repositorios clonados (VERIFICAR_REPOS="dueno/repo=/ruta").
python3 docs/striker_v3/herramientas/verificar_enlaces.py docs/striker_v3/03_freertos_tiempo_real.md

# Apps de referencia (sección 1).
docs/striker_v3/herramientas/compilar_referencias.sh . /tmp/build_referencias
```

---

## Datos del robot que faltan

El diseño no depende de estos datos para estar completo: quedan como parámetros de `RobotConfig` y se confirman con la [guía de pruebas del robot](guia_pruebas_robot.md) y el firmware [`striker_hw_test`](../../firmware/apps/striker_hw_test/README.md).

| Dato | Para qué | Sección |
|---|---|---|
| Si el robot heredado es Striker 1 o Striker 2, y su cableado | Elegir la configuración | 2.6 |
| Modelo de ESC: frecuencias que acepta y qué hace si la señal desaparece | PWM más rápido; seguridad tras un reinicio | 3.7.3, 4.8.6 |
| Tipo de batería y número de celdas | Divisor de tensión y umbrales | 4.7.1, 4.8.5 |
| Radio de rueda, radio de la base y reducción del encoder | Cinemática | 2.6.2 |
| Modo de salida de los AS5600 (0–100 % o 10–90 %) | Conversión a ángulo | 4.8.7 |
