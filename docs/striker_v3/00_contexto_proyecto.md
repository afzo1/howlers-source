# Contexto del proyecto Striker v3

Resumen de una página para quien llega al proyecto: qué se pide, dónde está cada cosa, qué se ha decidido y qué falta. Se actualiza al cerrar cada etapa.

**Última actualización:** 11 de noviembre de 2026

---

## 1. Qué se pide

La *Guía de Proyecto para Estudiantes* (Howlers FC, grupo Robosyn, Facultad de Ingeniería, Universidad de Antioquia) pide diseñar e implementar un **firmware unificado** para el robot omnidireccional de 3 ruedas Striker, en `firmware/apps/striker_v3`, sobre ESP32-S3 con ESP-IDF y FreeRTOS SMP. Debe reunir lo mejor de Striker 1, Striker 2, el banco `bench_pid_autotuning` y la suite de estimación de estado, y superar sus problemas de acoplamiento, concurrencia y determinismo.

**Seis ejes técnicos:**

| Eje | Tema | Lo esencial |
|---|---|---|
| 1 | FreeRTOS SMP | Core 0 para WiFi/UDP/telemetría, Core 1 solo para el lazo de control; colas (`xQueueOverwrite`), stream/message buffers, event groups, notificaciones desde ISR y mutex con herencia de prioridad |
| 2 | Capas y C++ | 4 capas (HAL → drivers → control/estimación → aplicación y máquina de estados); interfaces `ISensorIMU`, `IWheelEncoder`, `IController`; sin `new`/`malloc` en los lazos, sin excepciones ni RTTI |
| 3 | Tiempo real | Prioridades RM/DM, activación periódica exacta, tabla Ti/Di/Ci, U = Σ Ci/Ti, medición de WCET, jitter y pila |
| 4 | Memoria y ULP | DRAM interna para lo crítico, PSRAM para buffers grandes, `IRAM_ATTR` para ISR y lazo rápido, una tarea en el coprocesador ULP (por ejemplo, la batería) |
| 5 | Precisión numérica | Solo `float`; comparar `float`, punto fijo (Q15/Q16.16) y SIMD (esp-dsp/esp-nn) |
| 6 | Autotuning y rutas | Modo de autotuning PID integrado, ganancias en NVS; rutas parametrizables (recta, círculo, ocho, waypoints) desde la app web, en lazo cerrado, con telemetría de pose |

**Entregas:**
- **Entrega 1 — plan de ingeniería:** (1) diagnóstico de referencia; (2) arquitectura por capas y diagrama de clases; (3) diseño FreeRTOS y tabla de tiempo real; (4) mapa de memoria y diseño ULP; (5) estrategia matemática y protocolo web/UDP. La Entrega 2 solo empieza cuando la 1 esté aprobada.
- **Entrega 2 — implementación:** firmware compilable, informe de verificación temporal y de memoria, y demostración en el robot (autotuning y al menos 3 rutas desde la app web, con gráficas de error).

---

## 2. Dónde está cada cosa

| Qué | Dónde |
|---|---|
| Repositorio de trabajo | [`afzo1/howlers-source`](https://github.com/afzo1/howlers-source), copia (*fork*) del repositorio base [`ravelasqvel/howlers-source`](https://github.com/ravelasqvel/howlers-source). Los cambios se proponen solo dentro del fork |
| Rama de la Entrega 1 | `entrega-1`, con su *pull request* #1 hacia `main` del fork |
| Documentos de diseño | [`docs/striker_v3/`](README.md) |
| Firmwares de referencia | `firmware/apps/striker_v1` (Striker 1), `firmware/apps/striker_v2` (Striker 2), `firmware/apps/bench_pid_autotuning` |
| Drivers compartidos | `firmware/components/` (todavía no son componentes ESP-IDF; ver hallazgo A1) |
| Estaciones base | `software/ground_station/` |
| Repositorios originales | [`nparra-code/striker-1`](https://github.com/nparra-code/striker-1), [`MaverickST/RoboCup_Striker2`](https://github.com/MaverickST/RoboCup_Striker2) y [`MaverickST/mobile-robot-estimation-suite`](https://github.com/MaverickST/mobile-robot-estimation-suite) (la suite de estimación no está en el monorepo) |
| Firmware de pruebas del robot | [`firmware/apps/striker_hw_test`](../../firmware/apps/striker_hw_test/README.md) |

---

## 3. Hardware

- **Placa:** YD-ESP32-S3 N16R8 de VCC-GND Studio (serigrafía «YD-ESP32-23 2022-V1.3»): 16 MB de flash, 8 MB de PSRAM octal, conversor USB-serie CH343 en UART0 (conector COM) y USB nativo en GPIO19/20 (conector USB). Detalle en la sección 3.3 del [diagnóstico](01_diagnostico_striker1_striker2.md).
- **Robot:** el equipo tiene un robot heredado de los autores de los firmwares viejos. Falta confirmar si es Striker 1 o Striker 2, su cableado y el modelo de ESC: para eso es la [guía de pruebas](guia_pruebas_robot.md).
- **Diferencias entre robots:** Striker 1 usa la IMU TM151 por UART; Striker 2, la BNO055 por I2C. Los dos usan encoders AS5600 leídos como tensión analógica, sensor de distancia VL53L1X y ESC con pulsos de 50 Hz más una señal de sentido. Los pines de cada uno están en la tabla de [`striker_hw_test`](../../firmware/apps/striker_hw_test/README.md#pines-tomados-de-los-firmwares-originales).

---

## 4. Herramientas

- **ESP-IDF v5.5.1** para todo el proyecto (los firmwares viejos usaban 5.2.5, 5.4.0 y 5.5.1; hallazgo T7). En Linux se instala con [`herramientas/instalar_esp_idf.sh`](herramientas/instalar_esp_idf.sh).
- [`herramientas/compilar_referencias.sh`](herramientas/compilar_referencias.sh): compila todas las apps sobre una copia del repositorio y resume el resultado.
- [`herramientas/buscar_double.py`](herramientas/buscar_double.py): lista las operaciones `double` emuladas de cada función en un binario.
- [`herramientas/verificar_enlaces.py`](herramientas/verificar_enlaces.py): comprueba que los enlaces de un documento apunten a archivos y líneas que existen.

---

## 5. Estado

| Parte | Estado |
|---|---|
| Entrega 1 · 1. Diagnóstico de Striker 1 y Striker 2 | Hecho: [01_diagnostico](01_diagnostico_striker1_striker2.md) |
| Correcciones mínimas para compilar las apps de referencia | Hecho: [01-1](01-1_correcciones_compilacion.md). Compilan las seis apps |
| Pruebas del robot heredado | Guía y firmware listos ([guía](guia_pruebas_robot.md)); pruebas por hacer |
| Entrega 1 · 1b. Diagnóstico de `bench_pid_autotuning` y de la suite de estimación | Pendiente |
| Entrega 1 · 2 a 5 | Pendientes |

---

## 6. Decisiones tomadas

| # | Decisión | Razón | Dónde |
|---|---|---|---|
| D1 | Los firmwares viejos se conservan como referencia, con correcciones mínimas para compilar y sin corregir su comportamiento | Son la línea base que v3 debe medir y superar | [01-1](01-1_correcciones_compilacion.md), criterios 1 y 5 |
| D2 | Cuando una app vieja quedó apuntando a drivers de otra, se restauran sus drivers originales dentro de la app (`local_drivers/`) | Vuelve a ser el código que sus autores probaron | 01-1, criterio 2 |
| D3 | Las primeras pruebas del robot se hacen con un firmware de pruebas propio, no con los firmwares viejos | Los viejos tienen riesgos al encender y sin WiFi (F1, F2, F4) | [Guía de pruebas](guia_pruebas_robot.md), sección 0 |
| D4 | El diagnóstico enlaza el código con enlaces permanentes al commit analizado (`0451897`) | Las citas no cambian cuando el código se corrige | Diagnóstico, encabezado |
| D5 | Para v3 (propuesto, por confirmar en las secciones 2 a 5 de la Entrega 1): Core 1 exclusivo para el lazo; buzones entre capas; `gptimer` con ISR para el periodo; solo `float`; un mapa de pines sin pines de *strapping*, USB ni bus de memoria; PSRAM en modo octal | Hallazgos C1–C8, T1–T7, M1–M5 y N1–N5 | Diagnóstico, sección 7 |

---

## 7. Convenciones

- Documentos en español, en `docs/striker_v3/`, numerados según la sección de la Entrega (`01_…`, `01-1_…`).
- Los hallazgos del diagnóstico tienen un código por eje (C = concurrencia, A = arquitectura, T = tiempo real, M = memoria, N = numérico, F = funcional) y una severidad (P0, P1, P2).
- Cada afirmación sobre el código viejo enlaza el archivo y las líneas donde se observa.
- Los cambios al código viejo se marcan con un comentario `[Correccion 2026-11]` que dice qué había y por qué cambió.

---

## 8. Próximos pasos

1. Hacer las pruebas del robot con la [guía](guia_pruebas_robot.md) y registrar los resultados en `docs/striker_v3/pruebas/`.
2. Diagnóstico de `bench_pid_autotuning`, de la suite de estimación y de los servidores de autotuning (sección 1b).
3. Con el robot identificado: mapa de pines de v3 y datos del ESC (frecuencias o protocolos que acepta).
4. Secciones 2 a 5 de la Entrega 1.

---

## Glosario breve

| Término | Significado |
|---|---|
| ESC | Controlador electrónico del motor sin escobillas (*brushless*): recibe pulsos y entrega la potencia al motor |
| AS5600 | Sensor magnético de ángulo; aquí se usa como encoder de cada rueda |
| BNO055 / TM151 | IMU (sensor de orientación) de Striker 2 / Striker 1 |
| VL53L1X | Sensor de distancia láser |
| Pines de *strapping* | Pines que el ESP32-S3 lee al arrancar para decidir cómo arranca (GPIO0, 3, 45 y 46) |
| PSRAM | Memoria RAM externa al chip, más grande y más lenta que la interna |
| WCET | Tiempo de ejecución en el peor caso de una tarea |
| *Jitter* | Variación del periodo real de una tarea periódica |
| LiPo | Batería de litio-polímero |
