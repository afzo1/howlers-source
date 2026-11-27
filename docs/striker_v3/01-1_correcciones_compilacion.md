# Entrega 1 · Sección 1 (anexo) — Correcciones mínimas para compilar las apps de referencia

**Proyecto:** firmware unificado Striker v3 (ESP32-S3) · Howlers FC / Robosyn · Universidad de Antioquia
**Rama:** `entrega-1` de `afzo1/howlers-source` · **Punto de partida:** revisión `0451897`, la misma del [diagnóstico](01_diagnostico_striker1_striker2.md)
**Fecha:** 11 de noviembre de 2026

---

## 0. Por qué se hicieron estas correcciones

El diagnóstico (sección 2) encontró que, en la revisión analizada, ninguna de las cinco apps de Striker compilaba. Al consultarlo, el profesor explicó que el repositorio base se había reorganizado hacía poco, que eso pudo introducir errores, y nos indicó trabajar con libertad y hacer las modificaciones pertinentes.

Antes de corregir se comparó cada app con el repositorio del que vino ([`nparra-code/striker-1`](https://github.com/nparra-code/striker-1), commit `dc56d28`, y [`MaverickST/RoboCup_Striker2`](https://github.com/MaverickST/RoboCup_Striker2), commit `e182de0`). El resultado es mixto: unos errores sí aparecieron con la reorganización y otros ya estaban en los repositorios originales (tabla de la sección 2). Se corrigieron todos, porque el objetivo es el mismo en ambos casos.

Tener las apps viejas compilando sirve para tres cosas:
1. **Medir la línea base.** La Entrega 2 debe mostrar que v3 mejora jitter, WCET y uso de memoria frente al firmware viejo, y eso solo se puede medir sobre un binario que exista.
2. **Usar el código viejo como referencia ejecutable**, no solo como texto.
3. **Poder probar el robot con seguridad.** Para las primeras pruebas no se usa ninguna de estas apps sino un firmware de pruebas aparte; ver la [guía de pruebas del robot](guia_pruebas_robot.md).

---

## 1. Criterios de las correcciones

En orden de prioridad:

1. **Cambio mínimo.** Solo lo necesario para compilar. No se rediseñó ni se "mejoró" nada.
2. **Preferir el código original de cada app.** Cuando una app quedó apuntando a los drivers de otra, se restauró la copia exacta de sus propios drivers desde su repositorio original, en vez de adaptar la app a los drivers compartidos. Así la app vuelve a ser la que sus autores probaron. Las copias quedan en una carpeta `local_drivers/` dentro de cada app, con un `ORIGEN.md` que dice de dónde salió cada archivo, y se verificaron byte a byte con `cmp`.
3. **No mezclar versiones de un mismo driver.** Si un archivo de un driver difería del original, se restauró la carpeta completa de ese driver. La única excepción es el TM151 de `wheel-movement`, explicada en W3.
4. **Que nada falle ni se mueva al encender.** Además de compilar, se corrigieron dos casos: un programa que se reiniciaba en bucle al arrancar (R2) y otro que movía el robot sin que nadie lo ordenara (C2).
5. **No corregir el comportamiento.** Errores como F1 (Striker 1 usa la distancia como velocidad) o F2 (el temporizador de trayectoria no se reinicia) se dejaron como estaban. Son la línea base que v3 debe superar, y corregirlos aquí borraría la evidencia del diagnóstico.
6. **Trazabilidad.** Cada cambio en el código lleva un comentario que empieza con `[Correccion 2026-11]` o `[Correccion de seguridad 2026-11]` y explica qué había antes y por qué cambió. Para encontrarlos todos:
   ```bash
   grep -rn "Correccion.*2026-11" firmware/
   ```
7. **Fuera de las apps solo se tocó un componente compartido:** la HAL de Striker 1 (`components/platform_hal/v1_striker1`), que solo usa `robot-mov`. `bench_pid_autotuning`, que ya compilaba, no se tocó.

---

## 2. Resumen

| App | Error en `0451897` | Origen | Corrección | Resultado |
|---|---|---|---|---|
| `striker_v1/robot-mov` | No enlazaba: `get_ip_address` definida dos veces. Además, con el código tal como estaba, se reiniciaría en bucle al arrancar | Repositorio original (comprobado compilando el original) | R1 y R2 | ✅ Compila |
| `striker_v1/wheel-movement` | Marcadores de conflicto de git, prototipo sin `;`, drivers de otra app y modo 2 que nunca se compilaba | Mixto | W1 a W4 | ✅ Compila en sus tres modos |
| `striker_v2/Robot-Control` | Marcadores de conflicto de git. Además, arrancaba moviéndose | Repositorio original | C1 y C2 | ✅ Compila |
| `striker_v2/Robot-Navigation` | Usaba una fusión sensorial que no existe en el componente compartido | Reorganización | N1 | ✅ Compila |
| `striker_v2/Velocity-Control` | No configuraba: carpeta `led_status` inexistente; detrás, drivers de otra app | Reorganización | V1 | ✅ Compila |
| `bench_pid_autotuning` | — | — | Ninguna | ✅ Compila |

Todas se compilaron con ESP-IDF v5.5.1. Algunas siguen mostrando avisos (*warnings*) del código original; no se corrigieron porque no impiden compilar (criterio 1).

---

## 3. Detalle de cada corrección

### 3.1 `striker_v1/robot-mov` (Striker 1, firmware del robot completo)

**R1 · La función `get_ip_address` estaba definida dos veces.**
- *Error.* Al enlazar: `multiple definition of 'get_ip_address'`. Una copia está en la HAL ([`platform_esp32s3.c:514`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/components/platform_hal/v1_striker1/platform_esp32s3.c#L514)) y otra en la capa de comunicaciones ([`wifi_lib.c:38`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/components/comms/wifi_udp/v1_striker1/wifi_lib.c#L38)), y `robot-mov` compila los dos archivos.
- *Origen.* Repositorio original. Se compiló el `robot-mov` de `striker-1@dc56d28` tal cual, con ESP-IDF v5.5.1, y falla con el mismo mensaje.
- *Cambio.* Se borró la copia de la HAL (`components/platform_hal/v1_striker1/platform_esp32s3.c`) y su prototipo (`include/platform_esp32s3.h`). Queda la de `wifi_lib.c`.
- *Por qué esa.* Las dos hacen lo mismo (buscan la interfaz `WIFI_STA_DEF` e imprimen la IP); solo cambia el texto del mensaje. La de `wifi_lib.c` pertenece a la capa de comunicaciones, que es donde debe estar (hallazgo A4), y acompaña a la inicialización del WiFi que sí se usa (R2). Esa HAL solo la usa `robot-mov`, así que ninguna otra app se ve afectada.
- *Alternativas descartadas.* Declarar una de las dos `static` o `weak`: compila, pero deja el código duplicado y esconde el problema. Renombrar una: obliga a tocar más archivos sin ganar nada.

**R2 · El WiFi se inicializaba dos veces (el programa no habría arrancado).**
- *Problema.* `app_main` llamaba primero a `wifi_init_station()` de la HAL ([`main.c:62`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/main.c#L62)) y después a `dev_wifi_init()` de `wifi_lib` ([`main.c:156`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/main.c#L156)). Las dos crean la interfaz WiFi por defecto. En la segunda, ESP-IDF encuentra la interfaz ya creada, `esp_netif_new()` devuelve `NULL` y un `assert` reinicia el chip. Como al reiniciar pasa lo mismo, el robot quedaría en un bucle de reinicios (ESP-IDF v5.5.1: `components/esp_netif/lwip/esp_netif_lwip.c`, líneas 771-775, y `components/esp_wifi/src/wifi_default.c`, líneas 418-422; las aserciones están activas en el `sdkconfig` de la app). No impide compilar, pero sí usar el programa.
- *Origen.* Repositorio original: el `main.c` de `striker-1` tiene las mismas dos llamadas.
- *Cambio.* Se quitó el primer bloque (`wifi_init_station()` y su `get_ip_address()`) de `firmware/apps/striker_v1/robot-mov/main/main.c`. El WiFi queda inicializado una sola vez, con `dev_wifi_init()`.
- *Por qué ese bloque.* `dev_wifi_init()` usa las credenciales que el equipo usaba de verdad (`wifi_lib.h`), mientras que la HAL tiene un nombre de red de relleno («er nombre er güifi»). El servidor UDP necesita la red real.
- *Alternativa descartada.* Hacer que las funciones de la HAL toleren una segunda llamada: es más código y mantiene la duplicación.

### 3.2 `striker_v1/wheel-movement` (Striker 1, banco de una rueda)

Este archivo tenía una fusión de ramas de git sin terminar entre dos versiones del programa:
- **Rama `main`:** una consola por UART (`vTaskUART`). Es la que usa `app_main`, que está fuera de los conflictos: crea la tarea `vTaskUART` y declara su prototipo.
- **Rama `HEAD`:** una versión por WiFi/TCP sin terminar. Nadie la llama (no hay ningún `xTaskCreate` para `tcp_server`) y tiene errores de sintaxis propios (`wifi_config_config_t`, `struct main sockaddr_in`, `len <= o`).

Por eso, en los tres conflictos se conservó la rama `main`: es la única versión completa y la que el resto del archivo usa. Copia del archivo antes de los cambios: [`main.c` en `0451897`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/wheel-movement/main/main.c).

**W1 · Primer conflicto: constantes del PID** ([`main.c:114-127`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/wheel-movement/main/main.c#L114-L127)).
- *Cambio.* Quedan los valores de `main`: `PID_KP 0.01`, `PID_KI 0.01`, `PID_KD 0.001`.
- *Dato útil.* La rama `HEAD` tenía `Kp = 1.66`, `Ki = 0.6`, `Kd = 0.05`. Si se quieren probar en el banco no hace falta recompilar: la consola acepta `P 1.66 0.6 0.05 <consigna>`.

**W2 · Segundo y tercer conflicto: la tarea de consola** ([`main.c:464-565`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/wheel-movement/main/main.c#L464-L565) y [`:654-683`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/wheel-movement/main/main.c#L654-L683)).
- *Problema.* Al fusionar, git mezcló el cuerpo de `vTaskUART` con el del servidor TCP: el comienzo de `vTaskUART` (la configuración de la UART) quedó seguido del código del WiFi. El cuerpo original de `vTaskUART` no está entero en ninguna de las dos ramas.
- *Cambio (reconstrucción).* `vTaskUART` se rearmó con piezas que están en el propio archivo: la configuración de la UART de la rama `main`; los comandos `P`, `X` y `S` de la tarea `uart_task` que la rama `HEAD` dejó comentada (líneas 465-529); y los comandos `D` e `I` en su versión de la rama `main`, que además reinician los datos del LiDAR y la bandera `reached_goal`. Se eliminó el código WiFi/TCP sin terminar (`wifi_init_sta`, `get_ip_address`, `tcp_server`) y los prototipos de las dos primeras; al de `get_ip_address` le faltaba el `;` ([`main.c:251`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/wheel-movement/main/main.c#L251)).
- *Advertencia.* Es la única corrección que no es una elección directa entre dos versiones existentes. Su comentario en el código lo indica.

**W3 · La app apuntaba a los drivers de otra app.**
- *Problema.* Al reorganizar el monorepo, `wheel-movement` quedó compilando con las versiones de `robot-mov` (`components/*/v1_striker1`), que tienen otra API. Por ejemplo, `estimate_velocity_encoder` recibe 1 argumento en la versión compartida y 3 en la original, y la estructura `encoder_data_t` original tiene un campo `estimate` que la compartida no tiene (hallazgo A3).
- *Origen.* Reorganización: en `striker-1`, `wheel-movement` traía sus propios drivers.
- *Cambio.* Se restauraron, idénticos al original, la plataforma, `pid_lib`, `sensor_fusion` y el AS5600 en `firmware/apps/striker_v1/wheel-movement/local_drivers/`, y se reescribió `main/CMakeLists.txt` para usarlos (con las carpetas locales primero en `INCLUDE_DIRS`, para que sus cabeceras tengan prioridad). El AS5600 se restauró completo aunque solo su cabecera difería (criterio 3).
- *Lo que se sigue tomando de `components/`.* BLDC y VL53L1X, que son idénticos a los originales, y el TM151. Este último es la excepción al criterio 3: la versión compartida (la de `robot-mov`) solo agrega dos funciones para leer el ángulo de guiñada y cambia comentarios. Las funciones que usa `wheel-movement` son idénticas, así que se dejó la compartida.

**W4 · El modo 2 (prueba del AS5600) nunca se compilaba.**
- *Problema.* El `Kconfig` de la app ofrece `MAIN_MODE` = 0, 1 o 2 (2 = prueba del AS5600), pero el bloque de ese modo estaba bajo `#if MAIN_MODE == 3`. Al elegir el modo 2 no había `app_main`. Al corregir la condición apareció un segundo error que estaba escondido: ese bloque redefinía `TAG`, ya definido al inicio del archivo.
- *Origen.* Repositorio original.
- *Cambio.* La condición pasó a `MAIN_MODE == 2` y la variable del bloque se renombró `TAG_PROBE`. Su valor (`"AS5600"`) no cambió, así que los mensajes salen igual.

**Modos compilados.** Se compilaron los tres modos (0, 1 y 2) cambiando `CONFIG_MAIN_MODE` en una copia del `sdkconfig`. El repositorio queda en el modo 0, el que ya traía.

### 3.3 `striker_v2/Robot-Control` (Striker 2, control de velocidad de las 3 ruedas)

**C1 · Conflicto de git en `app_main`** ([`main.c:37-48`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v2/Robot-Control/main/main.c#L37-L48)).
- *Origen.* Repositorio original: el mismo conflicto está en `Robot-Control/main/main.c` de `e182de0`.
- *Las dos versiones.* `HEAD` empezaba creando las tareas (`create_tasks()`) antes de inicializar los drivers y los objetos del kernel, y volvía a crearlas al final de `app_main`. La versión del commit `8dc4390` empieza con una espera de 5 s y crea las tareas una sola vez, al final.
- *Cambio.* Se conservó la versión de `8dc4390`.
- *Por qué.* Con `HEAD`, las tareas usarían semáforos y colas todavía sin crear y luego existirían duplicadas (hallazgo C8). La versión de `8dc4390` es la misma que usa la app hermana `Robot-Navigation`.

**C2 · Corrección de seguridad: el robot se movía al encender.**
- *Problema.* La tarea de control ejecutaba, apenas arrancaba, una trayectoria fija de 50 cm a 30 cm/s sin orden de nadie ([`tasks.c:197`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v2/Robot-Control/main/tasks.c#L197), hallazgo F4).
- *Cambio.* Esa línea quedó comentada en `firmware/apps/striker_v2/Robot-Control/main/tasks.c`. El robot arranca quieto y solo se mueve con un comando UDP.
- *Alternativa descartada.* Dejar la trayectoria pero más corta o más lenta: cualquier movimiento que nadie pidió es un riesgo, sobre todo al conectar la batería con el robot en la mano.

### 3.4 `striker_v2/Robot-Navigation` (Striker 2, control en cascada con fusión IMU)

**N1 · Usaba una fusión sensorial que no existe en el componente compartido.**
- *Problema.* `tasks.c` usa `senfusion_config_t`, `senfusion_update1/2`, `senfusion_get_state` y `calc_fordwkinematics` ([`tasks.c:144-205`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v2/Robot-Navigation/main/tasks.c#L144-L205)). El componente compartido `control/pid_lib/v2_striker2` trae la fusión de `Robot-Control`, con otra API.
- *Origen.* Reorganización: en `e182de0`, `Robot-Navigation` traía su propia carpeta `components/control`.
- *Cambio.* Se restauraron, idénticas al original, su carpeta `control` (`control.c/.h` y `senfusion.c/.h`) y su driver del BNO055, en `firmware/apps/striker_v2/Robot-Navigation/local_drivers/`, y se reescribió `main/CMakeLists.txt`.
- *Cuánto difieren.* Frente a la versión compartida, `diff` marca 211 líneas distintas en `senfusion.c`, 158 en `senfusion.h` y 2 en `bno055.h`. `control.c` y `control.h` son idénticos, pero se restauraron con su carpeta (criterio 3). La plataforma, el AS5600, el BLDC, el VL53L1X y la consola UART compartidos son idénticos a los originales, así que se siguen tomando de `components/`.
- *Alternativas descartadas.* Agregar la API de `Robot-Navigation` al componente compartido: comparte nombres de funciones con la de `Robot-Control` pero no su comportamiento, y rompería esa app. Reescribir `tasks.c` para la API compartida: sería reescribir la app y cambiar su comportamiento.

### 3.5 `striker_v2/Velocity-Control` (Striker 2, banco de un motor)

**V1 · Le faltaban sus drivers.**
- *Problema.* `main/CMakeLists.txt` declaraba la carpeta `components/actuators/led_status/v2_striker2`, que no existe ([`main/CMakeLists.txt:31`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v2/Velocity-Control/main/CMakeLists.txt#L31)), así que la app ni siquiera configuraba. Detrás de ese error había más: la app quedó apuntando a las versiones de `Robot-Control` de casi todos sus drivers.
- *Origen.* Reorganización: en `e182de0`, `Velocity-Control` traía sus propios drivers en `components/`.
- *Cómo se comprobó que no bastaba con arreglar la carpeta.* Se probó paso a paso sobre una copia de `0451897`:
  1. Quitando la carpeta inexistente, falta `led.h`: el driver del LED de Striker 2 no está en ningún lugar del monorepo.
  2. Agregando el LED original, falta `control_senfusion.h`, la biblioteca de control propia de esta app.
  3. Agregando también esa biblioteca, la HAL compartida no compila: espera las constantes `UDP_PORT`, `UDP_RX_BUF_SIZE` y `TAG_UDP_SERVER`, que define el `types.h` de `Robot-Control`.

  Además, al comparar archivo por archivo, 8 de los 11 archivos de drivers compartidos difieren de los originales de la app (plataforma, AS5600, BLDC y las cabeceras del BNO055 y del VL53L1X).
- *Cambio.* Se restauraron todos sus drivers, idénticos al original (plataforma, `control_senfusion`, AS5600, BLDC, BNO055, LED y VL53L1X), en `firmware/apps/striker_v2/Velocity-Control/local_drivers/`, y se reescribió `main/CMakeLists.txt` sin la carpeta inexistente. Solo la consola UART, idéntica al original, se sigue tomando de `components/`.

> ⚠️ **`Velocity-Control` mueve el motor sola, por diseño.** Cuando sus sensores quedan listos, ejecuta un experimento de identificación: una secuencia de escalones de potencia hacia adelante y en reversa, de hasta 20 %. No se modificó porque es su propósito como banco, pero **no debe cargarse en el robot armado ni con las ruedas en el piso**.

---

## 4. Lo que estas correcciones no resuelven

Que una app compile no la hace segura de usar en el robot. Para las primeras pruebas se usa el firmware de pruebas [`striker_hw_test`](../../firmware/apps/striker_hw_test/README.md), no estas apps. Si más adelante se carga alguna de ellas, hay que tener en cuenta:

| App | Qué puede pasar al cargarla en el robot | Hallazgo |
|---|---|---|
| `robot-mov` | Con una orden `L`, recorre una distancia distinta de la pedida; la duración de cada orden depende de cuánto tiempo estuvo quieto el robot; no se detiene si se cae el WiFi | F1, F2, F4 |
| `wheel-movement` | Es un banco de una rueda. Su modo 1 calibra el ESC: manda acelerador **máximo** durante 7 s y después sube la potencia paso a paso hasta el máximo, una y otra vez, alternando el sentido | — |
| `Robot-Control` | Si se pierde el WiFi, la última orden sigue hasta vencer su tiempo; un error de red deja la tarea de red muerta | F4 |
| `Robot-Navigation` | Recibe las órdenes UDP y las descarta (el robot no obedece); usa un tick de 100 Hz | F3, T3 |
| `Velocity-Control` | Mueve el motor sola (recuadro anterior) | — |
| Todas | Usan pines de *strapping* y, en Striker 1, una línea del USB nativo para los motores | M5 |

Los demás hallazgos del diagnóstico (concurrencia, tiempo real, memoria, precisión numérica) siguen igual: v3 los resuelve por diseño.

---

## 5. Archivos modificados

| Archivo | Cambio |
|---|---|
| `firmware/apps/striker_v1/robot-mov/main/main.c` | R2 |
| `firmware/components/platform_hal/v1_striker1/platform_esp32s3.c` | R1 |
| `firmware/components/platform_hal/v1_striker1/include/platform_esp32s3.h` | R1 |
| `firmware/apps/striker_v1/wheel-movement/main/main.c` | W1, W2 y W4 |
| `firmware/apps/striker_v1/wheel-movement/main/CMakeLists.txt` | W3 |
| `firmware/apps/striker_v1/wheel-movement/local_drivers/` (nuevo) | W3: copia de `striker-1@dc56d28` |
| `firmware/apps/striker_v2/Robot-Control/main/main.c` | C1 |
| `firmware/apps/striker_v2/Robot-Control/main/tasks.c` | C2 |
| `firmware/apps/striker_v2/Robot-Navigation/main/CMakeLists.txt` | N1 |
| `firmware/apps/striker_v2/Robot-Navigation/local_drivers/` (nuevo) | N1: copia de `RoboCup_Striker2@e182de0` |
| `firmware/apps/striker_v2/Velocity-Control/main/CMakeLists.txt` | V1 |
| `firmware/apps/striker_v2/Velocity-Control/local_drivers/` (nuevo) | V1: copia de `RoboCup_Striker2@e182de0` |

Para ver todos los cambios frente a la revisión analizada:

```bash
git diff 0451897 -- firmware/apps/striker_v1 firmware/apps/striker_v2 firmware/components
```

---

## 6. Cómo reproducir

Requisito: ESP-IDF v5.5.1 instalado (en Linux, [`herramientas/instalar_esp_idf.sh`](herramientas/instalar_esp_idf.sh) lo instala).

```bash
# Desde la raíz del repositorio y con ESP-IDF activo (por ejemplo: . ~/esp/esp-idf/export.sh)
# Compilar todas las apps sobre una copia (no ensucia el repositorio)
docs/striker_v3/herramientas/compilar_referencias.sh . /tmp/build_referencias
cat /tmp/build_referencias/resumen.txt    # todas con configure=0 y build=0

# Compilar los otros modos de wheel-movement, sobre la misma copia
cd /tmp/build_referencias/firmware/apps/striker_v1/wheel-movement
for m in 1 2; do
  sed -i "s/^CONFIG_MAIN_MODE=.*/CONFIG_MAIN_MODE=$m/" sdkconfig
  idf.py -B build build > /dev/null && echo "modo $m: compila"
done

# Ver los fallos originales: compilar la revisión analizada
git worktree add /tmp/howlers_0451897 0451897
docs/striker_v3/herramientas/compilar_referencias.sh /tmp/howlers_0451897 /tmp/build_0451897
```
