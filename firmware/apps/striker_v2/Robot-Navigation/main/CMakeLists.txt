# [Correccion 2026-11] Robot-Navigation usa su propia version del control,
# la fusion sensorial (senfusion) y el BNO055, restauradas desde el repositorio
# original MaverickST/RoboCup_Striker2 (commit e182de0) en ../local_drivers.
# Al reorganizar el monorepo, esas versiones se reemplazaron por las de
# Robot-Control (control/pid_lib/v2_striker2 y sensors/bno055/v2_striker2),
# que tienen otra API, y la app dejo de compilar. El resto de drivers
# compartidos son identicos a los originales y se siguen usando.
# Los directorios locales van primero en INCLUDE_DIRS para que sus cabeceras
# tengan prioridad sobre las compartidas del mismo nombre.

file(GLOB MAIN_SRCS "main.c" "tasks.c" "functions.c" "*.c")
file(GLOB LOCAL_CONTROL_SRCS "../local_drivers/control/*.c")
file(GLOB LOCAL_BNO055_SRCS "../local_drivers/drivers/bno055/*.c")
file(GLOB PLATFORM_SRCS "../../../../components/platform_hal/v2_striker2/*.c")
file(GLOB AS5600_SRCS "../../../../components/sensors/as5600/v2_striker2/*.c")
file(GLOB BLDC_PWM_SRCS "../../../../components/actuators/bldc_pwm/v2_striker2/*.c")
file(GLOB UART_CONSOLE_SRCS "../../../../components/comms/uart_console/v2_striker2/*.c")
file(GLOB VL53L1X_SRCS "../../../../components/sensors/vl53l1x/v2_striker2/*.c")

idf_component_register(
    SRCS
        ${MAIN_SRCS}
        ${LOCAL_CONTROL_SRCS}
        ${LOCAL_BNO055_SRCS}
        ${PLATFORM_SRCS}
        ${AS5600_SRCS}
        ${BLDC_PWM_SRCS}
        ${UART_CONSOLE_SRCS}
        ${VL53L1X_SRCS}
    INCLUDE_DIRS
        "."
        "../include"
        "../local_drivers/control"
        "../local_drivers/drivers/bno055"
        "../../../../components/platform_hal/v2_striker2"
        "../../../../components/sensors/as5600/v2_striker2"
        "../../../../components/actuators/bldc_pwm/v2_striker2"
        "../../../../components/comms/uart_console/v2_striker2"
        "../../../../components/sensors/vl53l1x/v2_striker2"
)
