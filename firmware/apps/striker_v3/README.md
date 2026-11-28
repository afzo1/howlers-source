# striker_v3: firmware unificado (esqueleto con medición de tiempos)

Estado actual: todas las tareas de la Entrega 1 creadas de forma estática y fijadas a su núcleo ([`03_freertos_tiempo_real.md`](../../../docs/striker_v3/03_freertos_tiempo_real.md)), el lazo de 1 kHz despertado por el `gptimer` y un módulo que mide los tiempos con el propio ESP32-S3, sin analizador lógico. Las tareas todavía no hacen su trabajo (encoders, PID, IMU, red): solo se miden.

**Seguridad: no configura ningún pin de motor.** El único pin que puede mover es el de medida, y solo si se activa con `pin_medida`, que acepta únicamente GPIO 9 a 14 (libres en Striker 1 y Striker 2).

## Cómo correrlo

En la **segunda placa** (no hace falta el robot), por el puerto **COM**:

```bash
cd firmware/apps/striker_v3
idf.py set-target esp32s3
idf.py build
idf.py -p PUERTO flash monitor     # PUERTO: COM5, /dev/ttyUSB0, etc.
```

La configuración (`sdkconfig.defaults` y `particiones_v3.csv`) es la de la Entrega 1 para la YD-ESP32-S3 N16R8 (16 MB de flash y 8 MB de PSRAM octal), más la consola solo por UART0. En una placa con otra PSRAM, el arranque falla.

## Comandos

| Comando | Qué hace |
|---|---|
| `estado` | Modo, tiempo encendido, activaciones de `T_ctrl` (y perdidas), pin de medida y carga |
| `stats reset` | Reinicia las medidas (hacerlo al empezar cada prueba) |
| `stats` | Tabla de medidas, tareas periódicas, carga de cada núcleo, pila libre y memoria |
| `hist <medida>` | Histograma en CSV (casillas de 1 µs) para graficar. Correrlo **antes** de `stats`: `stats` perturba un poco al final |
| `pin_medida <9..14\|off>` | Activa el pin de medida y la captura MCPWM |
| `carga_ctrl <0..200>` | Hace que `T_ctrl` simule ese trabajo, en µs (para validar las medidas) |
| `conteo <on\|off>` | Cada 10 s imprime `conteo <activaciones> <us desde el arranque>` |

Medidas de `stats` (en µs):

- `lat_isr`: de la alarma del `gptimer` a la entrada de su ISR (contador del `gptimer`, 0,1 µs).
- `activacion`: de la alarma al momento en que `T_ctrl` empieza. Su variación (`max-min`) es el jitter.
- `periodo`: entre dos ISR seguidas (contador de ciclos de la CPU).
- `duracion`: trabajo de `T_ctrl` (contador de ciclos).
- `cap_periodo` y `cap_ancho`: periodo y ancho del pulso en el pin de medida, medidos por hardware con la captura MCPWM (12,5 ns).

La carga de cada núcleo es 100 % menos lo que usó su tarea `IDLE0` o `IDLE1` desde el último `stats reset`.

## Qué se espera (placa sola)

| Prueba | Cómo | Esperado |
|---|---|---|
| Arranque | `flash monitor` | Mensaje de inicio y el prompt `v3>` en menos de 1 s |
| Activaciones | `stats` a los 60 s de un `stats reset` | ~60 000 en `periodo`, 0 perdidas |
| Periodo | `stats` | `periodo` entre ~995 y ~1005 µs |
| Latencia de la ISR | `stats` | `lat_isr` del orden de 1–2 µs |
| Activación | `stats` | `activacion` del orden de 3–10 µs, con poca variación |
| Captura | `pin_medida 14`, `stats reset`, 60 s, `stats` | `cap_periodo` de 1000 µs ± unos pocos µs, igual a `periodo` |
| Duración vacía | `stats` con `carga_ctrl 0` | `duracion` y `cap_ancho` menores a 5 µs |
| Cadena de medida | `carga_ctrl 20`, `stats reset`, 60 s, `stats` | `duracion` y `cap_ancho` ≈ 20 µs; carga del Core 1 ≈ 2 % |
| Multímetro | `carga_ctrl 20` y el multímetro en voltaje DC entre GPIO 14 y GND | Promedio = 20 µs ÷ 1000 µs × V del pin 3V3: ~66 mV con 3,30 V |
| Reloj externo | `conteo on`, monitor con `--timestamps` 10 min | (activaciones finales − iniciales) ÷ segundos de la PC ≈ 1000 por segundo (±0,01 %) |
