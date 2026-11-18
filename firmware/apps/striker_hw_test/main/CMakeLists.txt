# Protocolo de la IMU TM151 (Striker 1): biblioteca del fabricante, la misma de
# robot-mov. Se omite EasyRetrieve.c porque depende de la HAL vieja.
set(TM151_DIR "../../../components/sensors/tm151/v1_striker1")

idf_component_register(
    SRCS "main.c" "robot_config.c" "esc.c" "encoders.c" "imu_tm151.c"
         "cmd_basic.c" "cmd_sensors.c" "cmd_motors.c"
         "${TM151_DIR}/EasyProfile.c" "${TM151_DIR}/EasyProtocol.c"
         "${TM151_DIR}/EasyObjectDictionary.c" "${TM151_DIR}/EasyQueue.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "${TM151_DIR}/include"
    PRIV_REQUIRES console nvs_flash esp_adc esp_driver_mcpwm esp_driver_i2c
                  esp_driver_uart esp_driver_gpio esp_wifi esp_netif esp_event
                  esp_psram spi_flash esp_timer
)
