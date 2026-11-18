# Entrega 1 · Sección 4 — Mapa de memoria del ESP32-S3 y diseño del ULP

**Proyecto:** firmware unificado Striker v3 (ESP32-S3) · Howlers FC / Robosyn · Universidad de Antioquia
**Rama:** `entrega-1` de `afzo1/howlers-source` · **Fecha:** 18 de noviembre de 2026
**Herramientas:** [`sdkconfig_v3.defaults`](herramientas/sdkconfig_v3.defaults), [`particiones_v3.csv`](herramientas/particiones_v3.csv) y [`medir_memoria_base.sh`](herramientas/medir_memoria_base.sh)

---

## Resumen

1. **Cada dato y cada función tiene un lugar decidido** (hallazgo M1): el lazo rápido y sus interrupciones en **IRAM**; pilas, estado de control, matrices del estimador y *buffers* del Core 1 en **DRAM interna**; historiales, telemetría y *buffers* de red en **PSRAM**; la memoria compartida con el coprocesador en **memoria RTC**.
2. **Escribir la flash no detiene el robot.** Con `CONFIG_SPIRAM_XIP_FROM_PSRAM`, el código y las constantes se copian a la PSRAM al arrancar y ESP-IDF ya no desactiva la caché ni detiene al otro núcleo cuando se escribe en NVS (verificado en el código de ESP-IDF, 4.3). Igual se mantiene el camino crítico en IRAM y las escrituras solo en Idle, como defensa en profundidad.
3. **La memoria alcanza con holgura, y está medida.** La base de software (WiFi, LwIP, PSRAM octal, ULP) compilada con la configuración de v3 ocupa **101,5 KB de los 341,8 KB** de SRAM interna disponibles para datos y código estáticos; v3 agrega unos 85 KB propios (4.4).
4. La flash de 16 MB queda con una **tabla de particiones propia**: aplicación de 3 MB, NVS de ESP-IDF, NVS de parámetros del robot y una partición para volcados de memoria ante fallas (4.6).
5. El **ULP-RISC-V es el único dueño del ADC1**: muestrea los tres encoders cada 250 µs y la batería cada 10 ms, publica los datos sin bloqueos (*seqlock*), supervisa la batería LiPo con umbrales e histéresis y vigila que el lazo de ruedas siga vivo (4.8).

---

## 4.1 Memorias del ESP32-S3 N16R8

| Memoria | Tamaño | Cómo se accede | Uso en v3 |
|---|---|---|---|
| **SRAM interna** (IRAM y DRAM) | 512 KB. Con la configuración de v3, `idf.py size` deja **341 760 B** (DIRAM) y 16 KB (IRAM fija) para código y datos estáticos. Del resto, 48 KB son las cachés (16 KB de instrucciones y 32 KB de datos por defecto, [`Kconfig.cache:3-5`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_system/port/soc/esp32s3/Kconfig.cache#L3-L5) y [`:66-68`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_system/port/soc/esp32s3/Kconfig.cache#L66-L68)), y los ~114 KB restantes no se asignan a código ni datos estáticos: los usa el arranque y luego quedan para el *heap* ([`memory.ld.in:17-26`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_system/ld/esp32s3/memory.ld.in#L17-L26)) | Directo, sin caché: tiempo de acceso fijo | Código del lazo rápido e ISR (IRAM); pilas, objetos de FreeRTOS, estado de control, matrices del estimador, *buffers* del Core 1 y de DMA (DRAM) |
| **PSRAM octal** | 8 MB | A través de la caché de datos: rápida si el dato está en caché, más lenta y variable si no | Copia del código y las constantes (ejecución desde PSRAM), *buffers* de WiFi y LwIP, historiales de autotuning y telemetría |
| **Flash** | 16 MB | A través de la caché | Imagen de la aplicación, NVS y volcados de memoria |
| **Memoria RTC lenta** | 8 KB ([`soc.h:181-182`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/include/soc/soc.h#L181-L182)) | La leen y escriben la CPU principal y el ULP | 4 KB para el programa y los datos del ULP; el resto, libre |
| **Memoria RTC rápida** | 8 KB ([`soc.h:176-179`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/include/soc/soc.h#L176-L179)) | La CPU principal, un poco más lenta que la DRAM | ESP-IDF suma lo que queda libre al *heap* ([`memory-types.rst:187`](https://github.com/espressif/esp-idf/blob/v5.5.1/docs/en/api-guides/memory-types.rst#L187)); v3 no la usa directamente |

Las direcciones de cada región están en [`soc.h:167-196`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/include/soc/soc.h#L167-L196). La PSRAM y la flash comparten la misma ventana de direcciones de datos (0x3C000000–0x3E000000), y la MMU decide qué página de cada una se ve ahí.

---

## 4.2 Política de ubicación

| Qué | Dónde | Cómo se logra | Por qué |
|---|---|---|---|
| ISR del `gptimer` | IRAM | `CONFIG_GPTIMER_ISR_CACHE_SAFE` | Latencia mínima y fija: sin fallos de caché (sección 3.7.1) |
| `T_ctrl` y todo lo que llama: lectura del ULP, conversión a ángulo, estimador de velocidad, 3 PID, salida al MCPWM | IRAM | Fragmentos de enlazador (`linker.lf` con `* (noflash)`) para los objetos del lazo de ruedas en `striker_domain` y `pid_lib` y las envolturas de MCPWM y ULP en `striker_hal` (sección 2.3), y `CONFIG_MCPWM_CTRL_FUNC_IN_IRAM` para actualizar los comparadores | El WCET de `T_ctrl` no depende de la caché (hallazgo M1). La API de FreeRTOS ya está en IRAM por defecto |
| Pilas y bloques de control (TCB) de todas las tareas propias | DRAM | `xTaskCreateStaticPinnedToCore` con arreglos estáticos | ESP-IDF no pone pilas en PSRAM por defecto: con la caché desactivada serían inaccesibles ([`external-ram.rst:206`](https://github.com/espressif/esp-idf/blob/v5.5.1/docs/en/api-guides/external-ram.rst#L206)) |
| Colas, buzones, grupos de eventos | DRAM | Variantes estáticas de FreeRTOS | Ídem; además se crean antes de la barrera de arranque, sin `malloc` |
| `sb_autotune` (16 KB) y `mbuf_telemetry` (4 KB) | DRAM | Arreglos estáticos | Los escribe el Core 1 en cada ciclo: acceso de tiempo fijo |
| Estado del PID, pose, matrices de covarianza del EKF/UKF | DRAM | Miembros de objetos estáticos | Acceso en un ciclo; se usan cada 1 ms o 10 ms |
| *Buffers* de DMA de los drivers (I2C, UART) | DRAM | Los reservan los drivers con `MALLOC_CAP_DMA` | El DMA solo llega a la SRAM interna |
| Colchón del autotuning (10 s = 640 KB) | PSRAM | `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` una vez, en el arranque | `T_autotune` vacía cada 100 ms el *stream buffer* de DRAM en este colchón y lo envía por TCP a su ritmo: una pausa del WiFi de varios segundos no pierde muestras |
| Historial de telemetría (60 s) y registro de eventos | PSRAM | `EXT_RAM_BSS_ATTR` | Volumen grande, acceso poco frecuente y solo desde el Core 0 |
| *Buffers* de WiFi y LwIP | PSRAM | `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` | Libera SRAM interna |
| Resto del código y constantes | PSRAM (copia de la flash) | `CONFIG_SPIRAM_XIP_FROM_PSRAM` | Ver 4.3 |
| Configuración de cada robot (`RobotConfig` de S1 y S2) | Constantes en flash (copiadas a PSRAM) | `constexpr` | `T_ctrl` usa una copia en DRAM de los pocos datos que necesita |
| Memoria compartida con el ULP | RTC lenta | Variables del programa del ULP (símbolos `ulp_*`) | Es la única memoria que ven los dos procesadores (4.8.4) |
| Parámetros persistentes (ganancias, calibraciones, red) | Partición `nvs_striker` | `IParamStore`, solo desde `T_storage` | Sección 4.6 |

**Reglas que acompañan la tabla:**
- Nada de `malloc` ni `new` después de liberar la barrera de arranque. Los únicos `heap_caps_malloc` (colchones en PSRAM) se hacen en el arranque; si fallan, el robot no sale de Boot.
- Las tareas del Core 1 no tocan la PSRAM dentro de sus lazos: un fallo de caché hacia la PSRAM tarda más y compite con la lectura de código (que, con la ejecución desde PSRAM, también viene de ahí).
- Las funciones de `libm` (`sinf`, `cosf`) están en flash: `T_ctrl` no las usa. Las usa `T_nav`, que no tiene su código en IRAM y tiene un plazo de 10 ms.

---

## 4.3 Escrituras en la flash y caché

### 4.3.1 Qué pasa sin ejecución desde PSRAM

Escribir o borrar la flash (por ejemplo, guardar ganancias en NVS) necesita el bus de la flash, que es el mismo por donde la caché trae el código. Por eso, en la configuración por defecto, ESP-IDF:
- suspende el planificador en el núcleo que escribe y le pide al otro núcleo, por una llamada entre núcleos, que se quede girando en un lazo ([`cache_utils.c:89-113`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/spi_flash/cache_utils.c#L89-L113));
- desactiva en los dos núcleos las interrupciones que no estén en IRAM y luego las cachés ([`cache_utils.c:182-191`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/spi_flash/cache_utils.c#L182-L191)).

Mientras dura la escritura **ninguna tarea se ejecuta en ningún núcleo**, solo las ISR marcadas como seguras con la caché desactivada, y la PSRAM tampoco es accesible ([`external-ram.rst:206`](https://github.com/espressif/esp-idf/blob/v5.5.1/docs/en/api-guides/external-ram.rst#L206)). Un borrado de sector puede durar varios milisegundos o más, según el chip: el lazo de 1 kHz perdería activaciones.

### 4.3.2 Qué cambia con la ejecución desde PSRAM

Con `CONFIG_SPIRAM_XIP_FROM_PSRAM`, el código (`.text`) y las constantes (`.rodata`) de la flash se copian a la PSRAM al arrancar, y la MMU redirige esas direcciones a la PSRAM ([`external-ram.rst:167-176`](https://github.com/espressif/esp-idf/blob/v5.5.1/docs/en/api-guides/external-ram.rst#L167-L176)). Desde ese momento la CPU ya no necesita la flash para ejecutar, y el driver de la flash lo aprovecha: si las dos opciones están activas, en lugar de desactivar la caché y detener al otro núcleo solo toma un mutex ([`spi_flash_os_func_app.c:26`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/spi_flash/spi_flash_os_func_app.c#L26) y [`:119-120`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/spi_flash/spi_flash_os_func_app.c#L119-L120)). Resultado: **una escritura en NVS ya no detiene al Core 1**.

El costo es PSRAM: la copia ocupa lo mismo que la imagen (unos 0,63 MB en la medición de 4.4, unos 0,8 MB estimados con v3), menos del 10 % de los 8 MB. La placa tiene PSRAM octal a 80 MHz, más rápida que la flash quad a 80 MHz ([`external-ram.rst:174`](https://github.com/espressif/esp-idf/blob/v5.5.1/docs/en/api-guides/external-ram.rst#L174)), así que el código que corre desde caché no se vuelve más lento. ESP-IDF trae un ejemplo con esta función para un ESP32-S3 con PSRAM octal de 8 MB ([`sdkconfig.ci.esp32s3_f4r8`](https://github.com/espressif/esp-idf/blob/v5.5.1/examples/system/xip_from_psram/sdkconfig.ci.esp32s3_f4r8)).

### 4.3.3 Decisión

| Medida | Para qué |
|---|---|
| `CONFIG_SPIRAM_XIP_FROM_PSRAM` activo | Las escrituras en flash no detienen ningún núcleo |
| Camino crítico (`T_ctrl` e ISR) en IRAM de todos modos | Tiempo de ejecución sin fallos de caché, y que siga funcionando en el plan B |
| Escrituras en NVS solo desde `T_storage` y solo en Idle | Los parámetros cambian con el robot quieto; nunca hay una escritura durante un movimiento |
| WiFi con su configuración en RAM (`esp_wifi_set_storage(WIFI_STORAGE_RAM)`) | Que la pila de WiFi no escriba la flash por su cuenta cuando se reconecta |
| **Plan B**, si la función falla en la validación: desactivarla | Se vuelve al comportamiento de 4.3.1. Sigue siendo seguro porque las escrituras solo ocurren en Idle con los motores desarmados, y mientras dura la escritura `T_ctrl` suspende su vigilancia de activaciones perdidas (bit `STORAGE_BUSY`, sección 3.7.1) |

La alternativa de suspender automáticamente los borrados largos (`CONFIG_SPI_FLASH_AUTO_SUSPEND`, [`spi_flash/Kconfig:86-94`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/spi_flash/Kconfig#L86-L94)) se descarta: depende del modelo exacto del chip de flash del módulo, que no está documentado.

---

## 4.4 Presupuesto de memoria

### 4.4.1 Línea base medida

Antes de escribir v3 se midió la base de software que v3 va a tener sí o sí: el ejemplo `station` de WiFi de ESP-IDF 5.5.1, compilado con [`sdkconfig_v3.defaults`](herramientas/sdkconfig_v3.defaults) y con los componentes de PSRAM, ULP, `gptimer` y MCPWM. Se reproduce con [`medir_memoria_base.sh`](herramientas/medir_memoria_base.sh).

| Región (salida de `idf.py size`) | Usado | Disponible | Comentario |
|---|---:|---:|---|
| SRAM interna (DIRAM) | 101 531 B (29,7 %) | 341 760 B | 77 927 B de código en IRAM, 20 228 B de `.data`, 3 376 B de `.bss` |
| IRAM de la SRAM0 | 16 384 B (100 %) | 16 384 B | Lo normal: el código en IRAM llena primero estos 16 KB y sigue en la DIRAM |
| PSRAM (`.bss` externo) | 12 704 B | 8 MB | Variables de WiFi y LwIP movidas por `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY` |
| Código en flash | 534 556 B | 3 MB (partición) | Se copia a PSRAM al arrancar |
| Constantes en flash | 93 776 B | — | Ídem |
| RTC lenta para la aplicación | 32 B | 4 096 B | Los otros 4 096 B quedan reservados para el ULP |
| Programa del ULP (ejemplo de ADC de ESP-IDF) | 296 B | 4 096 B | Referencia del tamaño de un programa ULP que lee el ADC |

La misma compilación sin la ejecución desde PSRAM da prácticamente los mismos números (100 639 B de DIRAM): la función no gasta SRAM interna, solo PSRAM en tiempo de ejecución.

### 4.4.2 Lo que agrega v3

| Concepto | Memoria | Estimado | Base del estimado |
|---|---|---:|---|
| Pilas de las 9 tareas propias | DRAM | 39 KB | 4 KB cada una; 8 KB para `T_nav` (EKF) y 3 KB para `T_range`. Se ajustan con la marca de agua medida (sección 3.9) |
| Bloques de control de las tareas | DRAM | 3,5 KB | ~400 B cada uno |
| `sb_autotune` + `mbuf_telemetry` + colas y buzones | DRAM | 23 KB | 16 KB + 4 KB + ~3 KB (sección 3.4) |
| Estimador (EKF y UKF) | DRAM | 3,5 KB | Matrices de 6×6 y 4×6 en `float` |
| Instrumentación (histogramas y contadores) | DRAM | 4,5 KB | 4 histogramas de 256 casillas de 4 B |
| Estado de control, configuración, ruta | DRAM | 1 KB | — |
| **Total DRAM propio** | | **≈ 75 KB** | |
| Código del lazo rápido, ISR y funciones del MCPWM | IRAM | ≈ 9 KB | — |
| Colchón de autotuning, historial de telemetría y registro | PSRAM | ≈ 1,3 MB | 640 KB + 384 KB (60 s × 50 Hz × 128 B) + 256 KB |
| Copia del código y las constantes | PSRAM | ≈ 0,8 MB | 0,63 MB medidos + el código de v3 |

### 4.4.3 Balance y criterio de aceptación

- **SRAM interna estática:** 101,5 KB de base + ≈ 84 KB de v3 ≈ **186 KB de 341,8 KB** (54 %). Lo que queda, más los ~114 KB que libera el arranque, es el *heap* interno, del que ESP-IDF toma las pilas de sus tareas y los *buffers* que el WiFi necesita en memoria interna (se mide en la Entrega 2).
- **PSRAM:** ≈ 2,2 MB de 8 MB (27 %).
- **RTC lenta:** el programa del ULP de v3 (más grande que el ejemplo: cuatro canales, *seqlock*, batería y vigilancia) debe caber en los 4 096 B reservados; se espera que use menos de 2 KB.

**Criterio de la Entrega 2:** con el robot en Route y el WiFi transmitiendo, el mínimo histórico de *heap* interno libre (`heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)`) debe ser de al menos **32 KB**, el mismo valor que ESP-IDF reserva por defecto para memoria interna ([`Kconfig.spiram.common:109-112`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_psram/Kconfig.spiram.common#L109-L112)); y todas las pilas deben conservar al menos un 25 % libre (sección 3.9.3). El informe de la Entrega 2 incluirá `idf.py size-components`, como pide la guía.

---

## 4.5 Configuración de ESP-IDF

El archivo [`sdkconfig_v3.defaults`](herramientas/sdkconfig_v3.defaults) reúne todas las opciones que este informe justifica. En la Entrega 2 se copia como `sdkconfig.defaults` de `firmware/apps/striker_v3`.

| Grupo | Opciones | Justificación |
|---|---|---|
| Chip y flash | CPU a 240 MHz; flash de 16 MB en QIO a 80 MHz; tabla de particiones propia | Hallazgo M2: las apps viejas declaraban 2 MB |
| PSRAM | Modo octal a 80 MHz; ejecución desde PSRAM; WiFi/LwIP y `.bss` marcado a PSRAM | 4.2 y 4.3. Sin el modo octal la PSRAM de este módulo no se detecta |
| FreeRTOS | *Tick* de 1 kHz; tarea de *timers* en el Core 0; estadísticas de ejecución con contador de 64 bits | Secciones 3.2.2 y 3.9 |
| Afinidades | Tarea principal, `esp_timer`, LwIP y WiFi en el Core 0 | Sección 3.2.2 |
| Drivers | ISR del `gptimer` segura con caché desactivada; funciones de control del MCPWM en IRAM | 4.2 |
| ULP | ULP-RISC-V activo con 4 096 B reservados | 4.8. El máximo permitido es 8 176 B ([`ulp/Kconfig:29-35`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/Kconfig#L29-L35)) |
| Compilación | `-O2` (`CONFIG_COMPILER_OPTIMIZATION_PERF`), registros en nivel WARN | Hallazgo T6: medir el mismo binario que se entrega |
| Depuración | Volcado de memoria a la flash ante una falla | Analizar fallas del robot después, desde el PC |
| Energía | Sin gestión de energía | La CPU queda fija en 240 MHz y los ciclos se convierten a tiempo sin error |

---

## 4.6 Tabla de particiones

Archivo: [`particiones_v3.csv`](herramientas/particiones_v3.csv).

| Partición | Tipo | Desplazamiento | Tamaño | Contenido |
|---|---|---|---|---|
| `nvs` | data/nvs | 0x9000 | 24 KB | Datos de ESP-IDF: calibración de radio del WiFi |
| `phy_init` | data/phy | 0xF000 | 4 KB | Parámetros de radio |
| `factory` | app | 0x10000 | 3 MB | Aplicación (la medida de 4.4 ocupa 0,63 MB) |
| `nvs_striker` | data/nvs | 0x310000 | 64 KB | Parámetros del robot: variante, geometría, calibraciones, ganancias, red (`ParamKey` de [`storage.hpp`](../../firmware/components/striker_core/include/striker/storage.hpp)) |
| `coredump` | data/coredump | 0x320000 | 64 KB | Volcado de memoria de la última falla |
| (libre) | — | 0x330000 | 12,8 MB | Reservado: actualización por WiFi o registros de partidos, si se necesitan después |

Los parámetros del robot van en una partición NVS propia para que no compartan espacio ni ciclos de borrado con los datos de ESP-IDF, y para poder borrarlos o respaldarlos por separado (`parttool.py`). NVS reparte las escrituras entre páginas, así que 64 KB alcanzan para años de guardar ganancias.

---

## 4.7 Pines nuevos y pines de riesgo

### 4.7.1 Medición de la batería

- **Pin:** GPIO2, canal 1 del ADC1 ([`adc_channel.h:12`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/include/soc/adc_channel.h#L12)), libre en los dos robots (sección 2.6.2). Al ser del ADC1, lo puede leer el ULP.
- **Divisor de tensión:** la batería (la guía plantea una LiPo) supera los 3,3 V del pin, así que hace falta un divisor de dos resistencias, con la tierra común que ya comparten los ESC y la placa, y un condensador de 100 nF del pin a tierra para que el ADC tome la muestra sin error. **Falta un dato del robot: el tipo de batería de cada uno y cuántas celdas tiene.** Con la fórmula V<sub>pin</sub> = V<sub>bat</sub> · R2 / (R1 + R2), y buscando que la batería llena quede cerca de 2,6 V (dentro de la zona lineal del ADC, [`as5600_lib.h:10-12`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/components/sensors/as5600/v2_striker2/as5600_lib.h#L10-L12)):

| Batería | Tensión con carga completa | R1 / R2 | Tensión máxima en GPIO2 | Corriente del divisor |
|---|---:|---|---:|---:|
| LiPo 2S | 8,4 V | 22 kΩ / 10 kΩ | 2,63 V | 0,26 mA |
| LiPo 3S | 12,6 V | 39 kΩ / 10 kΩ | 2,57 V | 0,26 mA |
| LiPo 4S | 16,8 V | 56 kΩ / 10 kΩ | 2,55 V | 0,25 mA |

La relación del divisor queda en `RobotConfig::battery_divider_ratio`, así que el firmware no cambia con la batería.

### 4.7.2 Pines de medición

GPIO14, GPIO15 y GPIO16, libres en los dos robots, salen a un conector para el analizador lógico (sección 3.9). Los tres son además GPIO del dominio RTC ([`rtc_io_channel.h:52-58`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/include/soc/rtc_io_channel.h#L52-L58)): en la validación del ULP, GPIO16 lo maneja el ULP para medir la duración de sus rondas.

### 4.7.3 Pines de riesgo heredados (hallazgo M5)

Los motores de los dos robots están cableados a pines de *strapping* (GPIO0 y GPIO45 en S1; GPIO3 y GPIO46 en S2), y S1 usa GPIO20 (USB nativo) y GPIO48 (LED RGB si el puente está soldado). v3 **no cambia el cableado**: funciona con los robots como están, y reduce el riesgo así:

- Ningún pin de motor se configura como salida hasta que Boot termina y se sabe qué robot es; desde ese momento solo salen pulsos de parada (sección 2.7.2).
- La consola y la carga del firmware usan el USB-UART (GPIO43/44), no el USB nativo, así que perder el USB nativo en S1 no afecta.
- El riesgo que queda es eléctrico y está en el reinicio: si la entrada de un ESC fuerza un nivel en un pin de *strapping*, la placa puede arrancar en modo de descarga o con la flash mal alimentada. Si eso pasa alguna vez, la solución es de hardware: **recablear esos motores a pines libres es una decisión del equipo**, igual que la alternativa de fijar por eFuse el voltaje de la flash para que GPIO45 deje de importar (irreversible). Este informe no la toma.

---

## 4.8 Diseño del ULP-RISC-V

### 4.8.1 Qué hace y por qué en el ULP

| Función | Periodo | Qué descarga de las CPU principales |
|---|---|---|
| Muestrear los 3 encoders (ADC1) | 250 µs (4 kHz) | La lectura del ADC, que en el driver de ESP-IDF entra en una sección crítica ([`adc_oneshot.c:205-210`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_adc/adc_oneshot.c#L205-L210)), y el muestreo a ritmo uniforme |
| Medir y supervisar la batería LiPo | 10 ms | La supervisión continua, con avisos solo cuando cambia el estado |
| Vigilar el latido de `T_ctrl` | Cada ronda, con los motores armados | Un vigilante independiente de los dos núcleos y de FreeRTOS |

Se usa el **ULP-RISC-V** y no el ULP-FSM porque se programa en C, tiene una función de ESP-IDF para leer el ADC desde el coprocesador ([`ulp_riscv_adc.c:9-30`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/ulp_core/ulp_riscv_adc.c#L9-L30)) y corre a unos 17,5 MHz ([`ulp_riscv_utils.h:86-88`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/ulp_core/include/ulp_riscv_utils.h#L86-L88)). No tiene FPU: todo su código usa enteros.

### 4.8.2 Por qué el ULP es dueño de todo el ADC1

- Los encoders de los dos robots están en el ADC1 (sección 2.6.2), y la batería también tiene que estar en el ADC1 porque es la única unidad que el ULP puede usar: `ulp_adc_init` rechaza cualquier otra ([`ulp_adc.c:25`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_common/ulp_adc.c#L25)).
- El driver de ESP-IDF deja reclamar cada unidad del ADC **una sola vez**: el segundo intento falla con "adc1 is already in use" ([`adc_oneshot.c:97-98`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/esp_adc/adc_oneshot.c#L97-L98)), y la inicialización del ULP la reclama ([`ulp_adc.c:39`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_common/ulp_adc.c#L39)). Por eso el ULP y la CPU principal no pueden repartirse el ADC1: o lo tiene uno, o lo tiene el otro. Es el mismo choque que en los firmwares viejos terminaba en un error fatal (hallazgo C6).
- **Decisión:** el ULP lee los cuatro canales (3 encoders + batería) y la CPU principal no toca el ADC. Como `ulp_adc_init` configura un solo canal ([`ulp_adc.c:49`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_common/ulp_adc.c#L49)), v3 hace lo mismo que esa función con la API pública, para los cuatro canales: `adc_oneshot_new_unit` con `ulp_mode = ADC_ULP_MODE_RISCV`, `adc_oneshot_config_channel` para cada canal (atenuación de 12 dB) y el código de calibración del ADC, como en [`ulp_adc.c:55-56`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_common/ulp_adc.c#L55-L56).
- Beneficio extra: el muestreo queda **uniforme**, porque el ULP no tiene interrupciones ni otras tareas. Es la base del estimador de velocidad (hallazgo T5).

### 4.8.3 Programa del ULP

```c
// Corre en el ULP-RISC-V. Lazo sin fin: el ULP no se apaga mientras el robot está encendido.
#include "ulp_riscv_utils.h"
#include "ulp_riscv_adc_ulp_core.h"
#include "soc/rtc_cntl_reg.h"

int main(void)
{
    uint32_t inicio = ULP_RISCV_GET_CCOUNT();
    for (;;) {
        // 1. Encoders: tres conversiones del ADC1.
        uint16_t raw[3]; uint16_t invalidos = 0;
        for (int i = 0; i < 3; i++) {
            const int32_t r = ulp_riscv_adc_read_channel(ADC_UNIT_1, enc_canal[i]);
            if (r < 0) { invalidos |= 1u << i; raw[i] = 0; } else { raw[i] = (uint16_t)r; }
        }
        publicar_encoders(raw, invalidos);       // seqlock (4.8.4)

        // 2. Batería: cada 40 rondas (10 ms), filtro y umbrales enteros (4.8.5).
        if (++div_bateria == 40) { div_bateria = 0; supervisar_bateria(); }

        // 3. Vigilancia del latido de T_ctrl, solo con los motores armados (4.8.6).
        if (armado) { vigilar_latido(); }

        // 4. Esperar a completar la ronda: periodo fijo medido con el contador de ciclos.
        while ((uint32_t)(ULP_RISCV_GET_CCOUNT() - inicio) < ciclos_por_ronda) { }
        inicio += ciclos_por_ronda;
    }
}
```

- `ciclos_por_ronda` lo escribe la CPU principal antes de arrancar el ULP: 250 µs × 17,5 ciclos/µs ≈ 4375 ciclos. La espera usa una resta sin signo, que funciona aunque el contador dé la vuelta.
- Cuatro muestras por milisegundo dan a `T_ctrl` una muestra de como mucho 250 µs de antigüedad, y reducen el riesgo de perder vueltas al desenvolver el ángulo cuando la rueda gira rápido.
- El reloj del ULP es un oscilador RC, menos exacto que el cristal de las CPU. Por eso el **tiempo de una ronda se calibra**: al arrancar y luego cada 10 s, `T_ctrl` cuenta cuántas rondas pasan en un intervalo medido con su propio reloj. El estimador de velocidad usa el número de rondas transcurridas multiplicado por ese tiempo calibrado como Δt.
- Si las tres conversiones no caben en 250 µs, la ronda se alarga hasta 500 µs; el requisito es tener una muestra nueva en cada ciclo de 1 ms.

### 4.8.4 Memoria compartida y *seqlock*

```c
// En la memoria RTC lenta. Escribe el ULP; leen T_ctrl (Core 1) y T_supervisor (Core 0).
volatile uint32_t secuencia;          // impar mientras el ULP escribe
volatile uint32_t ronda;              // contador de rondas: base de tiempo de los encoders
volatile uint16_t enc_raw[3];         // ADC1, 12 bits
volatile uint16_t enc_invalidos;      // bit i = lectura fallida del encoder i
volatile uint16_t bateria_raw;        // promedio filtrado
volatile uint16_t bateria_estado;     // 0 normal, 1 baja, 2 crítica
volatile uint16_t alarma_latido;      // 1 = T_ctrl dejó de latir

// Escribe la CPU principal; lee el ULP.
volatile uint32_t latido;             // T_ctrl lo incrementa en cada ciclo
volatile uint32_t armado;             // 1 = motores armados: vigilancia activa
volatile uint32_t confirmacion;       // T_supervisor confirma que atendió una alarma

// Configuración: la escribe la CPU principal antes de arrancar el ULP.
uint8_t  enc_canal[3], bateria_canal;
uint16_t bateria_umbral_baja, bateria_umbral_critica, bateria_histeresis;
uint32_t ciclos_por_ronda;
```

**Cómo se publica sin bloquear a nadie (*seqlock*).** El ULP incrementa `secuencia` (queda impar), escribe los datos y la incrementa otra vez (queda par). El lector copia `secuencia`, copia los datos y vuelve a leer `secuencia`: si las dos lecturas son iguales y pares, la copia es coherente; si no, repite (como mucho 3 veces; si falla, usa la ronda anterior y lo cuenta). El escritor nunca espera y el lector espera, en el peor caso, lo que tarda el ULP en escribir unos pocos datos (~1 µs). El lector pone barreras de memoria entre las lecturas para que el compilador y la CPU no las reordenen.

ESP-IDF trae un candado para compartir datos con el ULP ([`ulp_riscv_lock.c:9-16`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/ulp_core/ulp_riscv_lock.c#L9-L16)), pero con él la CPU principal tendría que girar en un lazo mientras el ULP tiene el candado, y `T_ctrl` no puede esperar a otro procesador. Por eso se usa el *seqlock*.

`T_ctrl` también vigila al ULP: si `ronda` no avanza en 2 ms, detiene los motores y avisa `UlpLost` (sección 3.10).

### 4.8.5 Supervisión de la batería

- Cada 10 ms el ULP lee el canal de la batería y lo filtra con un promedio exponencial entero (`y += (x − y) >> 3`), que suaviza el ruido del ADC sin usar `float`.
- Los umbrales llegan ya convertidos a unidades del ADC: la CPU principal los calcula al arrancar con el divisor de `RobotConfig` y la curva de calibración del ADC. El ULP solo compara enteros.
- Tres estados: **normal → baja** (aviso: no se inician rutas ni autotuning) **→ crítica** (`BatteryCritical` → Fault). Un cambio exige 50 lecturas seguidas (0,5 s) más allá del umbral, para que las caídas de tensión al acelerar los motores no disparen la falla, y el regreso exige superar el umbral más una histéresis.
- Los umbrales por celda se configuran en `RobotConfig`; como punto de partida, 3,5 V (baja) y 3,3 V (crítica) por celda, valores habituales para LiPo que se confirman con la batería del equipo.
- Cuando cambia el estado, el ULP genera su interrupción de software ([`ulp_riscv_utils.h:133`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/ulp_core/include/ulp_riscv_utils.h#L133)). La CPU principal la recibe con `ulp_riscv_isr_register(..., ULP_RISCV_SW_INT)` ([`ulp_riscv.h:39`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/include/ulp_riscv.h#L39) y [`:59`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/include/ulp_riscv.h#L59)), registrada desde el Core 0, y la ISR notifica a `T_supervisor` (sección 3.4).

### 4.8.6 Vigilancia del lazo de ruedas

1. Con los motores armados, el ULP compara `latido` en cada ronda. Si no cambia en **60 ms** (240 rondas), pone `alarma_latido` y genera la interrupción: `T_supervisor`, en el Core 0, detiene los motores y lleva el robot a Fault.
2. Si en **100 ms** más no llega `confirmacion` (por ejemplo, porque los dos núcleos están bloqueados), el ULP **reinicia todo el chip** escribiendo el bit `RTC_CNTL_SW_SYS_RST` ([`rtc_cntl_reg.h:22-25`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/soc/esp32s3/register/soc/rtc_cntl_reg.h#L22-L25)). El ULP puede escribir esos registros: ESP-IDF lo hace desde el mismo coprocesador para despertar a la CPU ([`ulp_riscv_utils.c:23`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/ulp_core/ulp_riscv_utils.c#L23)).
3. Tras el reinicio los pines de los motores quedan sin pulsos y el robot arranca en Boot, sin mover motores. **Falta verificar** cómo reaccionan los ESC cuando la señal desaparece (lo habitual es que se detengan); se hará con las ruedas al aire, siguiendo la guía de pruebas del robot.

El vigilante del ULP complementa, no reemplaza, a los de ESP-IDF (de tareas y de interrupciones): es el único que sigue funcionando si los dos núcleos se bloquean con las interrupciones desactivadas.

### 4.8.7 Riesgos y plan B

| Riesgo | Cómo se detecta | Respuesta |
|---|---|---|
| Las conversiones del ADC desde el ULP son lentas | Duración de la ronda en GPIO16 | Rondas de hasta 500 µs |
| El oscilador del ULP deriva con la temperatura | Calibración cada 10 s | Ya contemplado en 4.8.3 |
| Los AS5600 están configurados con salida de 0 a 100 %: el extremo alto queda fuera de la zona lineal del ADC ([`as5600_lib.h:6-12`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/components/sensors/as5600/v2_striker2/as5600_lib.h#L6-L12)) | En Calibración: valores mínimo y máximo del ADC al girar despacio cada rueda | Corrección por tabla en la conversión a ángulo |
| Un error en el programa del ULP (por ejemplo, un acceso inválido) | Interrupción de *trap* del ULP ([`ulp_riscv.h:40`](https://github.com/espressif/esp-idf/blob/v5.5.1/components/ulp/ulp_riscv/include/ulp_riscv.h#L40)) y `ronda` detenida | Fault |
| El diseño con ULP no funciona en la validación | Pruebas de 4.8.8 | **Plan B:** el ADC1 en modo continuo con DMA en la CPU principal (encoders y batería), y el ULP solo como vigilante. Las interfaces `IWheelEncoders` e `IBatteryMonitor` no cambian |

### 4.8.8 Validación (Entrega 2)

| Prueba | Cómo | Criterio |
|---|---|---|
| Periodo de la ronda | El ULP cambia GPIO16 en cada ronda; analizador lógico | 250 µs (o el periodo elegido) ± 5 %, sin rondas perdidas |
| Coherencia de los datos | 30 min leyendo con el *seqlock* desde `T_ctrl` | Cero lecturas incoherentes; reintentos contados |
| Lectura de los encoders | Misma posición de rueda leída con `striker_hw_test` (CPU) y con v3 (ULP) | Diferencia dentro del ruido del ADC |
| Batería | En la placa sola (sin motores), con un potenciómetro en GPIO2 que simule la tensión del divisor | Cambios de estado en los umbrales, con histéresis y sin falsas alarmas |
| Vigilancia | Orden de prueba que detiene el latido de `T_ctrl`, con las ruedas al aire y los límites de potencia de `striker_hw_test` | Parada en ≤ 60 ms; reinicio a los 160 ms sin confirmación |
| Tamaño | `ulp_main.bin` del programa de v3 | ≤ 4 096 B |

---

## 4.9 Trazabilidad

| Hallazgo | Decisión | Dónde |
|---|---|---|
| M1 · Sin decisiones de ubicación de memoria | Política de ubicación; camino crítico en IRAM; ejecución desde PSRAM | 4.2, 4.3 |
| M2 · PSRAM sin configurar; flash declarada de 2 MB | PSRAM octal; flash de 16 MB; tabla de particiones propia | 4.5, 4.6 |
| M3 · Pilas dimensionadas a ojo | Presupuesto por tarea y medición de la marca de agua | 4.4, sección 3.9 |
| M4 · El ULP no tiene antecedentes | Programa ULP para encoders, batería y vigilancia | 4.8 |
| M5 · Pines de riesgo | Mitigaciones en software; recableado como decisión del equipo | 4.7.3 |
| C6 · ADC compartido con error fatal | El ULP es el único dueño del ADC1 | 4.8.2 |
| T5 · Derivar un ángulo analógico | Muestreo uniforme y base de tiempo calibrada | 4.8.3 |
| Nuevo · Escribir la flash detiene los dos núcleos | Ejecución desde PSRAM + escrituras solo en Idle | 4.3 |
