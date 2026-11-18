# Guía de pruebas del robot heredado

**Proyecto:** firmware unificado Striker v3 · Howlers FC / Robosyn · Universidad de Antioquia
**Para quién:** cualquier integrante del equipo, aunque nunca haya programado un ESP32.
**Fecha:** 11 de noviembre de 2026 · **Firmware de pruebas:** [`firmware/apps/striker_hw_test`](../../firmware/apps/striker_hw_test/README.md)

---

## 0. Respuesta corta: ¿se puede probar el robot ya?

**Sí, con un programa de pruebas hecho para eso, no con los firmwares viejos.**

| Se puede revisar ahora | No todavía |
|---|---|
| Que la placa enciende y tiene la memoria esperada | Cargar `robot-mov`, `Robot-Control`, `Robot-Navigation`, `Velocity-Control` o `wheel-movement` en el robot: tienen riesgos documentados (uno se mueve solo al encender, otros no se detienen si se cae el WiFi). Detalle en la [sección 4 de 01-1](01-1_correcciones_compilacion.md#4-lo-que-estas-correcciones-no-resuelven) |
| Si el robot es Striker 1 o Striker 2 | Poner el robot en el piso con los motores activos |
| Que cada encoder mide el giro de su rueda | Calibrar los ESC |
| Que la IMU y el sensor de distancia responden | Subir los límites de potencia o de tiempo del programa de pruebas |
| Que el WiFi funciona | |
| Que cada motor gira, en ambos sentidos, con las ruedas en el aire, a potencia baja (máximo 30 %) y por poco tiempo (máximo 5 s) | |

El programa de pruebas (`striker_hw_test`) no mueve nada al encender, solo mueve un motor cuando se le ordena con un comando escrito, y se detiene solo. Así se puede revisar el robot pieza por pieza sin sorpresas.

**Tiempo estimado la primera vez:** 2 a 3 horas, de las cuales casi una hora es instalar programas en el computador.

---

## 1. Mapa de la guía

| Fase | Qué se hace | ¿Batería conectada? |
|---|---|---|
| [Fase 0](#fase-0--preparar-el-computador-una-sola-vez) | Preparar el computador (una sola vez) | — |
| [Fase 1](#fase-1--inspeccionar-el-robot-sin-conectar-nada) | Inspeccionar el robot, sin conectar nada | No |
| [Fase 2](#fase-2--primera-conexión-por-usb) | Conectar por USB, ver qué programa trae, guardarlo y cargar el de pruebas | **No** |
| [Fase 3](#fase-3--pruebas-sin-motores) | Pruebas sin motores | No (salvo que los sensores la necesiten) |
| [Fase 4](#fase-4--pruebas-de-motores-ruedas-en-el-aire) | Pruebas de motores, con las ruedas en el aire | **Sí** |
| [Fase 5](#fase-5--terminar-y-registrar) | Terminar, registrar y compartir resultados | No |

Al final hay una [tabla de problemas frecuentes](#6-problemas-frecuentes), las instrucciones para [devolver el robot a su programa original](#7-volver-al-programa-original-del-robot) y una [plantilla para anotar resultados](#anexo-a--plantilla-de-registro).

---

## 2. Reglas de seguridad (leer antes de empezar)

### 2.1 Batería LiPo

Las baterías de litio-polímero (LiPo) guardan mucha energía. Bien tratadas son seguras; mal tratadas pueden incendiarse.

- **Revísala antes de cada sesión.** Si está hinchada, golpeada, con cables pelados o huele raro, **no la uses ni la cargues**: entrégala al encargado del laboratorio.
- **Cárgala solo con un cargador balanceador** (el que se conecta también al conector pequeño de varios cables), sobre una superficie que no arda y, si hay, dentro de una bolsa ignífuga para LiPo. **Nunca la dejes cargando sin nadie al lado.**
- **Tensión por celda.** Una LiPo tiene varias celdas en serie: «3S» significa 3 celdas. Cada celda está llena a 4,2 V y nunca debe bajar de 3,0 V. Para estas pruebas, úsala solo si tiene al menos **3,7 V por celda** en reposo (11,1 V en una 3S). Entre 3,0 y 3,7 V por celda, cárgala primero. Por debajo de 3,0 V por celda, no la uses ni la cargues: consulta al encargado.
- **Para guardarla más de unos días,** déjala en unos 3,8 V por celda (muchos cargadores tienen un modo *storage*).
- **El conector de la batería es el botón de emergencia.** Durante la fase 4, una persona tiene la mano en ese conector y lo desconecta ante cualquier cosa rara: ruido, olor, humo, calor o un motor que no se detiene.

### 2.2 Motores y ruedas

- Durante las pruebas de motores, el robot va **sobre un soporte con las tres ruedas en el aire**, sin tocar nada.
- Manos, cables, pelo y ropa suelta, lejos de las ruedas.
- No conectes ni desconectes cables de señal con la batería conectada.
- Orden para conectar: **primero USB, después batería.** Orden para desconectar: **primero batería, después USB.**

### 2.3 Placa

- Usa siempre el conector USB-C marcado **COM** de la placa, no el marcado **USB** (sección [1.3](#13-la-placa)).
- Carga programas siempre con la **batería desconectada.**

---

## 3. Materiales

| Material | Para qué |
|---|---|
| Computador con Windows 10/11 o Linux, con unos 10 GB libres | Programar la placa |
| Cable USB-C **de datos** (algunos cables solo cargan: si el computador no detecta nada, prueba otro) | Conectar la placa |
| El robot y su batería | — |
| Cargador balanceador de LiPo (y, si hay, bolsa ignífuga) | Cargar la batería con seguridad |
| Multímetro, o un medidor de LiPo con pitido | Medir la batería |
| Caja o soporte más alto que las ruedas | Dejar las ruedas en el aire |
| Cinta de enmascarar y marcador | Marcar ruedas y motores |
| Celular | Fotos y notas |

---

## Fase 0 — Preparar el computador (una sola vez)

Vas a instalar **ESP-IDF**, el conjunto de programas de Espressif para compilar y cargar código en el ESP32-S3. Todo el proyecto se compiló con la versión **v5.5.1**; cualquier otra v5.5.x también sirve.

> **Rutas sin espacios ni tildes.** ESP-IDF falla con rutas que tienen espacios, tildes o la letra ñ (por ejemplo `C:\Users\María José\...`). En esta guía todo va en `C:\esp\`.

### 0.1 Windows

1. Entra a la página de descargas de Espressif para Windows ([dl.espressif.com/dl/esp-idf](https://dl.espressif.com/dl/esp-idf)) y descarga el **instalador en línea universal** (*Universal Online Installer*).
2. Ejecútalo. Cuando pregunte:
   - **Versión de ESP-IDF:** elige **v5.5.1**. Si no aparece, la v5.5.x más reciente.
   - **Carpeta de ESP-IDF:** `C:\esp\esp-idf`.
   - Deja marcada la instalación de Git y de los controladores (*drivers*) si los ofrece.
3. Espera a que termine (puede tardar 20 a 40 minutos). Al final marca **Run ESP-IDF Command Prompt (cmd.exe)** y pulsa *Finish*.
4. Desde ahora, cada vez que esta guía diga «abre la terminal de ESP-IDF», busca en el menú Inicio el acceso directo **ESP-IDF 5.5 CMD** (o *ESP-IDF Command Prompt*). Es una ventana negra de comandos que ya tiene todo listo. Escribe ahí los comandos y presiona Enter después de cada uno.
5. Comprueba la instalación escribiendo:
   ```
   idf.py --version
   ```
   Debe responder algo como `ESP-IDF v5.5.1`.

> Si tu usuario de Windows tiene tildes o espacios y la instalación falla, crea la carpeta `C:\esp\tools`, define la variable de entorno `IDF_TOOLS_PATH` con el valor `C:\esp\tools` (menú Inicio → «Editar las variables de entorno del sistema» → *Variables de entorno* → *Nueva*, en las variables de usuario) y vuelve a ejecutar el instalador.

**Controlador del puerto serie (CH343).** La placa usa un chip CH343 para hablar con el computador. Windows 10/11 normalmente instala su controlador solo la primera vez que conectas la placa. Si en la fase 2 no aparece ningún puerto COM nuevo, instala el controlador oficial de WCH (archivo `CH343SER`, en [wch-ic.com](http://www.wch-ic.com/downloads/CH343SER_ZIP.html)) y vuelve a conectar la placa.

### 0.2 Linux (Ubuntu o Debian)

```bash
sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv \
     cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
sudo usermod -aG dialout $USER     # permiso para usar el puerto serie; luego cierra sesión y vuelve a entrar
```

Después de descargar el proyecto (paso 0.3), instala ESP-IDF con el script del repositorio:

```bash
~/esp/howlers-source/docs/striker_v3/herramientas/instalar_esp_idf.sh
```

En cada terminal nueva, activa ESP-IDF con `. ~/esp/esp-idf/export.sh` (eso es lo que esta guía llama «abrir la terminal de ESP-IDF»).

### 0.3 Descargar el proyecto

En la terminal de ESP-IDF:

**Windows**
```
cd C:\esp
git clone -b entrega-1 https://github.com/afzo1/howlers-source.git
```

**Linux**
```bash
mkdir -p ~/esp && cd ~/esp
git clone -b entrega-1 https://github.com/afzo1/howlers-source.git
```

`-b entrega-1` descarga la rama donde está el firmware de pruebas. Cuando esa rama se una a `main`, se puede omitir. Para traer cambios nuevos más adelante: `cd C:\esp\howlers-source` y `git pull`.

### 0.4 Compilar el firmware de pruebas (todavía sin el robot)

Compilar es traducir el código a un archivo que la placa entiende. Se hace una vez antes de ir al robot, para no perder tiempo allá.

**Windows**
```
cd C:\esp\howlers-source\firmware\apps\striker_hw_test
idf.py set-target esp32s3
idf.py build
```

**Linux**
```bash
cd ~/esp/howlers-source/firmware/apps/striker_hw_test
idf.py set-target esp32s3
idf.py build
```

La primera compilación tarda unos minutos. Debe terminar con **`Project build complete.`** Si aparece un error, revisa la [sección 6](#6-problemas-frecuentes).

---

## Fase 1 — Inspeccionar el robot (sin conectar nada)

### 1.1 Fotos

Antes de tocar nada, toma fotos claras del robot desde arriba y de cada lado, de la placa (donde se vean los cables y los números de los pines) y de la batería con su etiqueta. Sirven para el diseño de v3 y para volver todo a su lugar si algo se suelta.

### 1.2 Batería

1. Lee la etiqueta y anota: número de celdas (por ejemplo 3S), capacidad (mAh) y tipo de conector.
2. Mide la tensión total con el multímetro en el conector principal y divídela por el número de celdas. Aplica los límites de la [sección 2.1](#21-batería-lipo).
3. Si está baja, cárgala ahora con el cargador balanceador, mientras haces las fases 1 a 3.

### 1.3 La placa

La placa del robot debe ser una **YD-ESP32-S3 N16R8** (serigrafía «YD-ESP32-23»). Ubica en ella:

| Elemento | Dónde | Qué anotar |
|---|---|---|
| Conector USB-C **COM** | Uno de los dos conectores; al lado dice COM | Es el que vas a usar |
| Conector USB-C **USB** | El otro | No lo uses: Striker 1 usa una de sus líneas (GPIO20) para un motor |
| Botones **BOOT** y **RST** | Junto a los conectores | Solo se usan si falla la carga del programa |
| Puente de soldadura **IN-OUT** | Dos cuadritos de metal con ese rótulo | ¿Separados (abierto) o unidos con estaño (soldado)? |
| Puente de soldadura **RGB** | Igual, con el rótulo RGB | ¿Abierto o soldado? (en Striker 1, GPIO48 es una señal de motor) |

### 1.4 ¿Cómo se alimenta la placa en el robot?

Sigue los cables rojos que llegan a la placa y anota a qué pin llegan: **5V** (a veces rotulado 5Vin), **3V3**, o ninguno (la placa solo se alimentaría por USB).

| Lo que encuentras | Qué hacer |
|---|---|
| Ningún cable de alimentación llega a la placa | Puedes conectar USB y batería a la vez |
| Un cable llega al pin **5V** y el puente **IN-OUT está abierto** (como viene de fábrica) | Puedes conectar USB y batería a la vez. Con el puente abierto, el USB y el pin 5V están separados por diodos y no se alimentan entre sí |
| Un cable llega al pin **5V** y el puente **IN-OUT está soldado** | Haz las fases 2 y 3 solo con USB. **No hagas la fase 4** hasta consultarlo: con el puente soldado, el USB y la fuente del robot quedan conectados directamente |
| Un cable llega al pin **3V3** | Igual que el caso anterior: fases 2 y 3 solo con USB, y consultar antes de la fase 4 |

### 1.5 ¿Striker 1 o Striker 2? (inspección visual)

Los dos robots usan la misma placa pero distintos sensores y pines. La pista más clara es la IMU, el sensor de orientación:

| | Striker 1 | Striker 2 |
|---|---|---|
| IMU | **TM151** (marca Syd Dynamics), conectada por UART a GPIO17 y GPIO18 | **BNO055** (chip de Bosch, en una placa pequeña), conectada por I2C a GPIO11, 12 y 13 |
| Señales de los motores | GPIO20, 21, 47, 48, 45 y 0 | GPIO17, 18, 8, 3, 46 y 9 |
| Encoders AS5600 (salida analógica) | GPIO5, 6 y 7 | GPIO6, 7 y 1 |
| Sensor de distancia VL53L1X | GPIO41 y 42 | GPIO39 y 38 |

Anota tu conclusión, pero no la des por segura: en la fase 3 se confirma con mediciones.

### 1.6 Marcar las ruedas

Con cinta, marca las tres ruedas como **A**, **B** y **C**, en cualquier orden. En la fase 4 vas a averiguar qué motor y qué encoder corresponde a cada una.

---

## Fase 2 — Primera conexión por USB

> **La batería sigue desconectada.** El programa que trae el robot es desconocido. Uno de los firmwares viejos (`Robot-Control`) arranca moviendo el robot; sin batería, los motores no pueden moverse.

### 2.1 Encontrar el puerto

1. Pon el robot sobre el soporte, con las ruedas en el aire.
2. **Windows:** abre el *Administrador de dispositivos* (clic derecho en el botón Inicio) y despliega **Puertos (COM y LPT)**. Conecta el cable USB al conector **COM** de la placa y mira qué puerto nuevo aparece, por ejemplo **USB-Enhanced-SERIAL CH343 (COM5)** o **Dispositivo serie USB (COM5)**. Anota el número: en esta guía se escribe `COM5`; usa el tuyo.
3. **Linux:** ejecuta `ls /dev/ttyACM* /dev/ttyUSB*` antes y después de conectar. El nuevo suele ser `/dev/ttyACM0`. En los comandos de esta guía, donde dice `COM5`, escribe ese nombre.

En la placa debe encenderse un LED de alimentación.

### 2.2 Ver qué programa trae

En la terminal de ESP-IDF:

```
python -m esp_idf_monitor -p COM5
```

El monitor reinicia la placa y muestra lo que esta escribe al arrancar. Busca y anota:
- La línea **`Project name:`**, que dice qué firmware tiene (por ejemplo `robot-mov` o `Robot-Control`), y las líneas **`Compile time:`** y **`ESP-IDF:`**.
- Los mensajes de las primeras líneas: errores, reinicios en bucle, intentos de conectarse a un WiFi.

Para salir del monitor: **`Ctrl+]`**. Si tu teclado no tiene esa combinación, **`Ctrl+T`** y después **`X`**.

### 2.3 Guardar una copia del programa que trae (respaldo)

Antes de reemplazarlo, guarda una copia exacta de toda la memoria de la placa. Con ella se puede devolver el robot a su estado original cuando se quiera ([sección 7](#7-volver-al-programa-original-del-robot)).

**Windows**
```
mkdir C:\esp\respaldos
cd C:\esp\respaldos
python -m esptool --chip esp32s3 -p COM5 -b 921600 read-flash 0 ALL respaldo_robot.bin
```

**Linux**
```bash
mkdir -p ~/esp/respaldos && cd ~/esp/respaldos
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 921600 read-flash 0 ALL respaldo_robot.bin
```

Tarda unos minutos. Al final, el archivo `respaldo_robot.bin` debe pesar **16 MB** (16 777 216 bytes). Si el comando falla, repítelo con `-b 115200` en lugar de `-b 921600` (es más lento, pero más tolerante).

> Guarda el respaldo en un lugar seguro del equipo (por ejemplo una carpeta compartida), **no en GitHub**: puede contener contraseñas de WiFi del equipo anterior.

### 2.4 Cargar el firmware de pruebas

**Windows**
```
cd C:\esp\howlers-source\firmware\apps\striker_hw_test
idf.py -p COM5 flash monitor
```

**Linux**
```bash
cd ~/esp/howlers-source/firmware/apps/striker_hw_test
idf.py -p /dev/ttyACM0 flash monitor
```

`flash` carga el programa (unos 30 segundos) y `monitor` abre la consola. Después de unas líneas de arranque, debes ver:

```
==============================================================
 Striker - firmware de pruebas de hardware
==============================================================
 Robot: NINGUNO. No se configura ningun pin de salida.
 1) Usa 'identificar' para ayudarte a saber si es Striker 1 o 2.
 2) Luego 'robot s1' o 'robot s2' (se guarda y reinicia).
 Escribe 'help' para ver los comandos.
==============================================================

striker>
```

`striker>` indica que la placa espera un comando: escríbelo y presiona Enter.

**Guarda todo en un archivo.** Presiona `Ctrl+T` y luego `Ctrl+L`: el monitor empieza a guardar todo lo que aparece en un archivo llamado `log.striker_hw_test.<fecha y hora>.txt`, dentro de la carpeta `striker_hw_test`. Ese archivo es tu registro de la sesión. Repite la combinación al final para cerrarlo.

> Si aparece el aviso *«Your terminal application does not support escape sequences»*, no pasa nada: los comandos funcionan igual, solo que sin historial ni edición de la línea.

---

## Fase 3 — Pruebas sin motores

Escribe `help` para ver todos los comandos. Ninguna prueba de esta fase mueve motores.

### P1 · Datos de la placa: `info`

```
striker> info
```

| Línea | Resultado esperado | Si no |
|---|---|---|
| `Chip` | `esp32s3`, 2 núcleos | — |
| `Flash` | 16 MB | Puede ser otra placa: anótalo |
| `PSRAM` | 8 MB | Si dice `NO DETECTADA`, anótalo ([sección 6](#6-problemas-frecuentes)); las demás pruebas siguen sirviendo |
| `Ultimo reinicio` | Encendido normal o reinicio por USB | Si dice `CAIDA DE TENSION (brownout)`, la alimentación es insuficiente |

### P2 · ¿Striker 1 o 2? Medición: `identificar`

```
striker> identificar
```

Durante 15 segundos el programa solo **mide** la tensión de los pines GPIO1, 5, 6 y 7, sin enviar nada. Mientras tanto, **gira despacio con la mano cada rueda**, una vuelta cada una. Al final muestra una tabla y una conclusión:
- En Striker 1, los encoders están en GPIO5, 6 y 7: esos tres deben decir `CAMBIA al girar`.
- En Striker 2, están en GPIO6, 7 y 1, y GPIO5 suele decir `en alto (3,3 V)`.

Si la medición y la inspección visual (paso 1.5) coinciden, elige el robot:

```
striker> robot s1
```
(o `robot s2`). La placa guarda la elección y se reinicia. Ahora el encabezado debe decir `Robot: Striker 1` (o 2), `ESC: enviando pulsos de PARADA a los 3 motores` y `Motores DESARMADOS`.

Si dice `No concluyente`, o la medición y la inspección no coinciden, **no elijas**: anota la tabla, toma fotos de los cables de la placa y consulta antes de seguir. Si todos los pines dan `sin senal`, puede que los encoders se alimenten de la batería: mira la [sección 6](#6-problemas-frecuentes).

### P3 · Pines del robot elegido: `pines`

Muestra qué pin usa cada motor, encoder y sensor. Sirve para comparar con las fotos de la placa. Fíjate en los pines marcados como *strapping* o *USB nativo*: son los que el diseño de v3 debe evitar (hallazgo M5 del [diagnóstico](01_diagnostico_striker1_striker2.md)).

### P4 · Encoders: `encoders`

```
striker> encoders
```

Durante 20 segundos muestra 5 veces por segundo el ángulo (0 a 360°) y la tensión de los tres encoders, `E1`, `E2` y `E3`. Gira despacio la rueda **A** una vuelta completa:
- **Bien:** uno de los tres cambia de forma continua y da la vuelta (de 359° pasa a 0°); los otros dos casi no cambian.
- **Anota** qué encoder corresponde a la rueda A. Repite con B y C.
- Presiona Enter para terminar antes.

Es normal que el ángulo se quede un momento en 0° o en 360° en una parte de la vuelta: depende de cómo esté configurado el sensor. Lo importante es que cambie al girar y que cada rueda mueva un encoder distinto.

### P5 · Sensores I2C: `i2c`

Busca dispositivos en cada bus I2C del robot. Resultado esperado:

| Robot | Bus | Debe responder |
|---|---|---|
| Striker 1 | VL53L1X (distancia) | `0x29` |
| Striker 2 | BNO055 (IMU) | `0x29` o `0x28` |
| Striker 2 | VL53L1X (distancia) | `0x29` |
| Striker 2 | AS5600 (programación) | `0x36` |

`Ningun dispositivo respondio` o `El bus no responde` indican un cable suelto o un sensor sin alimentación.

### P6 · IMU: `imu`

```
striker> imu
```

- **Striker 2 (BNO055):** debe decir `identificador 0xA0 (correcto)` y mostrar `rumbo`, `alabeo` y `cabeceo`. Levanta el robot con cuidado y gíralo despacio: el rumbo debe cambiar cuando lo giras sobre la mesa, y el alabeo y el cabeceo cuando lo inclinas. Los números de `calib` suben de 0 a 3 a medida que el sensor se calibra; para el magnetómetro (el último número) hay que mover el robot en forma de 8.
- **Striker 1 (TM151):** el programa le pide los ángulos, igual que el firmware viejo, y debe mostrar líneas con `alabeo`, `cabeceo` y `rumbo`, y al final `La TM151 responde y entrega angulos: funciona.`

### P7 · WiFi: `wifi`

Lista las redes cercanas con su intensidad. Si aparecen redes, el radio funciona. Mientras más cerca de 0 la señal, mejor: −50 dBm es excelente y −85 dBm, débil.

---

## Fase 4 — Pruebas de motores (ruedas en el aire)

### 4.1 Antes de empezar

Revisa esta lista, punto por punto:

- [ ] La [sección 1.4](#14-cómo-se-alimenta-la-placa-en-el-robot) permite conectar USB y batería a la vez.
- [ ] La batería tiene al menos 3,7 V por celda y no está hinchada.
- [ ] El robot está sobre el soporte, firme, con las tres ruedas en el aire y sin nada cerca.
- [ ] En la consola, el robot elegido es el correcto y dice `Motores DESARMADOS`.
- [ ] Una segunda persona tiene la mano sobre el conector de la batería, lista para desconectarla.

### 4.2 Conectar la batería

Con el USB conectado y el programa corriendo, conecta la batería. **Ningún motor debe moverse.** Los ESC (los controladores de los motores) suelen hacer unos pitidos al recibir energía y otro al reconocer la señal de parada: anota cuántos pitidos y cómo suenan.

Si algo se mueve, **desconecta la batería de inmediato** y anótalo: no debería pasar.

Escribe `info`: `Ultimo reinicio` no debe haber cambiado. Si la placa se reinició al conectar la batería, hay un problema de alimentación: desconecta la batería y consulta.

### 4.3 Armar

```
striker> armar
```

Desde ahora se aceptan órdenes de motor. Cada orden dura como máximo 5 s, no pasa de 30 % de potencia y se detiene sola. **Cualquier tecla la detiene antes.**

### 4.4 Un motor a la vez, potencia baja

```
striker> motor 1 10
```

El motor 1 recibe 10 % durante 1,5 s. Mientras gira, la consola muestra la velocidad que mide cada encoder (`E1`, `E2`, `E3`, en rpm) y al final la velocidad media. Observa y anota:
- **Qué rueda giró** (A, B o C).
- **Hacia dónde**, mirando la rueda desde afuera del robot: horario o antihorario.
- **Qué encoder** mostró la velocidad, y si fue positiva o negativa.
- Ruidos, vibraciones o pitidos del ESC.

Si no gira, sube de a poco: `motor 1 15`, `motor 1 20`, `motor 1 25`. Algunos ESC no responden por debajo de cierta potencia. Anota el mínimo que la mueve. El programa no permite más de 30 %.

Repite con `motor 2 10` y `motor 3 10`.

### 4.5 Sentido contrario

```
striker> motor 1 10 1500 rev
```

`rev` pide el sentido contrario: la rueda debe girar al revés que en 4.4. Repite con los motores 2 y 3. Si alguna gira igual que antes, o no gira, anótalo: es información sobre cómo están conectados los ESC.

### 4.6 Respuesta a la potencia (opcional)

Para cada motor, a la misma potencia, compara la velocidad media:

```
striker> motor 1 20 3000
```

(20 % durante 3 s). Repite con 2 y 3. Si un motor queda mucho más lento que los otros con la misma orden, anótalo.

### 4.7 Los tres a la vez

```
striker> girar 10
```

Los tres motores reciben la misma orden. En el piso, eso haría girar al robot sobre sí mismo; aquí solo se comprueba que los tres responden juntos.

### 4.8 Terminar la fase

```
striker> desarmar
```

Luego **desconecta la batería**.

---

## Fase 5 — Terminar y registrar

1. Si activaste el registro (`Ctrl+T`, `Ctrl+L`), ciérralo con la misma combinación y sal del monitor con `Ctrl+]`.
2. **Primero la batería, después el USB.**
3. Si no vas a usar la batería en varios días, déjala en modo *storage* (unos 3,8 V por celda).
4. Llena la [plantilla del anexo A](#anexo-a--plantilla-de-registro) con lo que anotaste.
5. Comparte el archivo `log.striker_hw_test.<fecha y hora>.txt` (está en la carpeta `striker_hw_test`), la plantilla y las fotos: pégalos en el chat del proyecto o súbelos al repositorio en `docs/striker_v3/pruebas/`. Con esos datos se arma el mapa de pines y la tabla de motores de v3.

La placa queda con el firmware de pruebas, que no mueve nada al encender. Puedes dejarlo así.

---

## 6. Problemas frecuentes

| Problema | Causa probable | Qué hacer |
|---|---|---|
| `idf.py` no se reconoce | La terminal no es la de ESP-IDF | Abre el acceso directo **ESP-IDF 5.5 CMD** (Windows) o ejecuta `. ~/esp/esp-idf/export.sh` (Linux) |
| La compilación falla con errores de ruta | La ruta tiene espacios, tildes o ñ | Mueve el proyecto a `C:\esp\howlers-source` |
| No aparece ningún puerto COM | Cable solo de carga, controlador faltante o conector equivocado | Prueba otro cable; usa el conector **COM**; instala el controlador CH343 (fase 0.1) |
| `Failed to connect` al cargar | La placa no entró en modo de carga | Mantén presionado **BOOT**, presiona y suelta **RST**, suelta **BOOT** y repite el comando |
| La carga falla solo con el robot conectado, o el monitor muestra `waiting for download` al arrancar | Un cable del robot fuerza un pin de *strapping* (GPIO0, 3, 45 o 46) | Asegúrate de que la batería esté desconectada. Si sigue, consulta antes de desconectar cables: anota el mensaje completo |
| El monitor muestra caracteres raros | Velocidad equivocada | Usa el monitor de ESP-IDF; la velocidad es 115200 |
| `info` dice `PSRAM: NO DETECTADA` | La placa no es N16R8 o la PSRAM tiene otro modo | Anota la línea de `Flash` y una foto del módulo metálico (dice el modelo, por ejemplo `N16R8`) |
| `info` dice `CAIDA DE TENSION (brownout)` | Alimentación insuficiente | Prueba otro puerto USB o cable. Si pasó al arrancar un motor, revisa la batería y no sigas con la fase 4 |
| `identificar` o `encoders` no ven señal solo con USB | Los sensores se alimentan de la batería del robot | Con el robot elegido y **desarmado**, conecta la batería (si la sección 1.4 lo permite) y repite. Los motores no se mueven: reciben pulsos de parada |
| Un encoder no cambia al girar su rueda | Cable suelto, imán del sensor movido o sensor dañado | Anótalo con una foto del sensor |
| Dos ruedas mueven el mismo encoder | Cables de encoder cruzados | Anótalo: es parte del mapa del robot |
| El ESC pita sin parar y el motor no responde | El ESC no reconoce la señal de parada | Anota el patrón de pitidos y, si se puede, el modelo del ESC. **No intentes calibrarlo** |
| Un motor no gira ni a 30 % | Motor o ESC sin energía, cable suelto o ESC dañado | Desarma, desconecta la batería y revisa los conectores |
| Un motor sigue girando después de la orden | No debería pasar | **Desconecta la batería** y anota todo lo que pasó |
| `rev` no cambia el sentido | La línea de sentido del ESC no está conectada o el ESC no la usa | Anótalo |
| Olor, humo o algo caliente | Falla eléctrica | **Desconecta la batería**, aléjate y avisa al encargado |

---

## 7. Volver al programa original del robot

Con la batería desconectada y el respaldo de la fase 2.3:

**Windows**
```
cd C:\esp\respaldos
python -m esptool --chip esp32s3 -p COM5 -b 921600 write-flash 0 respaldo_robot.bin
```

**Linux**
```bash
cd ~/esp/respaldos
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 921600 write-flash 0 respaldo_robot.bin
```

La placa queda exactamente como estaba antes de las pruebas. Recuerda que ese programa original puede mover el robot al encender.

---

## Anexo A — Plantilla de registro

Copia esta plantilla en un archivo nuevo (por ejemplo `docs/striker_v3/pruebas/AAAA-MM-DD_registro.md`) y llénala.

```markdown
# Registro de pruebas del robot — AAAA-MM-DD

Participantes:
Computador y sistema operativo:
Versión de ESP-IDF (idf.py --version):

## Robot
- Robot (inspección visual): Striker 1 / Striker 2 / no sé
- Robot (identificar): Striker 1 / Striker 2 / no concluyente
- Programa que traía (Project name / Compile time / ESP-IDF):
- Respaldo guardado en:
- Placa: Flash ___ MB · PSRAM ___ MB
- Puente IN-OUT: abierto / soldado · Puente RGB: abierto / soldado
- Alimentación de la placa en el robot: solo USB / pin 5V / pin 3V3
- Batería: ___S · ___ mAh · conector ___ · tensión inicial ___ V (___ V por celda)

## Sensores
| Prueba | Resultado | Observaciones |
|---|---|---|
| info | | |
| identificar | | |
| encoders: rueda A → E_ | | |
| encoders: rueda B → E_ | | |
| encoders: rueda C → E_ | | |
| i2c | | |
| imu | | |
| wifi | | |

## Motores
| Motor | Rueda (A/B/C) | % mínimo que la mueve | Sentido sin rev | Sentido con rev | Encoder que la mide | rpm a 10 % | rpm a 20 % | Observaciones |
|---|---|---|---|---|---|---|---|---|
| 1 | | | | | | | | |
| 2 | | | | | | | | |
| 3 | | | | | | | | |

Pitidos de los ESC al conectar la batería:
Tensión final de la batería: ___ V

## Problemas y dudas
-
```

---

## Anexo B — Resumen de una página

1. **Una sola vez:** instalar ESP-IDF v5.5.1, descargar el proyecto (rama `entrega-1`) y compilar `firmware/apps/striker_hw_test`.
2. **Inspección:** fotos, batería (≥ 3,7 V por celda), puentes IN-OUT y RGB, alimentación de la placa, tipo de IMU, marcar ruedas A, B y C.
3. **Sin batería, por el conector COM:** `python -m esp_idf_monitor -p COM5` (anotar `Project name`) → respaldo con `read-flash 0 ALL` → `idf.py -p COM5 flash monitor`.
4. **Sin motores:** `info` → `identificar` → `robot s1` o `robot s2` → `pines` → `encoders` → `i2c` → `imu` → `wifi`.
5. **Con batería y ruedas en el aire:** conectar batería (nada se mueve) → `armar` → `motor 1 10`, `motor 2 10`, `motor 3 10` → lo mismo con `rev` → `girar 10` → `desarmar`.
6. **Al terminar:** primero batería, después USB. Compartir el archivo `log.striker_hw_test...txt`, la plantilla y las fotos.

**Emergencia:** desconectar la batería.
