# Origen de estos archivos

Copia **sin cambios** de los drivers propios de `Robot-Navigation`, tomados del repositorio original
[`MaverickST/RoboCup_Striker2`](https://github.com/MaverickST/RoboCup_Striker2/tree/e182de0) en el commit `e182de0`. Se restauraron porque, al reorganizar el monorepo,
la app quedó apuntando a drivers compartidos de otra app, con otra API, y dejó de compilar.

Motivo y detalle: [`docs/striker_v3/01-1_correcciones_compilacion.md`](../../../../../docs/striker_v3/01-1_correcciones_compilacion.md), corrección N1.

Cada archivo se comparó byte a byte con el original (`cmp`). **No editarlos aquí:** son la referencia
del firmware viejo. El firmware nuevo (Striker v3) tendrá una sola versión de cada driver.

| Archivo aquí | Archivo en el repositorio original |
|---|---|
| `control/control.c` | `Robot-Navigation/components/control/control.c` |
| `control/control.h` | `Robot-Navigation/components/control/control.h` |
| `control/senfusion.c` | `Robot-Navigation/components/control/senfusion.c` |
| `control/senfusion.h` | `Robot-Navigation/components/control/senfusion.h` |
| `drivers/bno055/bno055.c` | `Robot-Navigation/components/drivers/bno055/bno055.c` |
| `drivers/bno055/bno055.h` | `Robot-Navigation/components/drivers/bno055/bno055.h` |
| `drivers/bno055/bno055_defs.h` | `Robot-Navigation/components/drivers/bno055/bno055_defs.h` |
