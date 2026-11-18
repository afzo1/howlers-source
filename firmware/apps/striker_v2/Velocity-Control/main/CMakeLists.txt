# [Correccion 2026-11] Velocity-Control usa sus propios drivers, restaurados
# desde el repositorio original MaverickST/RoboCup_Striker2 (commit e182de0)
# en ../local_drivers. Al reorganizar el monorepo se apunto a las versiones de
# Robot-Control (platform, AS5600, BLDC, BNO055, VL53L1X y control), que tienen
# otra API, y a una carpeta actuators/led_status/v2_striker2 que no existe;
# por eso la app ni siquiera configuraba. Solo uart_console es identico al
# original y se sigue tomando de components/.

file(GLOB MAIN_SRCS "main.c" "tasks.c" "functions.c" "*.c")
file(GLOB PLATFORM_SRCS "../local_drivers/platform/*.c")
file(GLOB CONTROL_SRCS "../local_drivers/control/*.c")
file(GLOB AS5600_SRCS "../local_drivers/drivers/as5600/*.c")
file(GLOB BLDC_PWM_SRCS "../local_drivers/drivers/bldc_pwm/*.c")
file(GLOB BNO055_SRCS "../local_drivers/drivers/bno055/*.c")
file(GLOB LED_SRCS "../local_drivers/drivers/led/*.c")
file(GLOB VL53L1X_SRCS "../local_drivers/drivers/vl53l1x/*.c")
file(GLOB UART_CONSOLE_SRCS "../../../../components/comms/uart_console/v2_striker2/*.c")

idf_component_register(
    SRCS
        ${MAIN_SRCS}
        ${PLATFORM_SRCS}
        ${CONTROL_SRCS}
        ${AS5600_SRCS}
        ${BLDC_PWM_SRCS}
        ${BNO055_SRCS}
        ${LED_SRCS}
        ${VL53L1X_SRCS}
        ${UART_CONSOLE_SRCS}
    INCLUDE_DIRS
        "."
        "../include"
        "../local_drivers/platform"
        "../local_drivers/control"
        "../local_drivers/drivers/as5600"
        "../local_drivers/drivers/bldc_pwm"
        "../local_drivers/drivers/bno055"
        "../local_drivers/drivers/led"
        "../local_drivers/drivers/vl53l1x"
        "../../../../components/comms/uart_console/v2_striker2"
)
