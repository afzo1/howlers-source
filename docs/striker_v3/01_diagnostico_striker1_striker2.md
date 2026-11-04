# Entrega 1 · Sección 1 — Diagnóstico de referencia de Striker 1 y Striker 2

**Proyecto:** firmware unificado Striker v3 (ESP32-S3) · Howlers FC / Robosyn · Universidad de Antioquia
**Revisión analizada:** `0451897` de `afzo1/howlers-source` (idéntica a `ravelasqvel/howlers-source@main` del 29-10-2026)
**Placa del proyecto:** YD-ESP32-S3 N16R8 de VCC-GND Studio (serigrafía «YD-ESP32-23 2022-V1.3»)
**Fecha:** 4 de noviembre de 2026

---

## 0. Alcance y método

| App | Robot | Rol | Peso en el diagnóstico |
|---|---|---|---|
| [`striker_v1/robot-mov`](../../firmware/apps/striker_v1/robot-mov) | Striker 1 | Firmware del robot completo (3 ruedas, UDP) | Principal |
| [`striker_v1/wheel-movement`](../../firmware/apps/striker_v1/wheel-movement) | Striker 1 | Banco de una rueda (calibración y sintonía) | Secundario |
| [`striker_v2/Robot-Control`](../../firmware/apps/striker_v2/Robot-Control) | Striker 2 | Control de velocidad de las 3 ruedas, órdenes C1/C2/C3 por UDP | Principal |
| [`striker_v2/Robot-Navigation`](../../firmware/apps/striker_v2/Robot-Navigation) | Striker 2 | Control en cascada cuerpo/rueda con fusión IMU (etapa más avanzada) | Principal |
| [`striker_v2/Velocity-Control`](../../firmware/apps/striker_v2/Velocity-Control) | Striker 2 | Banco de un motor (identificación y fusión) | Secundario |

También se revisaron los drivers de `firmware/components/` que esas apps enlazan y las estaciones base de `software/ground_station/striker1_proxy` y `striker2_udp_server`. El banco `bench_pid_autotuning` y la suite de estimación se analizan en el siguiente paso.

**Método.**
1. Lectura del código, de los `CMakeLists.txt` y de los `sdkconfig` de cada app.
2. Compilación de prueba de todas las apps con ESP-IDF v5.5.1 sobre una copia aislada del repositorio ([Anexo A](#anexo-a--reproducibilidad)). Los errores encontrados son de código fuente: aparecerían igual con las versiones originales (5.2.5 y 5.4.0).
3. Análisis de los objetos compilados para listar, por función, las operaciones `double` emuladas por software.
4. Contraste con los README de cada app (sección 5).

**Severidad.** **P0**: impide compilar o es un riesgo físico. **P1**: rompe el tiempo real, la concurrencia o una función. **P2**: deuda técnica o de mantenibilidad. Cada hallazgo enlaza el archivo y la línea donde se observa.

---

## 1. Resumen ejecutivo

1. **Ninguna de las cinco apps de Striker compila hoy** en el monorepo (sección 2). Solo `bench_pid_autotuning` compila.
2. **No existe la partición de núcleos que pide la guía.** `robot-mov` fija la tarea WiFi del IDF en el Core 1, el mismo del lazo de control. En Striker 2 ninguna tarea tiene afinidad.
3. **Hay datos compartidos sin protección en ambos robots.** En Striker 1 son la orden de movimiento y su duración. En Striker 2 es la trayectoria: el mutex `mtx_traj` se crea pero nunca se toma.
4. **La temporización no es determinista.** Striker 1 usa `vTaskDelay`, cuyo periodo salta de 2 a 3 ms con interferencia. Striker 2 dispara el lazo con `esp_timer`, pero ese despacho comparte el Core 0 con WiFi, y el lazo imprime por UART cada 10 ms. Ninguna app mide WCET, jitter ni pila.
5. **El actuador es el cuello de botella físico.** Los ESC reciben pulsos a 50 Hz y los comparadores se recargan al inicio de cada periodo. Con el PID a 500 Hz (S1) o 1 kHz (S2), 9 de cada 10 (S1) o 19 de cada 20 (S2) salidas del PID nunca llegan al motor. La orden además tiene solo 71–86 niveles distintos (pasos de 20 µs).
6. **La medición de velocidad es el segundo cuello de botella.** El AS5600 se lee como tensión analógica y el ángulo se deriva: 1 mV de error equivale a 1,2 rad/s (S1) o 2,4 rad/s (S2). Por eso se filtra mucho: frecuencia de corte de ≈ 8,4 Hz en S1 y ≈ 4,8 Hz en S2.
7. **Hay aritmética `double` emulada dentro de los lazos**, confirmada en el binario. Durante una trayectoria, Striker 2 ejecuta en cada iteración de 1 kHz 5 divisiones y 7 multiplicaciones `double`, además de un `sin` y un `cos` en doble precisión.
8. **Hay errores funcionales y de seguridad.**
   - Striker 1 usa la distancia como velocidad.
   - El temporizador de trayectoria de Striker 1 no se reinicia entre órdenes.
   - `Robot-Control` arranca moviéndose 50 cm sin que nadie lo ordene.
   - `Robot-Navigation` recibe los comandos UDP y los descarta.
   - Ningún firmware detiene el robot si se pierde la comunicación.
9. **La reutilización no funciona como tal.**
   - `components/` no contiene componentes ESP-IDF: no hay ningún `CMakeLists.txt`.
   - Hay 8 copias de `platform_esp32s3.c`.
   - La misma función devuelve grados en una variante y radianes en otra.
   - La fusión de `Robot-Navigation` se perdió en la migración al monorepo; sigue existiendo en el repositorio original.
10. **La placa tiene lo que pide el Eje 4, pero los pines de los firmwares viejos chocan con ella.** La YD-ESP32-S3 N16R8 trae 16 MB de flash y 8 MB de PSRAM octal, que ninguna app configura. Los dos robots sacan señales de motor por pines de *strapping* (GPIO0 y GPIO45 en S1; GPIO3 y GPIO46 en S2), y S1 usa además una línea del USB nativo (GPIO20) (sección 3.3, M5).
11. **Hay piezas aprovechables:** los buzones con `xQueueOverwrite` y la cascada de `Robot-Navigation`, el lazo disparado por temporizador y el PID discreto de Striker 2, el driver MCPWM y el uso de Kconfig en `wheel-movement` (sección 6).

---

## 2. Estado de compilación

Compilación con ESP-IDF v5.5.1, en una copia aislada, usando `ninja -k 0` para obtener todos los errores.

| App | Resultado | Causa | Evidencia |
|---|---|---|---|
| `robot-mov` | ❌ Falla al enlazar | `get_ip_address` está definida en dos archivos que se compilan juntos | [`wifi_lib.c:38`](../../firmware/components/comms/wifi_udp/v1_striker1/wifi_lib.c#L38) y [`platform_esp32s3.c:514`](../../firmware/components/platform_hal/v1_striker1/platform_esp32s3.c#L514) |
| `wheel-movement` | ❌ No compila | Tres bloques de marcadores de conflicto de git y un prototipo sin `;` | [`main.c:114`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L114), [`:464`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L464), [`:654`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L654), [`:251`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L251) |
| `Robot-Control` | ❌ No compila | Marcadores de conflicto de git en `app_main` | [`main.c:37-48`](../../firmware/apps/striker_v2/Robot-Control/main/main.c#L37-L48) |
| `Robot-Navigation` | ❌ No compila | Usa una API de fusión (`senfusion_config_t`, `senfusion_update1/2`, `senfusion_get_state`, `calc_fordwkinematics`) que no existe en el componente compartido `pid_lib/v2_striker2`. Sí existe en el repositorio original [`MaverickST/RoboCup_Striker2`](https://github.com/MaverickST/RoboCup_Striker2) (commit `e182de0`, `Robot-Navigation/components/control/senfusion.h`) | [`tasks.c:144-205`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L144-L205) |
| `Velocity-Control` | ❌ No configura | Declara como directorio de cabeceras `components/actuators/led_status/v2_striker2`, que no existe | [`main/CMakeLists.txt:31`](../../firmware/apps/striker_v2/Velocity-Control/main/CMakeLists.txt#L31) |
| `bench_pid_autotuning` | ✅ Compila | Se analiza en el siguiente paso | — |

**Consecuencia práctica.** Para medir la línea base del firmware viejo (jitter, WCET) y comparar en la Entrega 2, primero hay que hacer arreglos mínimos en una rama aparte, sin rediseñar nada.

---

## 3. Inventario de concurrencia

### 3.1 Tareas activas de las apps principales

Los stacks se dan en bytes: en ESP-IDF el tamaño de pila se expresa en bytes, no en palabras.

**Striker 1 — `robot-mov`** (tick de 1 kHz)

| Tarea | Núcleo | Prioridad | Stack | Activación |
|---|---|---:|---:|---|
| `rwh/lwh/bwh_control_task` (×3, una por rueda) | 1 | 9 | 4096 | `vTaskDelay(2 ticks)` |
| `distance_task` | 1 | 8 | 4096 | `vTaskDelay(10 ticks)` |
| `UDPServer` | 1 | 8 | **2048** | `recvfrom` bloqueante |
| `right/left/back_encoder_task` (×3) | 0 | 8 | 4096 | `vTaskDelay(2 ticks)` |
| Tarea WiFi del IDF | **1** ([`sdkconfig:1119`](../../firmware/apps/striker_v1/robot-mov/sdkconfig#L1119)) | 23 | — | Eventos del radio |
| Tarea TCP/IP (LwIP) del IDF | Sin afinidad ([`sdkconfig:1421`](../../firmware/apps/striker_v1/robot-mov/sdkconfig#L1421)) | 18 | — | Paquetes |

Fuente: [`main.c:216-280`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L216-L280).

**Striker 2 — `Robot-Control`** (tick de 1 kHz)

| Tarea | Núcleo | Prioridad | Stack | Activación |
|---|---|---:|---:|---|
| `uart_event_task` | Sin afinidad | **20** | 4096 | Cola de eventos UART |
| `wifi_task` (red + UDP) | Sin afinidad | **15** | 4096 | `recvfrom` |
| `control_bldc_task` | Sin afinidad | 12 | 8192 | Semáforo binario liberado por `esp_timer` cada 1 ms |
| Tarea `esp_timer` del IDF (ejecuta el callback del lazo) | 0 | 22 | — | — |
| Tarea WiFi del IDF | 0 | 23 | — | — |

Fuente: [`tasks.c:6-18`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L6-L18).

**Striker 2 — `Robot-Navigation`** (tick de **100 Hz**, [`sdkconfig:1333`](../../firmware/apps/striker_v2/Robot-Navigation/sdkconfig#L1333))

| Tarea | Núcleo | Prioridad | Stack | Activación |
|---|---|---:|---:|---|
| `wifi_task` (red + UDP) | Sin afinidad | **24** | 10240 | `recvfrom` (descarta lo recibido) |
| `uart_event_task` | Sin afinidad | 20 | 4096 | Cola UART |
| `bldc_control_task` (lazo de rueda) | Sin afinidad | 12 | 6144 | Semáforo liberado por `esp_timer` cada 1 ms |
| `robot_control_task` (lazo de cuerpo) | Sin afinidad | 11 | 6144 | Cola de la IMU |
| `bno055_task` | Sin afinidad | 9 | 4096 | Notificación desde un `esp_timer` de 10 ms |

Fuente: [`tasks.c:6-18`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L6-L18).

Las prioridades de las tareas del IDF salen de la documentación de rendimiento de ESP-IDF (WiFi = 23) y de `esp_task.h` (`esp_timer` = `configMAX_PRIORITIES − 3` = 22). Los bancos usan esquemas distintos:
- `wheel-movement`: tres tareas sin afinidad; UART con prioridad 10, por encima del control (9).
- `Velocity-Control`: siete tareas; la UART tiene prioridad 1 y el registro reserva un stack de 60 KB.

No hay una política de prioridades común a las apps.

### 3.2 Uso de los mecanismos que pide la guía

Conteo de archivos de `striker_v1/` y `striker_v2/` que los usan:

| Mecanismo | Uso actual |
|---|---|
| `xTaskCreatePinnedToCore` | 1 archivo (`robot-mov`) |
| `vTaskDelayUntil` / `xTaskDelayUntil` | 0 |
| Notificaciones directas | `Velocity-Control`, y `Robot-Navigation` (desde un `esp_timer`) |
| Colas con `xQueueOverwrite` (buzones) | Solo `Robot-Navigation` |
| Mutex | Striker 2 (parcial: uno no se usa nunca y otro solo cubre un wrapper de `printf`) |
| Event groups | Solo dentro de la HAL WiFi de Striker 2 |
| Stream / message buffers | 0 |
| `IRAM_ATTR`, `DRAM_ATTR`, `heap_caps_*`, PSRAM | 0 (`CONFIG_SPIRAM` desactivado en todas las apps) |
| `gptimer` | 0 |
| `esp_cpu_get_cycle_count`, `uxTaskGetStackHighWaterMark`, `vTaskGetRunTimeStats` | 0 (`CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS` desactivado) |
| Coprocesador ULP | 0 (tampoco hay monitoreo de batería) |

### 3.3 Placa del proyecto: YD-ESP32-S3 N16R8

Es una placa de VCC-GND Studio compatible con la ESP32-S3-DevKitC-1, con el módulo ESP32-S3-WROOM-1-N16R8.

| Característica | Valor | Consecuencia para v3 |
|---|---|---|
| Memoria | 16 MB de flash y 8 MB de PSRAM octal (OPI) | El mapa de memoria del Eje 4 puede usar PSRAM. Hay que elegir el modo octal (`CONFIG_SPIRAM_MODE_OCT`): ESP-IDF trae el modo quad por defecto, y con él la PSRAM no se detecta. |
| Pines del bus de memoria | GPIO26–32 (flash y PSRAM) y GPIO33–37 (líneas extra de la PSRAM octal) | No asignarlos a periféricos. |
| Pines de *strapping* | GPIO0, GPIO3, GPIO45, GPIO46 | Su nivel se lee al reiniciar: GPIO0 y GPIO46 fijan el modo de arranque, y GPIO45 el voltaje de la flash (en alto la alimenta a 1,8 V, pero la flash y la PSRAM del N16R8 son de 3,3 V). Evitar actuadores ahí. |
| USB nativo | Segundo conector USB-C, en GPIO19 (D−) y GPIO20 (D+) | Si se usan como GPIO se pierde el USB-JTAG. |
| USB-UART | Chip CH343P en UART0 (GPIO43 TX, GPIO44 RX), con LED en cada línea | Puerto de consola y de flasheo. |
| LED RGB WS2812 | GPIO48, a través del puente de soldadura «RGB», abierto de fábrica según el esquemático V1.4 | Si se suelda el puente, GPIO48 queda compartido con el LED. Conviene verificarlo en la placa V1.3. |

Fuentes: [repositorio de VCC-GND para la YD-ESP32-S3](https://github.com/vcc-gnd/YD-ESP32-S3), [esquemático V1.4](https://mischianti.org/wp-content/uploads/2023/08/YD-ESP32-S3-SCH-V1.4.pdf), y en la documentación de ESP-IDF v5.5.1 las notas de GPIO del ESP32-S3 (`peripherals/gpio/esp32s3.inc`, líneas 252-254), el resumen de eFuses (`espefuse_summary_ESP32-S3.rst`, líneas 186-187) y `components/esp_psram/esp32s3/Kconfig.spiram`.

---

## 4. Hallazgos por eje

### 4.1 Eje 1 — Concurrencia (FreeRTOS SMP)

**C1 · P1 · No hay partición de núcleos, y en S1 está invertida.**
- *Evidencia.* `robot-mov` fija la tarea WiFi del IDF en el Core 1 ([`sdkconfig:1119`](../../firmware/apps/striker_v1/robot-mov/sdkconfig#L1119)), donde también corren las tres tareas de control, el servidor UDP y `distance_task` ([`main.c:216-218`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L216-L218), [`:280`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L280)). La adquisición de encoders queda en el Core 0 ([`:244-246`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L244-L246)). En todas las apps la tarea TCP/IP no tiene afinidad, y en Striker 2 todas las tareas se crean con `xTaskCreate`.
- *Impacto.* WiFi (prioridad 23) y LwIP (18) pueden expropiar el lazo de control en cualquier instante, con duraciones que dependen del tráfico de red.
- *Para v3.* Core 1 exclusivo para el lazo. WiFi, LwIP y `esp_timer` fijados al Core 0 por `sdkconfig`. Todas las tareas propias creadas con afinidad explícita.

**C2 · P1 · Datos compartidos sin protección.**
- *Striker 1.* `movement`, `x_vel`, `y_vel` y `goal_time` son globales ([`control_main.c:85-87`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L85-L87)). Las escribe el servidor UDP ([`:321-323`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L321-L323)) y las leen el control ([`:193-196`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L193-L196)) y `distance_task` ([`:244-245`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L244-L245)). La orden se publica (`movement = LINEAR`) antes de calcular las velocidades, así que una rueda puede aplicar velocidades de la orden anterior.
- *Striker 2.* La tarea de red escribe `gTraj` ([`functions.c:530-576`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L530-L576)) y el control la lee y la modifica (`ktime++`, [`:486-528`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L486-L528)). Ninguna de las dos toma `mtx_traj`, que se crea ([`tasks.c:60`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L60)) pero no se usa en ningún lugar. Como las tareas no tienen afinidad, la carrera puede ocurrir en paralelo real en los dos núcleos.
- *Para v3.* La orden completa (`struct`) viaja por un buzón (cola de longitud 1 con `xQueueOverwrite`). Cada dato tiene un único propietario.

**C3 · P1 · Productor y consumidor sin sincronizar (S1).**
- *Evidencia.* Las tareas de encoder (Core 0) y de control (Core 1) duermen cada una con su propio `vTaskDelay` ([`control_main.c:114`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L114), [`:229`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L229)). El control lee `encoder_data->velocity` sin saber si es nueva ([`:189`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L189)).
- *Impacto.* Puede usar la misma muestra dos veces o saltarse una, y la latencia sensor→actuador varía entre 0 y 2 ms. Cada rueda corre en su propia tarea, así que las tres actualizan en instantes distintos y con consignas leídas en momentos distintos.
- *Para v3.* Una sola tarea de control que en cada periodo lee, estima, controla y actúa para las tres ruedas.

**C4 · P1 · Las prioridades no siguen ninguna política.**
- *Evidencia.*
  - `Robot-Control`: UART (20) > red (15) > control (12) ([`tasks.c:6-18`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L6-L18)).
  - `Robot-Navigation`: la tarea de red tiene prioridad 24, la máxima, por encima del driver WiFi (23) y de la tarea `esp_timer` (22) que dispara el lazo de 1 kHz ([`tasks.c:6`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L6)).
  - `wheel-movement`: UART (10) > control (9).
- *Para v3.* Asignación Rate Monotonic documentada en la tabla de tiempo real.

**C5 · P1 · Entrada/salida bloqueante dentro del lazo (S2).**
- *Evidencia.* `bldc_control_task` llama a `wrap_printf` cada 10 iteraciones ([`tasks.c:236-239`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L236-L239)). Esa función toma un mutex y hace `vprintf` + `fflush` ([`functions.c:678-690`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L678-L690)). Otras rutas imprimen con `printf` sin el mutex ([`:615`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L615)), así que ese mutex tampoco garantiza la exclusión.
- *Impacto.* La herencia de prioridad no acorta el tiempo de transmisión serie.
- *Para v3.* La telemetría sale por un stream/message buffer que vacía una tarea del Core 0.

**C6 · P1 · ADC compartido con manejo de error fatal (S1).**
- *Evidencia.* Las tres tareas de encoder usan el mismo handle de ADC1 ([`main.c:110-135`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L110-L135)). En ESP-IDF 5.x, `adc_oneshot_read` es seguro entre tareas porque usa un *try-lock*: si otra tarea tiene el ADC, devuelve `ESP_ERR_TIMEOUT` en vez de esperar (`components/esp_adc/adc_oneshot.c`, líneas 207-208 en v5.5.1). La HAL envuelve la lectura en `ESP_ERROR_CHECK` ([`platform_esp32s3.c:404`](../../firmware/components/platform_hal/v1_striker1/platform_esp32s3.c#L404)).
- *Impacto.* Una colisión no corrompe la lectura, pero **reinicia el microcontrolador**. La probabilidad es baja porque la ventana es corta, pero no es cero.
- *Para v3.* Una sola tarea lee los tres canales (o se usa el ADC continuo con DMA). Los errores de sensor se marcan como muestra inválida en vez de abortar.

**C7 · P1 · Callbacks de `esp_timer` usados como si fueran interrupciones (S2).**
- *Evidencia.* Los callbacks corren en la tarea `esp_timer`, no en una ISR, y aun así llaman a `xSemaphoreGiveFromISR` / `vTaskNotifyGiveFromISR` e ignoran la bandera de cambio de contexto ([`functions.c:448-452`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L448-L452); [`Robot-Navigation/functions.c:213`](../../firmware/apps/striker_v2/Robot-Navigation/main/functions.c#L213)). En `Robot-Navigation`, el callback del timer de sistema escribe logs y detiene motores ([`functions.c:220-221`](../../firmware/apps/striker_v2/Robot-Navigation/main/functions.c#L220-L221), [`:236`](../../firmware/apps/striker_v2/Robot-Navigation/main/functions.c#L236)).
- *Impacto.* La documentación de ESP-IDF advierte que los callbacks se ejecutan en serie y que uno lento retrasa a todos los demás timers (`esp_timer.rst`, líneas 74-78). Aquí eso incluye el timer de 1 kHz del lazo de motores.
- *Para v3.* `gptimer` con ISR (en `IRAM`) que notifica a la tarea de control con `vTaskNotifyGiveFromISR` + `portYIELD_FROM_ISR`.

**C8 · P2 · Arranque sin barrera de sincronización.**
- *Evidencia.* `robot-mov` crea el control antes que los encoders ([`main.c:216`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L216) frente a [`:244`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L244)). La alternativa `HEAD` del conflicto en `Robot-Control` crea las tareas antes que los objetos del kernel y las vuelve a crear al final ([`main.c:37-48`](../../firmware/apps/striker_v2/Robot-Control/main/main.c#L37-L48), [`:88`](../../firmware/apps/striker_v2/Robot-Control/main/main.c#L88)). También hay estado `static` compartido entre instancias, como el `t` de `circular_movement` ([`mov_calculation.c:14`](../../firmware/apps/striker_v1/robot-mov/main/mov_calculation.c#L14)).
- *Para v3.* Inicialización ordenada con un event group de arranque (`xEventGroupSync`) y funciones reentrantes.

### 4.2 Eje 2 — Arquitectura y reutilización

**A1 · P1 · `components/` no contiene componentes ESP-IDF.**
- *Evidencia.* No hay ningún `CMakeLists.txt` en `firmware/components/` (sin contar los proyectos de ejemplo `standalone`). Las apps hacen `file(GLOB …)` con rutas relativas `../../../../components/...` y compilan todo dentro de `main` ([`robot-mov/main/CMakeLists.txt`](../../firmware/apps/striker_v1/robot-mov/main/CMakeLists.txt)).
- *Impacto.* No hay fronteras entre módulos ni dependencias declaradas (`REQUIRES`), todos los headers son visibles desde todas partes y aparecen colisiones de símbolos (`get_ip_address`, sección 2).
- *Para v3.* Componentes reales con su `CMakeLists.txt` e `idf_component_register`, enlazados con `EXTRA_COMPONENT_DIRS`.

**A2 · P1 · Variantes por robot en lugar de abstracciones.**
- *Evidencia.* Hay 3 variantes de `platform_hal`, 3 de `bldc_pwm`, 4 de `pid_lib`, 3 de `as5600` y 3 de `vl53l1x`. Existen 8 copias de `platform_esp32s3.c`, de las cuales 7 son distintas entre sí.
- *Impacto.* Cada corrección hay que replicarla a mano, y en la práctica no se hace.
- *Para v3.* Una sola implementación por driver detrás de interfaces (`ISensorIMU`, `IWheelEncoder`, `IMotor`, `IController`). El robot (S1/S2) se elige por Kconfig.

**A3 · P1 · La misma API tiene distinta semántica según la variante.**
- *Evidencia.*
  - `AS5600_ADC_GetAngle` devuelve grados en v1 ([`as5600_lib.c:29`](../../firmware/components/sensors/as5600/v1_striker1/as5600_lib.c#L29)) y radianes en v2 ([`as5600_lib.c:81`](../../firmware/components/sensors/as5600/v2_striker2/as5600_lib.c#L81)).
  - `estimate_velocity_encoder` cambió de firma ([`sensor_fusion.h:71`](../../firmware/components/control/sensor_fusion/v1_striker1/include/sensor_fusion.h#L71)), pero `wheel-movement` la sigue llamando con 3 argumentos ([`main.c:372`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L372)).
  - La fusión de `Robot-Navigation` quedó sobrescrita por la de `Robot-Control` en el componente compartido.
- *Impacto.* Tres de los cinco fallos de compilación vienen de esta deriva sin versionado o de la falta de higiene al fusionar ramas.
- *Para v3.* Unidades del SI explícitas en las interfaces y pruebas de compilación en integración continua.

**A4 · P2 · Las capas se mezclan.**
- *Evidencia.*
  - La HAL de Striker 1 inicializa WiFi con credenciales ([`platform_esp32s3.c:467-512`](../../firmware/components/platform_hal/v1_striker1/platform_esp32s3.c#L467-L512)), y `comms/wifi_udp` lo vuelve a hacer ([`wifi_lib.c:3-36`](../../firmware/components/comms/wifi_udp/v1_striker1/wifi_lib.c#L3-L36)). `robot-mov` llama a las dos ([`main.c:62`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L62) y [`:156`](../../firmware/apps/striker_v1/robot-mov/main/main.c#L156)).
  - La fusión sensorial de Striker 2 vive dentro de `pid_lib`.
  - En `functions.c` conviven inicialización de drivers, máquina de estados, trayectorias y parseo de comandos.
- *Para v3.* Las 4 capas de la guía (HAL → drivers → dominio → aplicación), con dependencias solo hacia abajo.

**A5 · P2 · La configuración está dispersa y es incoherente.**
- *Evidencia.*
  - El radio de rueda vale 3,0 cm en [`control_main.h:27`](../../firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L27) y 3,25 cm en [`mov_calculation.h:12`](../../firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L12).
  - `PI` está definido dos veces con valores distintos ([`control_main.h:73`](../../firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L73) y [`mov_calculation.h:9`](../../firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L9)).
  - La IMU TM151 se configura a 115 600 baudios, probablemente un error por 115 200 ([`control_main.h:40`](../../firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L40)).
  - Hay credenciales WiFi en headers de un repositorio público ([`wifi_lib.h:27-28`](../../firmware/components/comms/wifi_udp/v1_striker1/include/wifi_lib.h#L27-L28), [`types.h:49-50`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L49-L50)) y una IP fija en el proxy ([`proxy_udp.py:6`](../../software/ground_station/striker1_proxy/proxy_udp.py#L6)).
- *Lo bueno.* `wheel-movement` ya usa `Kconfig.projbuild`.
- *Para v3.* Kconfig para la configuración de compilación y NVS para las calibraciones.

**A6 · P2 · Los modos son programas separados.** La calibración de encoders y ESC y la prueba de ruedas son `app_main` alternativos en archivos que no se compilan ([`encoder_calibration.c:24`](../../firmware/apps/striker_v1/robot-mov/main/encoder_calibration.c#L24), [`escs_calibration.c:31`](../../firmware/apps/striker_v1/robot-mov/main/escs_calibration.c#L31), [`test_wheels.c:42`](../../firmware/apps/striker_v1/robot-mov/main/test_wheels.c#L42)). *Para v3:* estados Calibración y Autotuning dentro de la máquina de estados.

**A7 · P2 · Código muerto o sin terminar, y C poco seguro.**
- La fusión sensorial de Striker 1 nunca se invoca, la IMU está desactivada y los comandos `C`/`R` se parsean pero no hacen nada ([`control_main.c:325-338`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L325-L338)).
- `wheel-movement` declara bitfields `int8_t x : 1`, que solo pueden valer 0 o −1 ([`main.c:213-218`](../../firmware/apps/striker_v1/wheel-movement/main/main.c#L213-L218)).
- Hay macros sin paréntesis, como `#define DELTA PI/6.0` ([`mov_calculation.h:10`](../../firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L10)).

### 4.3 Eje 3 — Tiempo real

**T1 · P1 · Striker 1 no tiene activación periódica absoluta.**
- *Evidencia.* Las tareas de encoder y de control usan `vTaskDelay(SAMPLE_TIME / portTICK_PERIOD_MS)` ([`control_main.c:114`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L114), [`:229`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L229)). Con un tick de 1 kHz la liberación queda alineada al tick: el periodo es de **2 ms** mientras la iteración, incluida la interferencia, termine antes del siguiente tick, y pasa a **3 ms (+50 %)** cuando lo cruza, por ejemplo por WiFi en el mismo núcleo (C1).
- *Impacto.* Las ganancias del PID incremental son por muestra (`Ki` no se escala con `Ts`, [`pid_ext.c:72-75`](../../firmware/components/control/pid_lib/v1_striker1/pid_ext.c#L72-L75)) y el estimador supone Δt = 2 ms fijo ([`control_main.c:25`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L25)). Cada periodo de 3 ms cambia la ganancia efectiva del lazo.

**T2 · P1 · Striker 2 tiene un periodo absoluto, pero sin garantías.**
- *Lo bueno.* Disparar el lazo desde `esp_timer` con un semáforo ([`tasks.c:183-191`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L183-L191), [`:211`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L211)) evita la deriva.
- *Los problemas.*
  - El despacho pasa por la tarea `esp_timer` (prioridad 22, Core 0), que cede ante WiFi (23).
  - El semáforo binario pierde periodos sin avisar cuando hay un desborde.
  - La velocidad se calcula con Δt = 1 ms fijo ([`functions.c:483`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L483)) y el tiempo de la trayectoria se mide contando iteraciones ([`:517-518`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L517-L518), [`:527`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L527)). Un periodo perdido duplica la velocidad medida en esa muestra y alarga la trayectoria.
- *Para v3.* Detectar desbordes (contador de periodos perdidos) y medir Δt real con `esp_timer_get_time()`.

**T3 · P1 · `Robot-Navigation` usa un tick de 100 Hz** ([`sdkconfig:1333`](../../firmware/apps/striker_v2/Robot-Navigation/sdkconfig#L1333)), mientras el resto de apps usa 1 kHz.
- *Impacto.* El reparto de CPU entre tareas de igual prioridad se hace en porciones de 10 ms, `pdMS_TO_TICKS(x)` vale 0 para x < 10 ms, y los timeouts de I2C convertidos con `portTICK_PERIOD_MS` cambian de valor.
- *Para v3.* Un tick único de 1 kHz para todo el firmware.

**T4 · P1 · Cuello de botella del actuador: 50 Hz.**
- *Evidencia.* El PWM de los ESC es de 50 Hz ([`control_main.h:60`](../../firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L60), [`types.h:80`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L80)), y los comparadores se recargan solo al inicio de cada periodo (`update_cmp_on_tez`, [`bldc_pwm.c v1:49`](../../firmware/components/actuators/bldc_pwm/v1_striker1/bldc_pwm.c#L49), [`v2:47`](../../firmware/components/actuators/bldc_pwm/v2_striker2/bldc_pwm.c#L47)). El PID corre a 500 Hz (S1) o 1 kHz (S2, [`types.h:107`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L107)).
- *Impacto.* Solo la última orden antes de cada inicio de periodo llega al motor: el sistema real es un retenedor de orden cero de 20 ms. Mientras tanto, la integral acumula 10 o 20 veces por cada actualización efectiva, lo que favorece oscilaciones y ciclos límite.
- *Para v3, a evaluar:* (a) un protocolo o frecuencia de ESC más rápidos, si el ESC instalado lo admite; (b) un lazo de velocidad a la tasa del actuador, sincronizado con el evento TEZ del MCPWM (callback del timer MCPWM), manteniendo la estimación a mayor tasa.

**T5 · P1 · Cuello de botella del sensor: derivar un ángulo analógico.**
- *Evidencia.* El AS5600 se lee como tensión (330–2970 mV ↔ 360°): 1 mV equivale a 0,136°. Derivando, 1 mV de error se convierte en 1,19 rad/s con Δt = 2 ms (S1) y en 2,38 rad/s con Δt = 1 ms (S2).
- *Consecuencia.* Se filtra con fuerza:
  - Striker 1 usa un exponencial con β = 0,9 ([`sensor_fusion.c:74`](../../firmware/components/control/sensor_fusion/v1_striker1/sensor_fusion.c#L74)): constante de tiempo τ ≈ 19 ms y corte f_c ≈ 8,4 Hz.
  - Striker 2 usa un Kalman escalar con Q = 0,001 y R = 1,12 ([`types.h:123-124`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L123-L124), [`senfusion.c:264-275`](../../firmware/components/control/pid_lib/v2_striker2/senfusion.c#L264-L275)). Converge a una ganancia fija K ≈ 0,029, equivalente a un pasa-bajas con τ ≈ 33,5 ms y f_c ≈ 4,8 Hz.
- *Impacto.* Ese retardo limita el ancho de banda alcanzable del lazo de velocidad más que la CPU.
- *Para v3, a evaluar:* leer el AS5600 en digital por I2C (los tres comparten la dirección 0x36, así que hace falta multiplexor o buses separados) o por su salida PWM con captura MCPWM; derivar a menor tasa; o usar un observador de seguimiento de 2 estados.

**T6 · P1 · No hay medición ni código representativo.**
- *Evidencia.* No se usa el contador de ciclos, ni la marca de agua de pila, ni las estadísticas de ejecución (desactivadas en `sdkconfig`), ni pulsos en GPIO. Todas las apps compilan con `-Og` (`CONFIG_COMPILER_OPTIMIZATION_DEBUG`, p. ej. [`robot-mov/sdkconfig:516`](../../firmware/apps/striker_v1/robot-mov/sdkconfig#L516)), así que un WCET medido hoy no representaría el binario final. No hay periodos ni plazos documentados.
- *Para v3.* Plan de instrumentación y compilación con `-O2` para las mediciones.

**T7 · P2 · Hay tres versiones de ESP-IDF distintas** (5.2.5, 5.4.0 y 5.5.1). *Para v3:* fijar una sola versión.

### 4.4 Eje 4 — Memoria, IRAM y ULP

**M1 · P1 · No hay ninguna decisión de ubicación de memoria** (ni `IRAM_ATTR`, ni `DRAM_ATTR`, ni `heap_caps`).
- *Por qué importa en v3.* En el ESP32-S3, mientras se escribe o borra la flash, por ejemplo al guardar ganancias en NVS, la caché se deshabilita, salvo que se active `CONFIG_SPIRAM_XIP_FROM_PSRAM` (`spi_flash_concurrency.rst`, líneas 20-24). Durante ese tiempo solo puede ejecutarse código en IRAM con datos en DRAM. El autotuning de v3 escribirá en NVS con el lazo en marcha.

**M2 · P2 · La placa tiene PSRAM, pero ninguna app la configura.** La YD-ESP32-S3 N16R8 trae 8 MB de PSRAM octal (sección 3.3), y ninguna app activa `CONFIG_SPIRAM`. Además, `robot-mov` y el banco de autotuning están configurados con 2 MB de flash, aunque la placa tiene 16 MB. *Para v3:* activar la PSRAM en modo octal, configurar los 16 MB de flash y definir una tabla de particiones propia (NVS para calibraciones y ganancias).

**M3 · P2 · Los stacks están dimensionados a ojo.**
- `robot-mov` usa 4 KB por tarea, y solo 2 KB para la tarea UDP, que llama a `sscanf` y a `ESP_LOGI` con flotantes.
- La tarea de registro de Striker 2 (`save_data_task`) reserva un buffer de 45 KB en su propia pila ([`Robot-Control/tasks.c:255`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L255), donde está desactivada). `Velocity-Control` sí la crea, con 60 KB de stack ([`tasks.c:25`](../../firmware/apps/striker_v2/Velocity-Control/main/tasks.c#L25)). Ese buffer es un candidato natural a PSRAM en v3.
- No hay mediciones de marca de agua.

**M4 · P2 · El ULP no tiene antecedentes.** Ningún firmware monitorea la batería. La tarea ULP de v3 parte de cero y exige reservar un GPIO con ADC accesible desde el dominio RTC.

**M5 · P1 · Los firmwares viejos usan pines problemáticos en esta placa.**
- *Striker 1.* El motor trasero usa GPIO45 (PWM) y GPIO0 (sentido), los dos de *strapping*. El motor derecho usa GPIO20, que es la línea D+ del USB nativo. El sentido del motor izquierdo va por GPIO48, compartido con el LED RGB si el puente está soldado ([`control_main.h:51-58`](../../firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L51-L58)).
- *Striker 2.* El PWM del motor 3 sale por GPIO46 y el sentido del motor 2 por GPIO3, ambos de *strapping* ([`types.h:89-95`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L89-L95)). `Velocity-Control` usa GPIO3 para el PWM ([`types.h:65`](../../firmware/apps/striker_v2/Velocity-Control/include/types.h#L65)).
- *Impacto.* Si el circuito de entrada de un ESC fuerza un nivel durante el encendido, la placa puede arrancar en modo de descarga o, con GPIO45 en alto, alimentar la flash a 1,8 V y no arrancar. Durante el arranque esos pines también pueden cambiar de nivel, y un ESC podría tomar esas transiciones como pulsos. Usar GPIO20 como GPIO desactiva el USB-JTAG.
- *Para v3.* Un mapa de pines que evite GPIO0, 3, 45 y 46, GPIO19 y 20 (si se usa el USB nativo), GPIO26–37 (memoria) y GPIO43 y 44 (consola), y que reserve para el ULP un pin del ADC1 (GPIO1–10, todos del dominio RTC).

### 4.5 Eje 5 — Precisión numérica

**N1 · P1 · Hay aritmética `double` emulada dentro de los lazos.** Se confirmó en los objetos compilados (Anexo A): estas son las llamadas a rutinas de libgcc `__*df*` y a libm en doble precisión que se ejecutan en cada llamada a la función.

| Función | Tasa | Operaciones `double` por llamada | Origen en el código |
|---|---|---|---|
| S1 `estimate_velocity_encoder` | 3 × cada 2 ms | 1 mul, 1 div, 1 comparación (+2 conversiones) | `angle * 3.1415 / 180` y `> 0.15` ([`sensor_fusion.c:66,72`](../../firmware/components/control/sensor_fusion/v1_striker1/sensor_fusion.c#L66-L72)) |
| S1 `linear_movement` | Por orden | 2 mul, 2 div | `angle * PI / 180.0f`, con `PI` en `double` ([`mov_calculation.c:5-9`](../../firmware/apps/striker_v1/robot-mov/main/mov_calculation.c#L5-L9)) |
| S2 `AS5600_ADC_GetAngle` | 3 × cada 1 ms | 1 mul, 1 div, 1 suma | `MAP(..., 6.2831853)` ([`as5600_lib.c:81`](../../firmware/components/sensors/as5600/v2_striker2/as5600_lib.c#L81)) |
| S2 `calculate_motor_speed` | 3 × cada 1 ms | 2 comparaciones (+1 suma o resta cuando el ángulo da la vuelta) | Uso de `M_PI` ([`functions.c:478-479`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L478-L479)) |
| S2 `calculate_motor_setpoints` | 1 × cada 1 ms | 1 `sin` y 1 `cos` en `double`, 2 mul (en la rama recta o en la circular) | [`functions.c:503-504`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L503-L504), [`:517-518`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L517-L518) |
| S2 `calc_invkinematics` | 1 × cada 1 ms | 3 sumas, 2 div, 2 mul, 1 resta | `DELTA` es un literal `double` ([`senfusion.c:279-281`](../../firmware/components/control/pid_lib/v2_striker2/senfusion.c#L279-L281), [`senfusion.h:223`](../../firmware/components/control/pid_lib/v2_striker2/senfusion.h#L223)) |

Durante una trayectoria, cada iteración de 1 kHz de Striker 2 ejecuta 5 divisiones, 7 multiplicaciones, un `sin` y un `cos` en `double`, más unas 13 sumas, restas y comparaciones. Con el robot quieto quedan las de los encoders: 3 divisiones, 3 multiplicaciones y 9 sumas y comparaciones. Un detalle: el compilador pliega las expresiones constantes (el `cosf(DELTA)` de S1 desaparece del binario). El costo aparece cuando una variable `float` se mezcla con un literal `double` o con una función de libm en doble precisión.

*Para v3:* política de solo `float`, con `-Wdouble-promotion` y sufijos `f`, y la cuantificación del costo real en el benchmark de la sección 5.

**N2 · P1 · Unidades incoherentes, compensadas a mano.**
- En Striker 1 la velocidad del encoder sale en rad/s aunque el comentario diga cm/s ([`sensor_fusion.c:73`](../../firmware/components/control/sensor_fusion/v1_striker1/sensor_fusion.c#L73)).
- La consigna aplica N/R y luego divide por 5 sin justificación ([`mov_calculation.c:47-53`](../../firmware/apps/striker_v1/robot-mov/main/mov_calculation.c#L47-L53)): N/(5R) ≈ 0,985, un ajuste empírico.
- En Striker 2, `ROBOT_RADIUS` está comentado como diámetro ([`senfusion.h:222`](../../firmware/components/control/pid_lib/v2_striker2/senfusion.h#L222)).

**N3 · P1 · El PID tiene problemas de discretización.**
- S1 usa ganancias por muestra (T1).
- El PID en ecuación de diferencias de S2 guarda en su historia la salida sin saturar ([`control.c:57-61`](../../firmware/components/control/pid_lib/v2_striker2/control.c#L57-L61) frente a la saturación en [`:75-76`](../../firmware/components/control/pid_lib/v2_striker2/control.c#L75-L76)), lo que produce *windup*.

**N4 · P2 · El actuador está cuantizado y el cambio de sentido no tiene histéresis.**
- En S1 la salida `float` del PID se trunca a `int16_t` ([`control_main.c:219`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L219), [`bldc_pwm.c:115`](../../firmware/components/actuators/bldc_pwm/v1_striker1/bldc_pwm.c#L115)), y luego se mapea a milésimas del periodo. Quedan 86 niveles en S1 y 71 en S2, con pasos de 20 µs.
- La línea de sentido de giro cambia con el signo de la orden ([`bldc_pwm.c:123-128`](../../firmware/components/actuators/bldc_pwm/v1_striker1/bldc_pwm.c#L123-L128)), así que alrededor de velocidad cero conmuta en cada muestra.

**N5 · P2 · No hay punto fijo ni uso de esp-dsp o esp-nn.** Esa es la línea base del benchmark de la sección 5.

### 4.6 Eje 6 — Comandos, rutas y seguridad funcional

**F1 · P0 · Striker 1 usa la distancia como velocidad.**
- *Evidencia.* La web envía `L <sentido> <grados> <velocidad> <distancia>` ([`index.html:87-91`](../../software/ground_station/striker1_proxy/index.html#L87-L91)) y el firmware lo lee en ese orden. Después llama `linear_movement(forward, distance, degrees, …)` ([`control_main.c:323`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L323)), cuyo segundo parámetro es la velocidad ([`mov_calculation.h:22`](../../firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L22)).
- *Impacto.* El robot avanza a |v| = d durante d/v segundos, es decir, recorre d²/v. Con v = 15 cm/s y d = 30 cm recorre 60 cm.

**F2 · P0 · El temporizador de trayectoria de Striker 1 no se reinicia.**
- *Evidencia.* `time_count` es un `static uint16_t` ([`control_main.c:242`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L242)). Sigue incrementándose mientras el robot está quieto ([`:248`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L248)) y solo vuelve a cero al alcanzar la meta ([`:246`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L246)).
- *Impacto.* La duración de la siguiente orden depende de cuánto tiempo estuvo quieto el robot, y el contador da la vuelta a los 65 535 ms. El tiempo se mide contando `vTaskDelay`.

**F3 · P1 · Las rutas están incompletas o en lazo abierto.**
- Striker 1 solo implementa la recta; `C` y `R` llegan desde la web ([`index.html:97-110`](../../software/ground_station/striker1_proxy/index.html#L97-L110)) y se ignoran.
- `Robot-Control` implementa recta, giro y círculo en lazo abierto por tiempo: la duración se calcula como distancia/velocidad ([`functions.c:542-557`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L542-L557)) y no hay realimentación de posición.
- `Robot-Navigation` recibe los datagramas y los descarta: `parse_command` está comentado ([`tasks.c:442`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L442)) y nadie escribe `queue_wifi` ([`:426`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L426)), así que el lazo de cuerpo siempre tiene consigna cero.

**F4 · P0 · Riesgos físicos.**
- `Robot-Control` arranca ejecutando una trayectoria fija de 50 cm a 30 cm/s sin orden del operador ([`tasks.c:197`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L197)).
- Ningún firmware tiene timeout de comandos. Si se cae el WiFi, la última orden sigue hasta que vence su tiempo, y en S1 ese tiempo puede ser infinito: con velocidad 0, `goal_time = d/0` ([`control_main.c:322`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L322)).
- La tarea de red de `Robot-Control` termina ante un error de `recvfrom` y no se recupera ([`tasks.c:364-367`](../../firmware/apps/striker_v2/Robot-Control/main/tasks.c#L364-L367)).

**F5 · P1 · Las entradas no se validan.**
- S1 hace `sscanf("%s")` sobre un `char[16]` sin límite de ancho, dentro de una tarea de 2 KB de stack, con datos que llegan de la red ([`control_main.c:303-305`](../../firmware/apps/striker_v1/robot-mov/main/control_main.c#L303-L305)). Tampoco comprueba el valor de retorno.
- S2 divide por cero con velocidad 0 y convierte el resultado a entero, lo que es comportamiento indefinido ([`functions.c:542`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L542)).
- La consola UART de S2 escribe 4 bytes sobre un `uint16_t` y lee sin límite sobre un buffer de 128 bytes ([`uart_console.c:43-45`](../../firmware/components/comms/uart_console/v2_striker2/uart_console.c#L43-L45)).

**F6 · P2 · Los protocolos son ad hoc.**
- Striker 1 usa `L dir deg vel dist` por UDP 3333 y Striker 2 usa `C1,angle,speed,dist` por UDP 12345 ([`types.h:53`](../../firmware/apps/striker_v2/Robot-Control/include/types.h#L53), [`functions.c:586-613`](../../firmware/apps/striker_v2/Robot-Control/main/functions.c#L586-L613)).
- No hay versión, confirmación, número de secuencia ni telemetría de vuelta: ambas estaciones solo envían.
- *Para v3.* Especificación del protocolo en la sección 5: validación, confirmación, latido y telemetría de pose estimada frente a la referencia.

---

## 5. Correcciones y complementos a los README

| README | Afirmación | Lo que muestra el código |
|---|---|---|
| `robot-mov` §4.3.1 | Las tres tareas pueden «corromper la lectura analógica» del ADC | `adc_oneshot_read` es seguro entre tareas (usa un try-lock). El riesgo real es `ESP_ERR_TIMEOUT` → `ESP_ERROR_CHECK` → reinicio (C6). |
| `robot-mov` §4.3.3 | T_real = 2 ms + t_ejecución + t_interferencia | Con `vTaskDelay` el periodo queda cuantizado al tick: 2 ms o 3 ms, no una suma continua (T1). |
| `robot-mov` §5.2 | El estimador usa un «umbral de zona muerta» | Es una cota superior (acepta solo \|Δ\| < 1 cm) que además descarta las muestras del salto 360°→0°. No hay zona muerta. |
| `robot-mov` TODO 1-B | Integrar `firmware/components/control/state_estimation/` | Esa carpeta no existe. La suite está en `MaverickST/mobile-robot-estimation-suite`. |
| `robot-mov` §1 | La app enlaza los controladores compartidos | No enlaza (sección 2). Además el README no menciona WiFi en el Core 1 (C1) ni los errores F1 y F2. |
| `Robot-Navigation` | «Etapa final» que recibe comandos del servidor local | No compila y descarta los comandos (sección 2, F3). |
| `Robot-Control` | — | Confirmado en lo esencial: marcadores de conflicto, arranque en movimiento, carrera en `gTraj`, mal uso de `FromISR`, *windup*, comparadores a 50 Hz, desborde de la UART. Este documento agrega la evidencia binaria de `double` en el lazo (N1), el ancho de banda del Kalman (T5) y el despacho de `esp_timer` compartido con WiFi en el Core 0 (T2). |

---

## 6. Qué conviene reutilizar (adaptado)

- **Buzones entre capas** (`Robot-Navigation`): colas de un elemento con `xQueueOverwrite` para estado y consignas ([`tasks.c:62-80`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L62-L80), [`:126`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L126), [`:210`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L210), [`:272`](../../firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L272)). Es exactamente el patrón que pide la guía.
- **Cascada cuerpo/rueda** (`Robot-Navigation`): PI de velocidad del cuerpo a 100 Hz sobre PID de rueda a 1 kHz, con fusión encoder + IMU por eje. Hay que recuperar `senfusion` del repositorio original (`e182de0`).
- **Lazo disparado por temporizador** (Striker 2): conservar la idea, pero llevarla a `gptimer` con ISR en el Core 1.
- **PID discreto en ecuación de diferencias** (`control_calc_pid_z`, S2), corrigiendo el *windup*, y el **PID con handle opaco** de S1 (`pid_ext`). Ambos son buenos puntos de partida para implementar `IController`.
- **Drivers:** la estructura MCPWM de `bldc_pwm` y los drivers de BNO055 y AS5600. Una sola variante de cada uno detrás de las interfaces.
- **`Kconfig.projbuild`** (`wheel-movement`) para los parámetros de compilación.
- **Abanico de salida y entrada con notificaciones** (`Velocity-Control`): la idea sirve, pero hay que añadir timeouts ([`tasks.c:155-157`](../../firmware/apps/striker_v2/Velocity-Control/main/tasks.c#L155-L157) espera sin límite).

---

## 7. Trazabilidad: hallazgos → decisiones de diseño de v3

| Eje | Hallazgos | Decisión para v3 | Se detalla en |
|---|---|---|---|
| 1 | C1 | WiFi, LwIP y `esp_timer` en el Core 0; Core 1 exclusivo para el lazo; afinidad explícita en todas las tareas | Sección 3 |
| 1 | C2, C3, C5, C8 | Buzones para consignas y estado; stream buffer para telemetría; event groups para modos y arranque; una sola tarea de lazo | Sección 3 |
| 1 | C6, C7 | Propietario único de cada bus o periférico (mutex donde se comparta); ISR de `gptimer` → notificación directa | Sección 3 |
| 3 | T1, T2, T3 | Activación por `gptimer` en el Core 1; detección de desbordes; Δt medido; tick único de 1 kHz | Sección 3 |
| 3 | T4, T5 | Periodos (Ti) elegidos según la tasa real del actuador y del sensor; evaluar la sincronización con el evento TEZ del MCPWM | Sección 3 |
| 3 | T6, T7 | Instrumentación (ciclos, GPIO, estadísticas de ejecución, marca de agua) con `-O2`; una sola versión de IDF | Sección 3 |
| 2 | A1–A7 | Componentes ESP-IDF reales; 4 capas; interfaces C++ (`ISensorIMU`, `IWheelEncoder`, `IMotor`, `IController`); máquina de estados con Calibración y Autotuning; Kconfig + NVS | Sección 2 |
| 4 | M1–M5 | IRAM para ISR y lazo rápido; DRAM para estado y covarianzas; PSRAM octal para registros; 16 MB de flash con particiones propias; mapa de pines sin *strapping*, USB ni bus de memoria; ULP para la batería | Sección 4 |
| 5 | N1–N5 | Solo `float`, verificado por el compilador; benchmark `float` vs Q15/Q16.16 vs esp-dsp; PID con anti-windup; resolución del actuador | Sección 5 |
| 6 | F1–F6 | Protocolo especificado con validación, confirmación, latido y timeout; rutas en lazo cerrado con pose estimada; telemetría | Sección 5 |

---

## 8. Pendientes

1. **Siguiente paso del diagnóstico:** `bench_pid_autotuning` (sí compila), la suite de estimación en C (unas 17 000 líneas propias más CLAPACK, en un repositorio aparte) y los servidores de autotuning de la estación base.
2. **Datos de hardware por confirmar.** La placa ya está confirmada (YD-ESP32-S3 N16R8, sección 3.3). Faltan:
   - Modelo de ESC y frecuencias o protocolos que acepta.
   - Robot que se usará en la Entrega 2, y su cableado actual.
   - Estado del puente «RGB» de la placa (GPIO48).
3. **Opcional:** una rama de referencia con arreglos mínimos de compilación para medir la línea base del firmware viejo.

---

## Anexo A — Reproducibilidad

Las herramientas están en [`herramientas/`](herramientas/):

```bash
# 1. Compilar todas las apps de referencia sobre una copia aislada (requiere ESP-IDF instalado)
docs/striker_v3/herramientas/compilar_referencias.sh <ruta_al_repo> /tmp/build_referencias
cat /tmp/build_referencias/resumen.txt

# 2. Listar operaciones double por función en los objetos de una app ya compilada
OBJDUMP=xtensa-esp32s3-elf-objdump
python3 docs/striker_v3/herramientas/buscar_double.py "$OBJDUMP" \
  $(find /tmp/build_referencias/firmware/apps/striker_v2/Robot-Control/build/esp-idf/main -name '*.obj')
```

Entorno usado: ESP-IDF v5.5.1, GCC `xtensa-esp-elf` 14.2.0 (esp-14.2.0_20241119).
