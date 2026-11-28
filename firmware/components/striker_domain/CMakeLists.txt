# striker_domain: calculo de Striker v3 en C++ sin ESP-IDF (cinematica de 3
# ruedas, PID, EKF, trayectorias y seguidor de ruta). Al no depender de ESP-IDF,
# tambien se compila y prueba en la PC.
# Ver docs/striker_v3/02_arquitectura_capas.md. Por ahora solo trae su encabezado.
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES striker_core
)
