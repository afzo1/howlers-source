# Striker v3 - benchmark de matematicas (Entrega 2, paquete P2).
# Mide ciclos de CPU de double, float, punto fijo Q16.16 y esp-dsp en el ESP32-S3.
# No usa ningun pin: solo calcula e imprime una tabla por la consola.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(striker_bench_math)
