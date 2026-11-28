# striker_bench_math: benchmark de matemáticas (Entrega 2, P2)

Mide en el ESP32-S3 cuántos ciclos de CPU cuesta cada variante de los cálculos de Striker v3 y cuánto error agrega el punto fijo:

- **PID por rueda:** la forma discreta de `pid_ext` del banco de autotuning ([`pid_ext.c`](../../components/control/pid_lib/autotuning_rnn/pid_ext.c), función `pid_calc_discrete`) en double, float y Q16.16, y también `pid_ext` tal cual (recalcula sus coeficientes en cada paso).
- **Filtro y derivada del encoder** (desenrollado del ángulo y filtro de velocidad del banco), en double, float y Q16.16.
- **Multiplicación de matrices** de 3×3, 4×4, 6×6, 8×8 y 16×16: C simple, `dspm_mult_f32_ansi`, `dspm_mult_f32_ae32` y `dspm_mult_f32_aes3` (SIMD del S3), más las versiones fijas de 3×3 y 4×4.
- **Un paso del EKF de 6 estados** (predicción, corrección con la odometría y con el rumbo de la IMU): C simple, esp-dsp con 6×6 y esp-dsp con las matrices rellenadas hasta 8×8.

**No usa ningún pin:** solo calcula e imprime. Corre una vez al arrancar, en una tarea fijada al Core 1, con el WiFi apagado, a 240 MHz y con `-O2` (la misma configuración de `striker_v3`).

## Cómo correrlo

En la **segunda placa** (no hace falta el robot), conectada por el puerto **COM** (UART0):

```bash
cd firmware/apps/striker_bench_math
idf.py set-target esp32s3
idf.py build                       # la primera vez descarga esp-dsp (necesita internet)
idf.py -p PUERTO flash monitor     # PUERTO: COM5, /dev/ttyUSB0, etc.
```

Tarda unos segundos. Al final imprime un bloque CSV para copiar a una hoja de cálculo (separador coma, decimal punto). Para repetirlo, presionar RST. Para salir del monitor: `Ctrl + ]`.

Si la placa solo tiene el USB nativo (sin puerto COM), quitar las líneas de consola de `sdkconfig.defaults` y borrar `sdkconfig` antes de compilar. Nunca hacerlo en la placa del robot: en Striker 1, GPIO 20 (línea D+ del USB nativo) es la señal de un motor.

## Qué se espera

- **Ciclos** (orientativo): el PID en double es varias veces más lento que en float (de 5 a 20 veces), porque el S3 no tiene unidad de double y lo hace por software; Q16.16 y float quedan parecidos. En matrices, Espressif reporta 3×3: 200 ciclos con la versión fija frente a 560 con ANSI, y 16×16: 6 280 con `aes3` frente a 56 915 con ANSI. Con 3×3 y 6×6, `aes3` no usa SIMD (las dimensiones deben ser múltiplos de 4 y las matrices estar alineadas a 16 bytes), así que mejora mucho menos que con 4×4, 8×8 y 16×16.
- **Error de cuantización:** sale igual en la PC y en la placa, así que estos valores sí son exactos:

  | Cálculo | float frente a double | Q16.16 frente a double |
  |---|---|---|
  | PID, 20 000 pasos (salida en ±80, RMS de la salida 19,80) | RMS 4,27e-04 | RMS 7,46e-03 (máximo 1,22e-02) |
  | Filtro del encoder, 20 000 pasos (RMS de la velocidad 10,60 rad/s) | RMS 1,12e-06 | RMS 2,80e-05 |

  En el PID, la resolución de Q16.16 (1/65536) limita la ganancia integral efectiva, que es muy pequeña (unos 0,007 por paso). Por eso el error de Q16.16 es unas 17 veces el de float.
- **EKF:** «Las 3 variantes [...] coinciden» y un error de posición de unos 2,4 mm frente a la trayectoria simulada. Si alguna línea dice `ERROR`, el resultado no sirve: avisar antes de usar los tiempos.
