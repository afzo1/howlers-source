# Origen de estos archivos

Copia **sin cambios** de los drivers propios de `Velocity-Control`, tomados del repositorio original
[`MaverickST/RoboCup_Striker2`](https://github.com/MaverickST/RoboCup_Striker2/tree/e182de0) en el commit `e182de0`. Se restauraron porque, al reorganizar el monorepo,
la app quedó apuntando a drivers compartidos de otra app, con otra API, y dejó de compilar.

Motivo y detalle: [`docs/striker_v3/01-1_correcciones_compilacion.md`](../../../../../docs/striker_v3/01-1_correcciones_compilacion.md), corrección V1.

Cada archivo se comparó byte a byte con el original (`cmp`). **No editarlos aquí:** son la referencia
del firmware viejo. El firmware nuevo (Striker v3) tendrá una sola versión de cada driver.

| Archivo aquí | Archivo en el repositorio original |
|---|---|
| `control/control_senfusion.c` | `Velocity-Control/components/control/control_senfusion.c` |
| `control/control_senfusion.h` | `Velocity-Control/components/control/control_senfusion.h` |
| `drivers/as5600/as5600_defs.h` | `Velocity-Control/components/drivers/as5600/as5600_defs.h` |
| `drivers/as5600/as5600_lib.c` | `Velocity-Control/components/drivers/as5600/as5600_lib.c` |
| `drivers/as5600/as5600_lib.h` | `Velocity-Control/components/drivers/as5600/as5600_lib.h` |
| `drivers/bldc_pwm/bldc_pwm.c` | `Velocity-Control/components/drivers/bldc_pwm/bldc_pwm.c` |
| `drivers/bldc_pwm/bldc_pwm.h` | `Velocity-Control/components/drivers/bldc_pwm/bldc_pwm.h` |
| `drivers/bno055/bno055.c` | `Velocity-Control/components/drivers/bno055/bno055.c` |
| `drivers/bno055/bno055.h` | `Velocity-Control/components/drivers/bno055/bno055.h` |
| `drivers/bno055/bno055_defs.h` | `Velocity-Control/components/drivers/bno055/bno055_defs.h` |
| `drivers/led/led.c` | `Velocity-Control/components/drivers/led/led.c` |
| `drivers/led/led.h` | `Velocity-Control/components/drivers/led/led.h` |
| `drivers/vl53l1x/VL53L1X.c` | `Velocity-Control/components/drivers/vl53l1x/VL53L1X.c` |
| `drivers/vl53l1x/VL53L1X.h` | `Velocity-Control/components/drivers/vl53l1x/VL53L1X.h` |
| `drivers/vl53l1x/VL53L1X_types.h` | `Velocity-Control/components/drivers/vl53l1x/VL53L1X_types.h` |
| `platform/platform_esp32s3.c` | `Velocity-Control/components/platform/platform_esp32s3.c` |
| `platform/platform_esp32s3.h` | `Velocity-Control/components/platform/platform_esp32s3.h` |
