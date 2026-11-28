# striker_hw_test — firmware de pruebas de hardware

Programa pequeño para revisar, pieza por pieza y sin riesgos, el robot heredado (Striker 1 o Striker 2) antes de usar cualquier firmware de control. Se maneja con comandos escritos desde el computador por el cable USB.

**Instrucciones completas, desde cero:** [`docs/striker_v3/guia_pruebas_robot.md`](../../../docs/striker_v3/guia_pruebas_robot.md).

## Qué revisa

| Pieza | Comando | ¿Mueve motores? |
|---|---|---|
| Placa: chip, flash, PSRAM, motivo del último reinicio | `info` | No |
| Si el robot es Striker 1 o Striker 2 (midiendo, sin enviar nada) | `identificar` | No |
| Encoders AS5600 (ángulo de cada rueda) | `encoders` | No |
| Sensores I2C (IMU BNO055, distancia VL53L1X, AS5600) | `i2c` | No |
| IMU (BNO055 en Striker 2; TM151 en Striker 1) | `imu` | No |
| Radio WiFi | `wifi` | No |
| Cada motor y su encoder | `motor`, `girar` | **Sí**, con límites |

## Reglas de seguridad del programa

1. **Sin robot elegido no configura ningún pin como salida.** Es el estado inicial después de cargarlo.
2. Con un robot elegido (`robot s1` o `robot s2`), cada pin se usa en el mismo papel que en el firmware original de ese robot, y al arrancar solo envía pulsos de **parada** a los ESC.
3. Los motores solo se mueven con una orden escrita, después de `armar`, con **potencia máxima de 30 %** y **duración máxima de 5 s**. Al terminar vuelven solos a parada.
4. Cualquier tecla detiene un motor en marcha.

Los límites se cambian en `idf.py menuconfig` → *Striker - firmware de pruebas*. No conviene subirlos hasta conocer el ESC.

## Compilar y cargar

Requiere ESP-IDF v5.5.1 (o otra v5.5.x). Desde esta carpeta, en una terminal con ESP-IDF activo:

```bash
idf.py set-target esp32s3        # solo la primera vez
idf.py build
idf.py -p PUERTO flash monitor   # PUERTO: COM5, /dev/ttyACM0, etc.
```

Conectar la placa por el conector USB-C marcado **COM** (chip CH343), no por el marcado USB: Striker 1 usa una línea del USB nativo (GPIO20) para un motor. Para salir del monitor: `Ctrl+]` (o `Ctrl+T` y luego `X`).

## Comandos

| Comando | Uso |
|---|---|
| `help` | Lista los comandos |
| `info` | Datos de la placa y robot elegido |
| `identificar [segundos]` | Prueba pasiva en GPIO1, 5, 6 y 7. Si las ruedas giran a mano, se giran mientras mide; si no (Striker 1), se dejan quietas |
| `robot s1` · `robot s2` · `robot ninguno` | Elige el robot; se guarda y la placa se reinicia |
| `pines` | Pines que usa el robot elegido |
| `encoders [segundos]` | Ángulo y tensión de los tres encoders, 5 veces por segundo; al final, el mínimo y el máximo de cada uno (con las ruedas quietas, su diferencia es el ruido) |
| `i2c` | Busca dispositivos en cada bus I2C del robot |
| `imu [segundos]` | BNO055: orientación y estado de calibración. TM151: le pide los ángulos, como `robot-mov`, y los muestra |
| `wifi` | Lista las redes WiFi cercanas y su señal |
| `armar` · `desarmar` | Habilita o bloquea las órdenes de motor |
| `parar` | Pone los tres motores en parada |
| `motor <1-3> <%> [ms] [rev]` | Mueve un motor. Ejemplo: `motor 1 10 1500` |
| `girar <%> [ms] [rev]` | Misma orden a los tres motores |

Durante `motor` y `girar` se imprime la velocidad (rpm) que mide cada encoder, para saber qué encoder corresponde a cada motor y en qué sentido gira. Al terminar la orden se muestran, por encoder, la velocidad media, la tensión mínima y máxima y las vueltas que dio. Así se mide, sin girar las ruedas a mano (en Striker 1 no giran), el rango de salida del AS5600 (de ~330 a ~2970 mV en el modo 10–90 %, desde casi 0 mV en el modo 0–100 %) y cuántas vueltas da el encoder por cada vuelta de la rueda.

## Pines (tomados de los firmwares originales)

| Señal | Striker 1 (`robot-mov`) | Striker 2 (`Robot-Control`) |
|---|---|---|
| Motor 1: acelerador / sentido | GPIO20 / GPIO21 (derecho) | GPIO17 / GPIO18 |
| Motor 2: acelerador / sentido | GPIO47 / GPIO48 (izquierdo) | GPIO8 / GPIO3 |
| Motor 3: acelerador / sentido | GPIO45 / GPIO0 (trasero) | GPIO46 / GPIO9 |
| Encoders AS5600 (analógicos) | GPIO5, 6, 7 | GPIO6, 7, 1 |
| IMU | TM151 por UART: TX GPIO17, RX GPIO18 | BNO055 por I2C: SDA 12, SCL 11, RST 13 |
| Distancia VL53L1X | SDA 41, SCL 42 | SDA 39, SCL 38, XSHUT 41 |
| Pulso de parada / 100 % | 700 µs / 2400 µs | 1000 µs / 2400 µs |
| Sentido: adelante / reversa | 860 µs / 2220 µs | 1980 µs / 1420 µs |

Las señales a los ESC son pulsos a 50 Hz, igual que en los firmwares originales. GPIO0, 3, 45 y 46 son pines de *strapping* y GPIO20 es D+ del USB nativo (hallazgo M5 del diagnóstico): por eso conviene cargar el programa con la batería desconectada.

## Archivos

| Archivo | Contenido |
|---|---|
| `main/main.c` | Arranque y consola |
| `main/robot_config.c` | Tabla de pines de cada robot y modelo de pulsos del ESC |
| `main/esc.c` | Salidas MCPWM hacia los ESC |
| `main/encoders.c` | Lectura analógica de los AS5600 |
| `main/cmd_basic.c` | `info`, `robot`, `pines`, `identificar`, `wifi` |
| `main/cmd_sensors.c` | `encoders`, `i2c`, `imu` |
| `main/imu_tm151.c` | Prueba de la TM151 con la biblioteca del fabricante (`components/sensors/tm151/v1_striker1`) |
| `main/cmd_motors.c` | `armar`, `desarmar`, `parar`, `motor`, `girar` |
| `sdkconfig.defaults` | Placa N16R8: 16 MB de flash, PSRAM octal, consola por UART0 |

Este programa es solo para pruebas: no es parte del firmware Striker v3.
