# Guía de pruebas del robot heredado

**Proyecto:** firmware unificado Striker v3 · Howlers FC / Robosyn · Universidad de Antioquia
**Para quién:** cualquier integrante del equipo, aunque nunca haya programado un ESP32.
**Fecha:** 11 de noviembre de 2026 · **Actualizada:** 27 de noviembre de 2026 (mediciones de la Entrega 2, batería 6S y cables de alimentación en 3V3) · **Firmware de pruebas:** [`firmware/apps/striker_hw_test`](../../firmware/apps/striker_hw_test/README.md)

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
| Los datos del robot que necesita la Entrega 2 (tabla de la [sección 1](#mediciones-para-la-entrega-2)) | |

El programa de pruebas (`striker_hw_test`) no mueve nada al encender, solo mueve un motor cuando se le ordena con un comando escrito, y se detiene solo. Así se puede revisar el robot pieza por pieza sin sorpresas.

**Tiempo estimado la primera vez:** 3 a 4 horas: casi una hora para instalar programas en el computador y otra para las mediciones de la Entrega 2.

---

## 1. Mapa de la guía

| Fase | Qué se hace | ¿Batería conectada? |
|---|---|---|
| [Fase 0](#fase-0--preparar-el-computador-una-sola-vez) | Preparar el computador (una sola vez) | — |
| [Fase 1](#fase-1--inspeccionar-el-robot-sin-conectar-nada) | Inspeccionar y medir el robot, sin conectar nada | No (salvo en [1.4.1](#141-si-un-cable-lleva-energía-al-pin-3v3), si hace falta) |
| [Fase 2](#fase-2--primera-conexión-por-usb) | Conectar por USB, ver qué programa trae, guardarlo y cargar el de pruebas | **No** |
| [Fase 3](#fase-3--pruebas-sin-motores) | Pruebas sin motores | No (salvo que los sensores la necesiten) |
| [Fase 4](#fase-4--pruebas-de-motores-ruedas-en-el-aire) | Pruebas de motores, con las ruedas en el aire | **Sí** |
| [Fase 5](#fase-5--terminar-y-registrar) | Terminar, registrar y compartir resultados | No |

Al final hay una [tabla de problemas frecuentes](#6-problemas-frecuentes), las instrucciones para [devolver el robot a su programa original](#7-volver-al-programa-original-del-robot) y una [plantilla para anotar resultados](#anexo-a--plantilla-de-registro).

### Mediciones para la Entrega 2

El firmware v3 necesita datos medidos del robot. Están repartidos en las fases y marcados en el texto de **[M1]** a **[M16]**. Anótalos en la [plantilla del anexo A](#anexo-a--plantilla-de-registro). Los valores esperados son los de Striker 1.

| # | Qué | Dónde | Esperado en Striker 1 |
|---|---|---|---|
| M1 | Confirmar que es Striker 1 | [Fase 3](#fase-3--pruebas-sin-motores): P2, P5 y P6 | `identificar` dice Striker 1; `i2c` encuentra el VL53L1X (`0x29`); `imu` muestra ángulos |
| M2 | Rango del encoder y vueltas del encoder por vuelta de rueda, con el motor | [4.4](#44-un-motor-a-la-vez-potencia-baja) | Modo 10–90 %: de ~0,33 a ~2,97 V. Modo 0–100 %: de ~0 V hasta quedar plano cerca de 3,1 V |
| M3 | ADC frente al multímetro | [P4](#p4--encoders-encoders) | Diferencia menor a ~30 mV |
| M4 | Ruido del encoder | [P4](#p4--encoders-encoders) | Variación menor a ~30 mV con la rueda quieta |
| M5 | Signo de cada encoder | [4.4](#44-un-motor-a-la-vez-potencia-baja) | Anotar + o − por motor |
| M6 | Curva de cada motor | [4.6](#46-curva-de-cada-motor) | Zona muerta; después crece casi en línea recta |
| M7 | ¿Llega a 20 rad/s? | [4.6](#46-curva-de-cada-motor) | ~190 rpm al 30 % |
| M8 | Radio de rueda r | [1.7](#17-medidas-del-robot) | ≈ 3,25 cm |
| M9 | Radio de la base L | [1.7](#17-medidas-del-robot) | Unos 7–9 cm |
| M10 | Posición de cada rueda | [1.7](#17-medidas-del-robot) y [4.4](#44-un-motor-a-la-vez-potencia-baja) | Separadas 120° |
| M11 | ESC: etiqueta y pitidos | [1.1](#11-fotos) y [4.2](#42-conectar-la-batería) | — |
| M12 | Batería: etiqueta y tensiones | [1.2](#12-batería) | Recién cargada: 25,0–25,2 V; celdas a menos de 0,05 V entre sí |
| M13 | IMU: giro de 90° | [P6](#p6--imu-imu) | El rumbo cambia ~90° (±5°) |
| M14 | Señal WiFi en el sitio de pruebas | [P7](#p7--wifi-wifi) | Mejor que −70 dBm |
| M15 | Opcional: qué hace el ESC si la placa se reinicia | [4.8](#48-opcional-qué-hace-el-esc-si-la-placa-se-reinicia) | Anotar si el motor se detiene, sigue o pita |
| M16 | Pin de depuración GPIO 14 libre | [1.8](#18-pin-de-depuración-gpio-14) | Libre |

---

## 2. Reglas de seguridad (leer antes de empezar)

### 2.1 Batería LiPo

Las baterías de litio-polímero (LiPo) guardan mucha energía. Bien tratadas son seguras; mal tratadas pueden incendiarse.

- **Revísala antes de cada sesión.** Si está hinchada, golpeada, con cables pelados o huele raro, **no la uses ni la cargues**: entrégala al encargado del laboratorio.
- **Cárgala solo con un cargador balanceador** que admita 6 celdas (el que se conecta también al conector pequeño de varios cables), sobre una superficie que no arda y, si hay, dentro de una bolsa ignífuga para LiPo. **Nunca la dejes cargando sin nadie al lado.**
- **Tensión por celda.** Una LiPo tiene varias celdas en serie: «6S» significa 6 celdas (la de Striker 1 dice «6S cell 22.2V»). Cada celda está llena a 4,2 V y nunca debe bajar de 3,0 V. Límites para estas pruebas:

| Estado | Por celda | Batería 6S |
|---|---|---|
| Cargada | 4,2 V | 25,2 V |
| Mínimo en reposo para usarla. Si baja de aquí, termina la sesión y cárgala | 3,7 V | 22,2 V |
| Para guardarla varios días (modo *storage* del cargador) | 3,8 V | 22,8 V |
| Límite absoluto: por debajo, no la uses ni la cargues; consulta al encargado | 3,0 V | 18,0 V |

- **El firmware no mide la batería:** el robot no tiene un divisor de tensión que lleve su voltaje a un pin del ESP32. Por eso se mide a mano con el multímetro, con los motores quietos:
  - la **tensión total**, en el conector principal, antes de empezar y cada ~20 minutos de uso de motores;
  - una vez por sesión, **cada celda**, en el conector de balanceo (el pequeño, de 7 cables): entre cada par de cables vecinos debe haber 3,7–4,2 V, y las 6 celdas deben estar a menos de ~0,05 V entre sí. Si alguna difiere más de 0,1 V, cárgala con balanceo antes de usarla.
- **Si tienen un medidor de LiPo con pitido,** conéctalo al conector de balanceo mientras usas el robot, con la alarma en 3,5 V por celda, y desconéctalo al terminar (conectado, descarga la batería).
- **El conector de la batería es el botón de emergencia.** Durante la fase 4, una persona tiene la mano en ese conector y lo desconecta ante cualquier cosa rara: ruido, olor, humo, calor o un motor que no se detiene.

### 2.2 Motores y ruedas

- Durante las pruebas de motores, el robot va **sobre un soporte con las tres ruedas en el aire**, sin tocar nada.
- Manos, cables, pelo y ropa suelta, lejos de las ruedas.
- No conectes ni desconectes cables de señal con la batería conectada.
- Orden para conectar: **primero USB, después batería.** Orden para desconectar: **primero batería, después USB.**

### 2.3 Placa

- Usa siempre el conector USB-C marcado **COM** de la placa, no el marcado **USB** (sección [1.3](#13-la-placa)).
- Carga programas siempre con la **batería desconectada.**
- **Al medir con el multímetro,** toca un solo pin a la vez, con la punta fina. Con la batería conectada, la punta roja solo toca el punto que vas a medir: si los 25 V de la batería llegan a un pin de la placa, la dañan al instante.

---

## 3. Materiales

| Material | Para qué |
|---|---|
| Computador con Windows 10/11 o Linux, con unos 10 GB libres | Programar la placa |
| Cable USB-C **de datos** (algunos cables solo cargan: si el computador no detecta nada, prueba otro) | Conectar la placa |
| El robot y su batería | — |
| Cargador balanceador de LiPo que admita 6S (y, si hay, bolsa ignífuga) | Cargar la batería con seguridad |
| Multímetro con voltaje DC y continuidad (el modo que pita) | Medir la batería, la alimentación de la placa y los encoders |
| Recomendado: medidor de LiPo con pitido (de 1 a 8 celdas) | Vigilar cada celda durante la fase 4 |
| Caja o soporte más alto que las ruedas | Dejar las ruedas en el aire |
| Calibrador (pie de rey) y flexómetro | Medir las ruedas y la base (1.7) |
| Escuadra, o una hoja de papel (su esquina es un ángulo recto) | Marcar un ángulo recto para la prueba de la IMU |
| Cinta de enmascarar, cinta aislante y marcador | Marcar ruedas y motores; aislar un cable desconectado (1.4.1) |
| Celular | Fotos y notas |

---

## Fase 0 — Preparar el computador (una sola vez)

Vas a instalar **ESP-IDF**, el conjunto de programas de Espressif para compilar y cargar código en el ESP32-S3. Todo el proyecto se compiló con la versión **v5.5.1**; cualquier otra v5.5.x también sirve. **No uses una v6.x**, aunque el instalador la ofrezca primero: cambia funciones que usa el proyecto, y los resultados no serían comparables.

> **Rutas sin espacios ni tildes.** ESP-IDF falla con rutas que tienen espacios, tildes o la letra ñ (por ejemplo `C:\Users\María José\...`). En esta guía todo va en `C:\esp\`.

### 0.1 Windows

1. Entra a la página de descargas de Espressif para Windows ([dl.espressif.com/dl/esp-idf](https://dl.espressif.com/dl/esp-idf)) y descarga el **instalador en línea universal** (*Universal Online Installer*).
2. Ejecútalo. Cuando pregunte:
   - **Versión de ESP-IDF:** elige **v5.5.1**. Si no aparece, la v5.5.x más reciente. No elijas una v6.x.
   - **Carpeta de ESP-IDF:** `C:\esp\esp-idf`. Si ya tienes ahí otra versión, usa `C:\esp\v5.5.1\esp-idf`: las dos pueden convivir.
   - Deja marcada la instalación de Git y de los controladores (*drivers*) si los ofrece.
3. Espera a que termine (puede tardar 20 a 40 minutos). Al final marca **Run ESP-IDF Command Prompt (cmd.exe)** y pulsa *Finish*.
4. Desde ahora, cada vez que esta guía diga «abre la terminal de ESP-IDF», busca en el menú Inicio el acceso directo **ESP-IDF 5.5 CMD** (o *ESP-IDF Command Prompt*). Es una ventana negra de comandos que ya tiene todo listo. Escribe ahí los comandos y presiona Enter después de cada uno.
5. Comprueba la instalación escribiendo:
   ```
   idf.py --version
   ```
   Debe responder algo como `ESP-IDF v5.5.1`. Si responde otra versión, estás en la terminal de otra instalación: abre la de la 5.5.

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
git clone https://github.com/afzo1/howlers-source.git
```

**Linux**
```bash
mkdir -p ~/esp && cd ~/esp
git clone https://github.com/afzo1/howlers-source.git
```

Se descarga la rama `main`, que ya trae el firmware de pruebas. Para traer cambios nuevos más adelante: `cd C:\esp\howlers-source` y `git pull`.

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

Antes de tocar nada, toma fotos claras de:
- el robot desde arriba y de cada lado;
- la placa, donde se vean los cables y los números de los pines, y su módulo metálico (el rótulo dice el modelo, por ejemplo `N16R8`);
- **[M11]** la etiqueta de cada ESC (los controladores de los motores): modelo, corriente, número de celdas que admite y si dice **BEC** (entrega 5 V por su cable rojo) u **OPTO** (no entrega);
- la batería con su etiqueta.

Sirven para el diseño de v3 y para volver todo a su lugar si algo se suelta.

### 1.2 Batería

**[M12]**
1. Lee la etiqueta y anota: número de celdas (Striker 1 usa una 6S), capacidad (mAh) y tipo de conector.
2. Mide la tensión total con el multímetro en el conector principal y compárala con la tabla de la [sección 2.1](#21-batería-lipo).
3. Mide cada celda en el conector de balanceo, como explica la sección 2.1, y anota las seis tensiones.
4. Si está baja o desbalanceada, cárgala ahora con el cargador balanceador, mientras haces las fases 1 a 3.

### 1.3 La placa

La placa del robot debe ser una **YD-ESP32-S3 N16R8** (serigrafía «YD-ESP32-23»). Ubica en ella:

| Elemento | Dónde | Qué anotar |
|---|---|---|
| Conector USB-C **COM** | Uno de los dos conectores; al lado dice COM | Es el que vas a usar |
| Conector USB-C **USB** | El otro | No lo uses: Striker 1 usa una de sus líneas (GPIO20) para un motor |
| Botones **BOOT** y **RST** | Junto a los conectores | Solo se usan si falla la carga del programa |
| Puente de soldadura **IN-OUT** | Dos cuadritos de metal con ese rótulo | ¿Separados (abierto) o unidos con estaño (soldado)? |
| Puente de soldadura **RGB** | Igual, con el rótulo RGB | ¿Abierto o soldado? (en Striker 1, GPIO48 es una señal de motor) |
| Módulo metálico | La caja plateada, junto a la antena impresa | El modelo: `N16R8` significa 16 MB de flash y 8 MB de PSRAM |

### 1.4 ¿Cómo se alimenta la placa en el robot?

Mira los cables que llegan a los pines **5V** (a veces rotulado 5Vin) y **3V3** de la placa. Cada uno es de una de estas dos clases:

- **Lleva energía a la placa.** Viene del lado de la batería: de un ESC (un cable de servo de 3 hilos: señal, rojo y negro o café), de un regulador de tensión (una plaquita con una bobina, a veces con un tornillo de ajuste y los rótulos IN y OUT) o de una tarjeta de distribución.
- **Toma energía de la placa.** Va a un sensor: un encoder AS5600 (la plaquita junto a cada motor), la IMU o el sensor de distancia. Es normal: los AS5600 deben alimentarse a 3,3 V para que su salida no pase de lo que admite el pin del ESP32 ([`as5600_lib.h:9-12`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/components/sensors/as5600/v1_striker1/include/as5600_lib.h#L9-L12)). Si en la fase 3 los encoders dan lecturas solo con el USB, es que la placa ya los alimenta.

Con la batería y el USB desconectados, sigue cada cable desde el pin hasta su otro extremo. Si no se ve, usa el multímetro en continuidad (el modo que pita): una punta en el pin de la placa y la otra en el extremo que sospechas.

En Striker 1 llega al pin 3V3 un cable rojo desde cada motor. Si ese mismo cable trae un hilo a GND y otro a GPIO 5, 6 o 7, es el cable del encoder de ese motor: toma energía de la placa. Si un hilo del mismo cable va a GPIO 20, 21, 47, 48, 45 o 0, es de un ESC: sigue 1.4.1. Si dudas, haz la prueba de tensión de [1.4.1](#141-si-un-cable-lleva-energía-al-pin-3v3) (pasos 2 a 4): un cable que solo toma energía de la placa marca 0 V.

| Lo que encuentras | Qué hacer |
|---|---|
| Ningún cable lleva energía a la placa (los que hay en 5V o 3V3 van a sensores) | Puedes conectar USB y batería a la vez |
| Un cable lleva energía al pin **5V** y el puente **IN-OUT está abierto** (como viene de fábrica) | Puedes conectar USB y batería a la vez. Con el puente abierto, el USB y el pin 5V están separados por diodos y no se alimentan entre sí |
| Un cable lleva energía al pin **5V** y el puente **IN-OUT está soldado** | Haz las fases 2 y 3 solo con USB y, antes de la fase 4, sigue [1.4.1](#141-si-un-cable-lleva-energía-al-pin-3v3): con el puente soldado, el USB y la fuente del robot quedan conectados directamente |
| Un cable lleva energía al pin **3V3** | Fases 2 y 3 solo con USB y, antes de la fase 4, [1.4.1](#141-si-un-cable-lleva-energía-al-pin-3v3) |
| No logras saber de dónde viene | Fases 2 y 3 solo con USB; envía fotos al equipo y consulta antes de la fase 4 |

#### 1.4.1 Si un cable lleva energía al pin 3V3

(Vale también para el pin 5V con el puente IN-OUT soldado.) La regla es: **con el USB conectado, la placa se alimenta solo del USB.** Ese cable se desconecta durante las pruebas y, antes, se mide qué tensión entrega.

1. **Si es el cable rojo de un ESC,** mira su etiqueta (foto de 1.1). Si dice BEC (por ejemplo «BEC 5V/2A»), ese ESC entrega unos 5 V por el cable rojo: con la batería conectada, llegarían al pin 3V3 y dañarían al instante el ESP32-S3 (admite como máximo 3,6 V) y los sensores. Sigue con el paso 2 sin conectar la batería con ese cable puesto.
2. Con la batería y el USB desconectados, **desconecta de la placa todos los cables que llevan energía** (solo los rojos: los negros, GND, se quedan) y cubre sus puntas con cinta aislante. Si el rojo va en un mismo conector con la señal (el cable de servo de un ESC), desconecta ese conector completo. Si un cable está soldado, no lo cortes ni lo desueldes: toma fotos y consulta.
3. Con el multímetro en continuidad, comprueba que el GND de la placa sigue unido al negativo de la batería (en el conector de la batería del lado del robot, con la batería desconectada): debe pitar. Si no pita, consulta: los ESC necesitan esa tierra común para entender los pulsos.
4. Con el robot sobre el soporte, las ruedas en el aire y el USB **desconectado**, conecta la batería, con la mano lista para desconectarla. La placa queda apagada y los ESC no reciben pulsos, así que no pueden arrancar (pueden pitar: es normal). Mide con el multímetro en voltaje DC: punta negra en un GND de la placa (o en el cable negro del mismo conector) y punta roja en la punta de cada cable desconectado; destápala solo para medir y vuelve a taparla. Después desconecta la batería.
5. Decide con esta tabla:

| Tensión medida | Qué es | Qué hacer |
|---|---|---|
| 3,2–3,4 V | Un regulador de 3,3 V | Es correcto para el pin 3V3. Durante las pruebas con USB, déjalo desconectado y aislado. Para que el robot ande sin USB (rutas en el piso de la Entrega 2), se vuelve a conectar con el USB desconectado |
| 4,5–6,5 V | Una salida de 5 V, como el BEC de un ESC | **Nunca al pin 3V3.** Déjalo desconectado y consulta: su lugar sería el pin 5V, con el puente IN-OUT abierto. Si es el cable de servo de un ESC, ese motor no se prueba hasta resolverlo |
| 0 V | No lleva energía (o el interruptor del robot, si tiene, estaba apagado) | Revisa a dónde va. Si va a un sensor o es el cable rojo de un ESC sin BEC, vuelve a conectarlo, con la batería y el USB desconectados |
| Otro valor | Desconocido | Déjalo desconectado y consulta |

Si el cable estaba en el pin 5V (con el puente soldado), lo esperado es 4,5–5,5 V: déjalo desconectado mientras uses el USB y consulta si conviene abrir el puente.

6. Pega en la placa una etiqueta que diga qué cable quedó desconectado, para que nadie use el robot sin USB pensando que tiene alimentación. Sigue con las fases 2 a 4 con la placa alimentada solo por el USB.

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

Con cinta, marca las tres ruedas como **A**, **B** y **C**, en cualquier orden. En la fase 4 vas a averiguar qué motor y qué encoder corresponde a cada una. Esas letras son solo marcas: la «B» no tiene que ver con la rueda trasera, que en el código también se llama B.

Haz además una raya con marcador en el borde de cada rueda: sirve para contar sus vueltas en el video del paso [4.4](#44-un-motor-a-la-vez-potencia-baja).

**En Striker 1 las ruedas no giran a mano:** su mecanismo no lo permite. No las fuerces. Solo se mueven con los motores, en la fase 4; por eso esta guía no pide girarlas a mano en ese robot.

### 1.7 Medidas del robot

El firmware v3 convierte la velocidad del robot en la de cada rueda con estas medidas. Tómalas con el robot apagado:

- **[M8] Radio de rueda r.** Con el calibrador, mide el diámetro exterior de cada rueda, sobre los rodillos, y divide por 2. El código de Striker 1 usa 3,25 cm ([`mov_calculation.h:12`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L12)).
- **[M9] Radio de la base L** (distancia del centro del robot al punto de apoyo de cada rueda). Con el robot en el piso, pega un trozo de cinta junto a cada rueda y marca en ella el centro de la rueda, a ras del piso. Levanta el robot y mide con el flexómetro las tres distancias entre las marcas: d₁, d₂ y d₃. Si las ruedas están separadas 120°, las tres son iguales (±3 mm) y L = d ÷ 1,732, con d el promedio. Esperado: unos 7–9 cm si el robot mide 16–18 cm de diámetro.
- **[M10] Posición de cada rueda.** Toma una foto desde arriba, con el celular centrado sobre el robot. En la fase 4 vas a saber qué motor mueve cada rueda: entonces escribe en la foto cuál es la derecha (motor 1, R), la izquierda (motor 2, L) y la trasera (motor 3, B). El frente del robot es el lado opuesto a la rueda trasera; la derecha y la izquierda deben quedar a 60° del frente, una a cada lado (las tres separadas 120°).

### 1.8 Pin de depuración (GPIO 14)

**[M16]** El firmware v3 usará un pin libre para medir tiempos: GPIO 14. Con el robot apagado (batería y USB desconectados):
1. Mira que el pin 14 de la placa no tenga cable ni pista conectada.
2. Con el multímetro en continuidad, comprueba que GPIO 14 **no** pita ni contra GND ni contra 3V3.

Si está ocupado, elige otro pin libre que no sea de arranque, USB, flash ni PSRAM (evita 0, 3, 19, 20, 26–37 y 43–46) y anótalo.

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
| `ESP-IDF` | `v5.5.1` (u otra v5.5.x) | Si dice otra versión, por ejemplo `v6.1-beta1`, el programa se compiló con otra instalación: revisa la [sección 6](#6-problemas-frecuentes) antes de seguir con la fase 4 |
| `Flash` | 16 MB | Puede ser otra placa: anótalo |
| `PSRAM` | 8 MB | Si dice `NO DETECTADA`, anótalo y revisa la [sección 6](#6-problemas-frecuentes); las demás pruebas siguen sirviendo |
| `Ultimo reinicio` | Encendido normal o reinicio por USB | Si dice `CAIDA DE TENSION (brownout)`, la alimentación es insuficiente |

### P2 · ¿Striker 1 o 2? Medición: `identificar`

```
striker> identificar
```

Durante 15 segundos el programa solo **mide** la tensión de los pines GPIO1, 5, 6 y 7, sin enviar nada. Si las ruedas giran a mano, gira despacio cada una, una vuelta; si no giran (Striker 1), déjalas quietas: la conclusión sale igual, porque se basa en la tensión de cada pin. Al final muestra una tabla y una conclusión **[M1]**:
- En Striker 1, los encoders están en GPIO5, 6 y 7: esos tres deben decir `tension fija en el rango de un AS5600` con las ruedas quietas (o `CAMBIA al girar` si se giraron).
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

Durante 20 segundos muestra 5 veces por segundo el ángulo (0 a 360°) y la tensión de los tres encoders, `E1`, `E2` y `E3`, y al final el mínimo y el máximo de cada uno. Si escribes un número después del comando (`encoders 60`), dura esos segundos (máximo 120). Presiona Enter para terminar antes.

**Striker 1: las ruedas no giran a mano.** Su mecanismo no lo permite: **no las fuerces**. Con las ruedas quietas, cada encoder debe mostrar una tensión fija entre ~0,3 y ~3 V. Qué encoder mide cada rueda, su rango y sus vueltas se averiguan en la fase 4, con los motores (paso [4.4](#44-un-motor-a-la-vez-potencia-baja)).

**Si las ruedas sí giran a mano** (Striker 2), gira despacio la rueda **A** una vuelta completa:
- **Bien:** uno de los tres cambia de forma continua y da la vuelta (de 359° pasa a 0°); los otros dos casi no cambian.
- **Anota** qué encoder corresponde a la rueda A. Repite con B y C.
- Es normal que el ángulo se quede un momento en 0° o en 360° en una parte de la vuelta: depende de cómo esté configurado el sensor.

Mediciones de la Entrega 2, con las ruedas quietas:
- **[M4] Ruido.** `encoders 10`. Anota el mínimo y el máximo que muestra al final para cada encoder: la variación debe ser menor a ~30 mV. Si es mayor, revisa la masa (GND) y los cables.
- **[M3] ADC frente al multímetro.** Con `encoders 60` corriendo, mide con el multímetro en voltaje DC (punta negra en GND) la tensión del pin de cada encoder (GPIO 5, 6 y 7 de la placa), sin tocar dos pines a la vez. Compárala con los mV de la consola: la diferencia debe ser menor a ~30 mV.

### P5 · Sensores I2C: `i2c`

Busca dispositivos en cada bus I2C del robot **[M1]**. Resultado esperado:

| Robot | Bus | Debe responder |
|---|---|---|
| Striker 1 | VL53L1X (distancia) | `0x29` |
| Striker 2 | BNO055 (IMU) | `0x29` o `0x28` |
| Striker 2 | VL53L1X (distancia) | `0x29` |
| Striker 2 | AS5600 (programación) | `0x36` |

`Ningun dispositivo respondio` o `El bus no responde` indican un cable suelto o un sensor sin alimentación.

En Striker 1, `Ningun dispositivo respondio` también puede significar que el VL53L1X no está instalado: el firmware viejo `robot-mov` no lo usa (su inicialización está comentada, [`main.c:73-81`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/main.c#L73-L81)). Mira si el sensor está en el robot y si sus cables llegan a GPIO41 (SDA) y GPIO42 (SCL). Si está, mide su alimentación con el multímetro (pin VIN o VCC del sensor contra GND): si da 0 V solo con el USB, se alimenta de la batería y la prueba se repite en el paso [4.2](#42-conectar-la-batería).

### P6 · IMU: `imu`

```
striker> imu
```

- **Striker 2 (BNO055):** debe decir `identificador 0xA0 (correcto)` y mostrar `rumbo`, `alabeo` y `cabeceo`. Levanta el robot con cuidado y gíralo despacio: el rumbo debe cambiar cuando lo giras sobre la mesa, y el alabeo y el cabeceo cuando lo inclinas. Los números de `calib` suben de 0 a 3 a medida que el sensor se calibra; para el magnetómetro (el último número) hay que mover el robot en forma de 8.
- **Striker 1 (TM151):** el programa le pide los ángulos, igual que el firmware viejo, y debe mostrar líneas con `alabeo`, `cabeceo` y `rumbo`, y al final `La TM151 responde y entrega angulos: funciona.` **[M1]**
- **[M13] Giro de 90°.** Pega en la mesa dos tiras de cinta en ángulo recto (guíate con la esquina de una hoja) y haz una raya de referencia en el soporte. Con el robot encima del soporte y `imu 30` corriendo, alinea la raya con la primera tira y anota el rumbo; gira el soporte hasta alinearla con la segunda y anota el rumbo otra vez: debe cambiar ~90° (±5°). Gira el soporte, no arrastres el robot sobre sus ruedas: en Striker 1 no ruedan a mano y podrías dañarlas.
- Si dice `Bytes recibidos: 0`, la IMU no envió nada. Lo más común es que no tenga alimentación solo con el USB. Mide con el multímetro la tensión entre sus pines de alimentación (VCC o VIN contra GND): si da 0 V, se alimenta de la batería; repite `imu` y [M13] en el paso [4.2](#42-conectar-la-batería), con los motores desarmados. Si tiene alimentación, revisa que el TX de la IMU llegue a GPIO18 y su RX a GPIO17.

### P7 · WiFi: `wifi`

Lista las redes cercanas con su intensidad. Si aparecen redes, el radio funciona. Mientras más cerca de 0 la señal, mejor: −50 dBm es excelente y −85 dBm, débil.

**[M14]** Haz esta prueba en el sitio donde se va a probar el robot. La red de pruebas (un router propio o el *hotspot* de un celular; las redes de la universidad suelen bloquear el tráfico entre dispositivos) debe tener una señal mejor que −70 dBm. Anota su nombre y su señal.

---

## Fase 4 — Pruebas de motores (ruedas en el aire)

### 4.1 Antes de empezar

Revisa esta lista, punto por punto:

- [ ] La [sección 1.4](#14-cómo-se-alimenta-la-placa-en-el-robot) permite conectar USB y batería a la vez, o ya hiciste [1.4.1](#141-si-un-cable-lleva-energía-al-pin-3v3) y los cables que llevan energía a la placa están desconectados y aislados.
- [ ] `info` muestra ESP-IDF v5.5.x (P1).
- [ ] La batería tiene al menos 22,2 V (3,7 V por celda) en reposo, sus celdas están parejas y no está hinchada.
- [ ] El robot está sobre el soporte, firme, con las tres ruedas en el aire y sin nada cerca.
- [ ] En la consola, el robot elegido es el correcto y dice `Motores DESARMADOS`.
- [ ] Una segunda persona tiene la mano sobre el conector de la batería, lista para desconectarla.
- [ ] Anota la hora: cada ~20 minutos de uso de motores, mide otra vez la batería, con los motores quietos.

### 4.2 Conectar la batería

Con el USB conectado y el programa corriendo, conecta la batería. **Ningún motor debe moverse.** Los ESC suelen hacer unos pitidos al recibir energía y otro al reconocer la señal de parada: anota cuántos pitidos y cómo suenan **[M11]**.

Si algo se mueve, **desconecta la batería de inmediato** y anótalo: no debería pasar.

Escribe `info`: `Ultimo reinicio` no debe haber cambiado. Si la placa se reinició al conectar la batería, hay un problema de alimentación: desconecta la batería y consulta.

**Sensores que se alimentan de la batería.** Si en la fase 3 `i2c` o `imu` no respondieron solo con el USB, repítelos ahora: los motores siguen desarmados. Si la IMU responde, haz también [M13] (giro de 90°), con cuidado de no tirar de los cables.

### 4.3 Armar

```
striker> armar
```

Desde ahora se aceptan órdenes de motor. Cada orden dura como máximo 5 s, no pasa de 30 % de potencia y se detiene sola. **Cualquier tecla la detiene antes.**

### 4.4 Un motor a la vez, potencia baja

```
striker> motor 1 10 3000
```

El motor 1 recibe 10 % durante 3 s. Mientras gira, la consola muestra la velocidad que mide cada encoder (`E1`, `E2`, `E3`, en rpm). Al terminar muestra la velocidad media y, para cada encoder, su tensión mínima y máxima durante la orden y las vueltas que dio. Observa y anota:
- **Qué rueda giró** (A, B o C). **[M10]** El código llama al motor 1 derecho (R), al 2 izquierdo (L) y al 3 trasero (B) ([`control_main.h:51-58`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L51-L58)), y espera que los midan E1, E2 y E3, en ese orden ([`control_main.h:30-32`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/include/control_main.h#L30-L32)): comprueba que la rueda que gira está en esa posición y escríbelo en la foto de 1.7.
- **Hacia dónde**, mirando la rueda desde afuera del robot: horario o antihorario.
- **Qué encoder** mostró la velocidad y **[M5]** si fue positiva o negativa. Ese es el encoder de esa rueda: los otros dos casi no cambian.
- **[M2] Rango del encoder.** Mira la línea `minimo..maximo` del encoder que giró. De ~330 a ~2970 mV es el modo 10–90 % (el que supone el programa). Si el mínimo baja cerca de 0 mV y el máximo se queda cerca de 3100 mV, es el modo 0–100 %: el ADC no mide más arriba, así que hay unos grados ciegos en cada vuelta.
- **[M2] Vueltas del encoder por vuelta de rueda.** Graba la rueda con el celular durante la orden y cuenta en el video las vueltas que dio, con la raya de 1.6. Compáralas con las `vueltas` de su encoder. Si son casi iguales, el imán del encoder está en el eje de la rueda. Si el encoder da muchas más, está en el motor, antes del reductor (el código de Striker 1 declara un reductor planetario de 16 a 1: [`mov_calculation.h:11`](https://github.com/afzo1/howlers-source/blob/0451897bc2c902e6e356acf153d503e0143c7b3a/firmware/apps/striker_v1/robot-mov/main/include/mov_calculation.h#L11)). Si la rueda gira muy rápido para contarla, repite con la potencia mínima que la mueve.
- Ruidos, vibraciones o pitidos del ESC.

Si no gira, sube de a poco: `motor 1 15 3000`, `motor 1 20 3000`, `motor 1 25 3000`. Algunos ESC no responden por debajo de cierta potencia. Anota el mínimo que la mueve. El programa no permite más de 30 %.

Repite con `motor 2 10 3000` y `motor 3 10 3000`.

### 4.5 Sentido contrario

```
striker> motor 1 10 1500 rev
```

`rev` pide el sentido contrario: la rueda debe girar al revés que en 4.4. Repite con los motores 2 y 3. Si alguna gira igual que antes, o no gira, anótalo: es información sobre cómo están conectados los ESC.

### 4.6 Curva de cada motor

**[M6]** Para cada motor, da cinco órdenes de 3 s, de menor a mayor potencia, con unos segundos de pausa entre una y otra. De cada una, anota la línea `Velocidad media (sin el arranque)` del encoder de esa rueda:

```
striker> motor 1 10 3000
striker> motor 1 15 3000
striker> motor 1 20 3000
striker> motor 1 25 3000
striker> motor 1 30 3000
```

Repite con los motores 2 y 3.
- **Esperado:** por debajo de algún porcentaje la rueda no gira (zona muerta); después, la velocidad crece casi en línea recta. Si un motor queda mucho más lento que los otros con la misma orden, anótalo.
- **[M7]** Para pasar a rad/s: rpm × 0,1047. El autotuning usa consignas de hasta ~17–20 rad/s, es decir, ~160–190 rpm medidas por el encoder. Anota si al 30 % se llega a ~190 rpm; si no, en v3 habrá que subir el límite con cuidado.
- Las rpm son las del eje que mide el encoder: si en [M2] contaste más de una vuelta del encoder por vuelta de rueda, la rueda gira más despacio en esa misma proporción.

### 4.7 Los tres a la vez

```
striker> girar 10
```

Los tres motores reciben la misma orden. En el piso, eso haría girar al robot sobre sí mismo; aquí solo se comprueba que los tres responden juntos.

### 4.8 Opcional: qué hace el ESC si la placa se reinicia

**[M15]** Sirve para saber qué le pasa a un motor si el programa se reinicia mientras gira. Ruedas en el aire y la segunda persona con la mano en el conector de la batería:

```
striker> motor 1 10 5000
```

Mientras gira, presiona el botón **RST** de la placa. Anota si el motor se detiene, sigue girando o pita, y cuánto tarda en detenerse. Si no se detiene en un par de segundos, desconecta la batería. Al arrancar de nuevo, el programa envía pulsos de parada y queda desarmado: escribe `armar` si vas a seguir.

### 4.9 Terminar la fase

```
striker> desarmar
```

Luego **desconecta la batería** y mide su tensión total para la plantilla.

---

## Fase 5 — Terminar y registrar

1. Si activaste el registro (`Ctrl+T`, `Ctrl+L`), ciérralo con la misma combinación y sal del monitor con `Ctrl+]`.
2. **Primero la batería, después el USB.**
3. Si no vas a usar la batería en varios días, déjala en modo *storage* (3,8 V por celda: 22,8 V en una 6S).
4. Si en 1.4.1 desconectaste cables, déjalos así, aislados y con la etiqueta en la placa.
5. Llena la [plantilla del anexo A](#anexo-a--plantilla-de-registro) con lo que anotaste, incluidas las mediciones M1 a M16.
6. Comparte el archivo `log.striker_hw_test.<fecha y hora>.txt` (está en la carpeta `striker_hw_test`), la plantilla y las fotos: pégalos en el chat del proyecto o súbelos al repositorio en `docs/striker_v3/pruebas/`. Con esos datos se arma el mapa de pines, la tabla de motores y la configuración (`RobotConfig`) de v3.

La placa queda con el firmware de pruebas, que no mueve nada al encender. Puedes dejarlo así.

---

## 6. Problemas frecuentes

| Problema | Causa probable | Qué hacer |
|---|---|---|
| `idf.py` no se reconoce | La terminal no es la de ESP-IDF | Abre el acceso directo **ESP-IDF 5.5 CMD** (Windows) o ejecuta `. ~/esp/esp-idf/export.sh` (Linux) |
| `idf.py --version` o la línea `ESP-IDF` de `info` muestran otra versión (por ejemplo v6.x) | Se instaló o se usó otra versión de ESP-IDF | Instala v5.5.1 (fase 0; puede convivir con la otra) y abre su terminal. En `striker_hw_test`, borra la carpeta `build` y el archivo `sdkconfig` (se crean de nuevo) y repite 0.4 y 2.4. Si al arrancar dice `Robot: NINGUNO`, repite `robot s1` |
| La compilación falla con errores de ruta | La ruta tiene espacios, tildes o ñ | Mueve el proyecto a `C:\esp\howlers-source` |
| No aparece ningún puerto COM | Cable solo de carga, controlador faltante o conector equivocado | Prueba otro cable; usa el conector **COM**; instala el controlador CH343 (fase 0.1) |
| `Failed to connect` al cargar | La placa no entró en modo de carga | Mantén presionado **BOOT**, presiona y suelta **RST**, suelta **BOOT** y repite el comando |
| La carga falla solo con el robot conectado, o el monitor muestra `waiting for download` al arrancar | Un cable del robot fuerza un pin de *strapping* (GPIO0, 3, 45 o 46) | Asegúrate de que la batería esté desconectada. Si sigue, consulta antes de desconectar cables: anota el mensaje completo |
| El monitor muestra caracteres raros | Velocidad equivocada | Usa el monitor de ESP-IDF; la velocidad es 115200 |
| `info` dice `PSRAM: NO DETECTADA` | La placa no es N16R8 (tiene otra PSRAM o ninguna), o el programa se compiló con otra versión de ESP-IDF | 1) Si `info` muestra una versión distinta de v5.5.x, corrige eso primero (fila de arriba) y repite `info`. 2) Con la batería desconectada y el monitor cerrado: `python -m esptool --chip esp32s3 -p COM5 flash-id`. Anota las líneas `Features` (una N16R8 dice `Embedded PSRAM 8MB`) y `Detected flash size`. 3) Foto del módulo metálico (dice el modelo, por ejemplo `N16R8`). 4) Copia las líneas del arranque que mencionan `psram`. Si esptool dice `Embedded PSRAM 8MB` y aun así `info` dice `NO DETECTADA`, la placa está bien y el problema está en la compilación |
| `info` dice `CAIDA DE TENSION (brownout)` | Alimentación insuficiente | Prueba otro puerto USB o cable. Si pasó al arrancar un motor, revisa la batería y no sigas con la fase 4 |
| `identificar` o `encoders` no ven señal solo con USB | Los sensores se alimentan de la batería del robot | Con el robot elegido y **desarmado**, conecta la batería (si la sección 1.4 lo permite, o después de 1.4.1) y repite. Los motores no se mueven: reciben pulsos de parada |
| `imu` dice `Bytes recibidos: 0` | La IMU no tiene alimentación solo con el USB, o TX y RX están cruzados | Mide su alimentación (P6). Si es 0 V, repite en el paso 4.2, con la batería conectada y los motores desarmados |
| `i2c` no encuentra nada en Striker 1 | El VL53L1X no está instalado, no tiene alimentación o va a otros pines | Revisa P5. Es de baja prioridad para la Entrega 2: puede quedar para el final |
| Las ruedas no giran a mano | En Striker 1 es normal: su mecanismo no lo permite | No las fuerces. El encoder de cada rueda, su rango y sus vueltas se miden con los motores (paso 4.4) |
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
Versión de ESP-IDF (idf.py --version y línea ESP-IDF de info):

## Robot
- Robot (inspección visual): Striker 1 / Striker 2 / no sé
- Robot (identificar): Striker 1 / Striker 2 / no concluyente
- Programa que traía (Project name / Compile time / ESP-IDF):
- Respaldo guardado en:
- Placa: Flash ___ MB · PSRAM ___ MB · rótulo del módulo ___ · línea Features de esptool ___
- Puente IN-OUT: abierto / soldado · Puente RGB: abierto / soldado
- Alimentación de la placa en el robot (1.4): ningún cable le lleva energía / pin 5V / pin 3V3 · de dónde viene ___
- Cables desconectados en 1.4.1 y tensión medida: ___
- [M12] Batería: ___S · ___ mAh · conector ___ · total ___ V · celdas ___ / ___ / ___ / ___ / ___ / ___ V
- [M11] ESC: modelo ___ · corriente ___ A · celdas ___ · BEC / OPTO · pitidos al conectar la batería ___
- [M16] GPIO 14: libre / ocupado (pin alternativo: ___)

## Geometría
- [M8] Diámetro de las ruedas: A ___ · B ___ · C ___ cm → r = ___ cm
- [M9] Distancias entre apoyos: d1 ___ · d2 ___ · d3 ___ cm → L = promedio ÷ 1,732 = ___ cm
- [M10] Foto desde arriba con R, L y B marcadas: ___ · ¿coincide con el código? sí / no

## Sensores
| Prueba | Resultado | Observaciones |
|---|---|---|
| info | | |
| identificar [M1] | | |
| encoders en reposo (P4) | | |
| i2c [M1] | | |
| imu [M1] | | |
| [M13] imu, giro de 90° | | |
| [M14] wifi: red de pruebas y señal | | |

| Encoder | Rueda (A/B/C, de 4.4) | [M4] mín–máx en reposo (mV) | [M3] consola / multímetro (mV) | [M2] mín–máx con el motor (mV) | Modo (10–90 % o 0–100 %) | [M2] vueltas del encoder / vueltas de la rueda en el video |
|---|---|---|---|---|---|---|
| E1 (GPIO5) | | | | | | |
| E2 (GPIO6) | | | | | | |
| E3 (GPIO7) | | | | | | |

## Motores
| Motor | Rueda (A/B/C) | Posición (derecha/izquierda/trasera) | % mínimo que la mueve | Sentido sin rev | Sentido con rev | Encoder que la mide | [M5] Signo sin rev | Observaciones |
|---|---|---|---|---|---|---|---|---|
| 1 | | | | | | | | |
| 2 | | | | | | | | |
| 3 | | | | | | | | |

[M6] Curva: rpm medias del encoder de cada rueda (órdenes de 3 s)
| Motor | 10 % | 15 % | 20 % | 25 % | 30 % | [M7] ¿~190 rpm al 30 %? |
|---|---|---|---|---|---|---|
| 1 | | | | | | |
| 2 | | | | | | |
| 3 | | | | | | |

[M15] Opcional, RST con el motor 1 girando:
Batería: inicial ___ V · cada ~20 min ___ · final ___ V

## Problemas y dudas
-
```

---

## Anexo B — Resumen de una página

1. **Una sola vez:** instalar ESP-IDF v5.5.1 (no una v6.x), descargar el proyecto (rama `main`) y compilar `firmware/apps/striker_hw_test`.
2. **Inspección:** fotos (placa, módulo, ESC y batería), batería 6S (≥ 22,2 V y celdas parejas), puentes IN-OUT y RGB, alimentación de la placa (1.4; si un cable lleva energía al pin 3V3, 1.4.1), tipo de IMU, marcar ruedas A, B y C (con una raya para contar vueltas; en Striker 1 no giran a mano), medir r y L, revisar GPIO 14.
3. **Sin batería, por el conector COM:** `python -m esp_idf_monitor -p COM5` (anotar `Project name`) → respaldo con `read-flash 0 ALL` → `idf.py -p COM5 flash monitor`.
4. **Sin motores:** `info` (ESP-IDF v5.5.x y PSRAM 8 MB) → `identificar` → `robot s1` o `robot s2` → `pines` → `encoders` con las ruedas quietas (ruido y multímetro) → `i2c` → `imu` (giro de 90° girando el soporte) → `wifi` (mejor que −70 dBm).
5. **Con batería y ruedas en el aire:** conectar batería (nada se mueve; repetir `i2c` e `imu` si fallaron) → `armar` → `motor 1 10 3000`, `motor 2 10 3000`, `motor 3 10 3000` (rueda, posición, signo, rango del encoder y vueltas con video) → lo mismo con `rev` → curva de 10 a 30 % → `girar 10` → opcional: RST con un motor girando → `desarmar` → medir la batería.
6. **Al terminar:** primero batería, después USB. Compartir el archivo `log.striker_hw_test...txt`, la plantilla y las fotos.

**Emergencia:** desconectar la batería.
