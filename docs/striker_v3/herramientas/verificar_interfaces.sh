#!/usr/bin/env bash
# Verifica el contrato C++ de Striker v3 (firmware/components/striker_core).
#
#  1. Compila la prueba del contrato con el g++ del PC en C++17 y C++20, con
#     -fno-exceptions -fno-rtti y todos los avisos como errores.
#  2. La compila con el compilador del ESP32-S3 (el de ESP-IDF) con -O2 y el
#     estándar por defecto de ESP-IDF 5.5 (gnu++2b).
#  3. Revisa que el objeto del ESP32-S3 no tenga referencias a malloc, a
#     operator new, a la información de tipos (RTTI) ni al soporte de
#     excepciones.
#  4. Con --idf: compila un proyecto ESP-IDF mínimo que usa striker_core como
#     componente (idf.py build), para comprobar que se integra con el sistema
#     de compilación de ESP-IDF. Tarda un par de minutos.
#
# Uso: docs/striker_v3/herramientas/verificar_interfaces.sh [--idf]   (desde la raíz del repo)
# Requiere ESP-IDF activo para los pasos 2 a 4 (. $IDF_PATH/export.sh).
set -euo pipefail

RAIZ="$(cd "$(dirname "$0")/../../.." && pwd)"
COMP="$RAIZ/firmware/components/striker_core"
SRC="$COMP/test/verificar_contrato.cpp"
OUT="$(mktemp -d)"
FLAGS="-fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror -I$COMP/include"

for std in c++17 c++20; do
  g++ -std=$std $FLAGS -O2 -c "$SRC" -o "$OUT/host_$std.o"
  echo "PC ($std): compila sin avisos"
done

if command -v xtensa-esp32s3-elf-g++ >/dev/null 2>&1; then
  xtensa-esp32s3-elf-g++ -std=gnu++2b $FLAGS -O2 -mlongcalls -c "$SRC" -o "$OUT/esp32s3.o"
  echo "ESP32-S3 (gnu++2b, $(xtensa-esp32s3-elf-g++ -dumpversion)): compila sin avisos"
  PROHIBIDOS='malloc|calloc|realloc|_Znwj|_Znaj|_Znwm|_Znam|__cxa_throw|__cxa_allocate_exception|__gxx_personality|_ZTI|_ZTS'
  if xtensa-esp32s3-elf-nm -u "$OUT/esp32s3.o" | grep -E "$PROHIBIDOS"; then
    echo "ERROR: el objeto usa memoria dinamica, RTTI o excepciones"
    exit 1
  fi
  echo "ESP32-S3: sin malloc, sin operator new, sin RTTI y sin excepciones"
  EXTERNOS="$(xtensa-esp32s3-elf-nm -u "$OUT/esp32s3.o")"
  echo "Simbolos externos que necesita el objeto: ${EXTERNOS:-(ninguno)}"
else
  echo "AVISO: no se encontro xtensa-esp32s3-elf-g++; activa ESP-IDF para el paso 2"
  exit 2
fi

if [ "${1:-}" = "--idf" ]; then
  PROY="$OUT/proyecto"
  mkdir -p "$PROY/main"
  cat > "$PROY/CMakeLists.txt" <<CMAKE
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "$COMP")
include(\$ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(verificar_striker_core)
CMAKE
  cat > "$PROY/main/CMakeLists.txt" <<CMAKE
idf_component_register(SRCS "main.cpp" "$SRC" REQUIRES striker_core)
target_compile_options(\${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
CMAKE
  cat > "$PROY/main/main.cpp" <<'CPP'
#include "striker/striker.hpp"
extern "C" int striker_contract_ctrl_step(void);
extern "C" int striker_contract_nav_step(void);
extern "C" void app_main(void)
{
    (void)striker_contract_ctrl_step();
    (void)striker_contract_nav_step();
}
CPP
  (cd "$PROY" && idf.py set-target esp32s3 > build.log 2>&1 && idf.py build >> build.log 2>&1) \
    || { echo "ERROR: el proyecto ESP-IDF minimo no compila; ver $PROY/build.log"; exit 1; }
  echo "ESP-IDF $(cd "$IDF_PATH" && git describe --tags 2>/dev/null): striker_core compila como componente (idf.py build)"
fi
rm -rf "$OUT"
