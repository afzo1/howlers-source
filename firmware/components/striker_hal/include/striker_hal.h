/*
 * striker_hal: capa de acceso al hardware de Striker v3.
 *
 * Aqui van los manejadores de los perifericos del ESP32-S3 que usan los
 * drivers: buses I2C y UART (cada uno con su mutex), MCPWM de los ESC, GPIO,
 * ADC1 compartido con el programa del ULP-RISC-V y NVS.
 * Ver docs/striker_v3/02_arquitectura_capas.md.
 */
#pragma once
