# Striker v3: firmware unificado para Striker 1 y Striker 2 (ESP32-S3).
# Capas (docs/striker_v3/02_arquitectura_capas.md): striker_core (contrato),
# striker_hal (perifericos), striker_domain (calculo sin ESP-IDF) y esta app.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components/striker_core"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/striker_hal"
    "${CMAKE_CURRENT_LIST_DIR}/../../components/striker_domain"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(striker_v3)
