# Origen de estos archivos

Copia **sin cambios** de los drivers propios de `wheel-movement`, tomados del repositorio original
[`nparra-code/striker-1`](https://github.com/nparra-code/striker-1/tree/dc56d28) en el commit `dc56d28`. Se restauraron porque, al reorganizar el monorepo,
la app quedó apuntando a drivers compartidos de otra app, con otra API, y dejó de compilar.

Motivo y detalle: [`docs/striker_v3/01-1_correcciones_compilacion.md`](../../../../../docs/striker_v3/01-1_correcciones_compilacion.md), corrección W3.

Cada archivo se comparó byte a byte con el original (`cmp`). **No editarlos aquí:** son la referencia
del firmware viejo. El firmware nuevo (Striker v3) tendrá una sola versión de cada driver.

| Archivo aquí | Archivo en el repositorio original |
|---|---|
| `drivers/pid_lib/include/pid.h` | `wheel-movement/drivers/pid_lib/include/pid.h` |
| `drivers/pid_lib/include/pid_ext.h` | `wheel-movement/drivers/pid_lib/include/pid_ext.h` |
| `drivers/pid_lib/pid.c` | `wheel-movement/drivers/pid_lib/pid.c` |
| `drivers/pid_lib/pid_ext.c` | `wheel-movement/drivers/pid_lib/pid_ext.c` |
| `drivers/sensor_fusion/include/sensor_fusion.h` | `wheel-movement/drivers/sensor_fusion/include/sensor_fusion.h` |
| `drivers/sensor_fusion/sensor_fusion.c` | `wheel-movement/drivers/sensor_fusion/sensor_fusion.c` |
| `platform/include/platform_esp32s3.h` | `wheel-movement/platform/include/platform_esp32s3.h` |
| `platform/platform_esp32s3.c` | `wheel-movement/platform/platform_esp32s3.c` |
| `sensors/AS5600_HAL_Library/as5600_lib.c` | `wheel-movement/sensors/AS5600_HAL_Library/as5600_lib.c` |
| `sensors/AS5600_HAL_Library/include/as5600_defs.h` | `wheel-movement/sensors/AS5600_HAL_Library/include/as5600_defs.h` |
| `sensors/AS5600_HAL_Library/include/as5600_lib.h` | `wheel-movement/sensors/AS5600_HAL_Library/include/as5600_lib.h` |
