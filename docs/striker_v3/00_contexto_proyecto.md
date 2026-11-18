# Contexto del proyecto Striker v3

Resumen de una página para quien llega al proyecto: qué se pide, dónde está cada cosa, qué se ha decidido y qué falta. Se actualiza al cerrar cada etapa.

**Última actualización:** 18 de noviembre de 2026 (secciones 2 a 4 de la Entrega 1)

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
| Informe de la Entrega 1 (portada e índice) | [`docs/striker_v3/README.md`](README.md) |
| Firmwares de referencia | `firmware/apps/striker_v1` (Striker 1), `firmware/apps/striker_v2` (Striker 2), `firmware/apps/bench_pid_autotuning` |
| Drivers compartidos | `firmware/components/` (las variantes viejas todavía no son componentes ESP-IDF; ver hallazgo A1) |
| Contrato de v3 (tipos, interfaces y máquina de estados) | [`firmware/components/striker_core`](../../firmware/components/striker_core/README.md), primer componente ESP-IDF de v3 |
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
- [`herramientas/verificar_enlaces.py`](herramientas/verificar_enlaces.py): comprueba que los enlaces de un documento apunten a archivos y líneas que existen, también en ESP-IDF (con `$IDF_PATH`) y en otros repositorios clonados (`VERIFICAR_REPOS`).
- [`herramientas/verificar_interfaces.sh`](herramientas/verificar_interfaces.sh): compila el contrato `striker_core` en el PC y para el ESP32-S3, y comprueba que no use memoria dinámica, excepciones ni RTTI.
- [`herramientas/analisis_tiempo_real.py`](herramientas/analisis_tiempo_real.py) con [`tareas_v3.json`](herramientas/tareas_v3.json): análisis de tiempo de respuesta de la sección 3.8.
- [`herramientas/medir_memoria_base.sh`](herramientas/medir_memoria_base.sh), con [`sdkconfig_v3.defaults`](herramientas/sdkconfig_v3.defaults) y [`particiones_v3.csv`](herramientas/particiones_v3.csv): línea base de memoria de la sección 4.4.

---

## 5. Estado

| Parte | Estado |
|---|---|
| Entrega 1 · 1. Diagnóstico de Striker 1 y Striker 2 | Hecho: [01_diagnostico](01_diagnostico_striker1_striker2.md) |
| Correcciones mínimas para compilar las apps de referencia | Hecho: [01-1](01-1_correcciones_compilacion.md). Compilan las seis apps |
| Pruebas del robot heredado | Guía y firmware listos ([guía](guia_pruebas_robot.md)); pruebas por hacer |
| Entrega 1 · 1b. Hallazgos de `bench_pid_autotuning` y de la suite de estimación | Integrados en la [sección 2](02_arquitectura_capas.md) (2.4.5 y 2.4.6) |
| Entrega 1 · 2. Arquitectura por capas y diagrama de clases | Hecho: [02](02_arquitectura_capas.md) y el componente [`striker_core`](../../firmware/components/striker_core/README.md) |
| Entrega 1 · 3. Diseño FreeRTOS y tabla de tiempo real | Hecho: [03](03_freertos_tiempo_real.md) |
| Entrega 1 · 4. Mapa de memoria y diseño ULP | Hecho: [04](04_memoria_ulp.md) |
| Entrega 1 · 5. Estrategia matemática y protocolo Web/UDP | Se entrega después: necesita medir ciclos de CPU en el ESP32-S3 |

---

## 6. Decisiones tomadas

| # | Decisión | Razón | Dónde |
|---|---|---|---|
| D1 | Los firmwares viejos se conservan como referencia, con correcciones mínimas para compilar y sin corregir su comportamiento | Son la línea base que v3 debe medir y superar | [01-1](01-1_correcciones_compilacion.md), criterios 1 y 5 |
| D2 | Cuando una app vieja quedó apuntando a drivers de otra, se restauran sus drivers originales dentro de la app (`local_drivers/`) | Vuelve a ser el código que sus autores probaron | 01-1, criterio 2 |
| D3 | Las primeras pruebas del robot se hacen con un firmware de pruebas propio, no con los firmwares viejos | Los viejos tienen riesgos al encender y sin WiFi (F1, F2, F4) | [Guía de pruebas](guia_pruebas_robot.md), sección 0 |
| D4 | El diagnóstico enlaza el código con enlaces permanentes al commit analizado (`0451897`) | Las citas no cambian cuando el código se corrige | Diagnóstico, encabezado |
| D5 | Core 1 solo para el tiempo real; buzones entre tareas; `gptimer` con ISR para el periodo de 1 kHz; solo `float`; PSRAM en modo octal (confirmado en las secciones 2 a 4) | Hallazgos C1–C8, T1–T7, M1–M4 y N1–N5 | Diagnóstico, sección 7 |
| D6 | El contrato entre capas es un componente ESP-IDF de solo encabezados (`striker_core`), verificado al compilar | Que las interfaces existan y compilen antes de la implementación | [Sección 2.5](02_arquitectura_capas.md#25-interfaces-c-y-diagrama-de-clases) |
| D7 | Un solo binario para los dos robots, con `RobotConfig` elegido en el arranque | Evitar variantes por robot que se separan con el tiempo (hallazgos A2 y A5) | [Sección 2.6](02_arquitectura_capas.md#26-un-binario-para-los-dos-robots-robotconfig) |
| D8 | Prioridades por Deadline Monotonic, un dueño por recurso y ningún mutex compartido entre núcleos | Análisis de bloqueos acotado en cada núcleo | [Sección 3](03_freertos_tiempo_real.md) |
| D9 | El ULP-RISC-V es el único dueño del ADC1 (encoders y batería) y vigila el lazo de ruedas | ESP-IDF no deja compartir el ADC1 entre el ULP y la CPU | [Sección 4.8](04_memoria_ulp.md#48-diseño-del-ulp-risc-v) |
| D10 | Ejecución desde PSRAM (`CONFIG_SPIRAM_XIP_FROM_PSRAM`) y escrituras en NVS solo en Idle | Escribir la flash no detiene los núcleos | [Sección 4.3](04_memoria_ulp.md#43-escrituras-en-la-flash-y-caché) |
| D11 | Se conserva el cableado de los robots; recablear los pines de *strapping* queda como decisión del equipo | v3 debe funcionar con los robots como están (hallazgo M5) | [Sección 4.7.3](04_memoria_ulp.md#473-pines-de-riesgo-heredados-hallazgo-m5) |
| D12 | La sección 5 de la Entrega 1 se entrega después del resto | Sus pruebas de ciclos de CPU se miden en el ESP32-S3 | [Portada del informe](README.md) |

---

## 7. Convenciones

- Documentos en español, en `docs/striker_v3/`, numerados según la sección de la Entrega (`01_…`, `01-1_…`).
- Los hallazgos del diagnóstico tienen un código por eje (C = concurrencia, A = arquitectura, T = tiempo real, M = memoria, N = numérico, F = funcional) y una severidad (P0, P1, P2).
- Cada afirmación sobre el código viejo enlaza el archivo y las líneas donde se observa.
- Los cambios al código viejo se marcan con un comentario `[Correccion 2026-11]` que dice qué había y por qué cambió.

---

## 8. Próximos pasos

1. Hacer las pruebas del robot con la [guía](guia_pruebas_robot.md) y registrar los resultados en `docs/striker_v3/pruebas/`: robot (S1 o S2), ESC, batería y geometría ([datos que faltan](README.md#datos-del-robot-que-faltan)).
2. Sección 5 de la Entrega 1: medir en el ESP32-S3 los ciclos de PID y Kalman en `float`, punto fijo y esp-dsp, y especificar las tramas UDP/JSON de rutas y autotuning.
3. Entrega 2: implementar `striker_hal`, `striker_domain` y la app `striker_v3` sobre el contrato `striker_core`, y medir con el plan de la sección 3.9.

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
| RM / DM | *Rate Monotonic* / *Deadline Monotonic*: dar más prioridad a la tarea de menor periodo / de menor plazo |
| Análisis de tiempo de respuesta | Cálculo del peor tiempo entre que una tarea se activa y termina, sumando lo que la interrumpen las de mayor prioridad |
| IRAM / DRAM | Partes de la SRAM interna para código / para datos: acceso en un ciclo, sin caché |
| Ejecución desde PSRAM (XiP) | Copiar el código de la flash a la PSRAM al arrancar, para que escribir la flash no detenga la CPU |
| ULP | Coprocesador de bajo consumo del ESP32-S3; aquí, un RISC-V que corre su propio programa |
| *Seqlock* | Forma de publicar datos sin candados: el escritor marca con un contador cuándo está escribiendo y el lector repite si lo encontró a medias |
