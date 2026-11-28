idf_component_register(
    SRCS "main.c" "tareas.c" "medicion.c" "consola.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES striker_core striker_hal striker_domain
                  console esp_driver_gptimer esp_driver_mcpwm esp_driver_gpio
                  esp_timer heap
)
