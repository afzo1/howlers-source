# [Correccion 2026-11] wheel-movement vuelve a usar sus propios drivers de
# plataforma, PID, fusion sensorial y la cabecera del AS5600, restaurados desde
# el repositorio original nparra-code/striker-1 (commit dc56d28) en
# ../local_drivers. Al reorganizar el monorepo se apunto a las versiones de
# robot-mov (components/*/v1_striker1), que tienen otra API (por ejemplo,
# estimate_velocity_encoder() con 1 argumento y sin el campo "estimate").
# BLDC y VL53L1X son identicos a los originales y se siguen tomando de
# components/. TM151 tambien: la version compartida (la de robot-mov) solo
# agrega dos funciones de lectura del yaw y cambia comentarios, asi que es
# compatible. Los directorios locales van primero en INCLUDE_DIRS.

file(GLOB SRCS_AS5600  "../local_drivers/sensors/AS5600_HAL_Library/*.c")
file(GLOB SRCS_PLATFORM  "../local_drivers/platform/*.c")
file(GLOB SRCS_PID_LIB  "../local_drivers/drivers/pid_lib/*.c")
file(GLOB SRCS_SENSOR_FUSION "../local_drivers/drivers/sensor_fusion/*.c")
file(GLOB SRCS_TM151  "../../../../components/sensors/tm151/v1_striker1/*.c")
file(GLOB SRCS_VL53L1X  "../../../../components/sensors/vl53l1x/v1_striker1/*.c")
file(GLOB SRCS_BLDC_PWM  "../../../../components/actuators/bldc_pwm/v1_striker1/*.c")

idf_component_register(
        SRCS "main.c" ${SRCS_AS5600} ${SRCS_TM151} ${SRCS_VL53L1X} ${SRCS_PLATFORM} ${SRCS_BLDC_PWM} ${SRCS_PID_LIB} ${SRCS_SENSOR_FUSION}

        INCLUDE_DIRS "."
                    "../local_drivers/sensors/AS5600_HAL_Library/include"
                    "../local_drivers/platform/include"
                    "../local_drivers/drivers/pid_lib/include"
                    "../local_drivers/drivers/sensor_fusion/include"
                    "../../../../components/sensors/tm151/v1_striker1/include"
                    "../../../../components/sensors/vl53l1x/v1_striker1/include"
                    "../../../../components/actuators/bldc_pwm/v1_striker1/include"
)
