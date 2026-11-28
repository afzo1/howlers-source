# striker_hal: capa de acceso al hardware de Striker v3 (I2C, UART, MCPWM, GPIO,
# ADC/ULP y NVS). Es duena de cada bus y de su mutex.
# Ver docs/striker_v3/02_arquitectura_capas.md. Por ahora solo trae su encabezado.
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES striker_core
)
