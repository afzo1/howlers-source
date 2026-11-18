#!/usr/bin/env bash
# Mide cuanta memoria ocupa la base de software de Striker v3 antes de escribir el
# firmware (Entrega 1, seccion 4.4):
#   1. El ejemplo "station" de WiFi de ESP-IDF compilado con sdkconfig_v3.defaults
#      (WiFi + LwIP + PSRAM octal + ejecucion desde PSRAM + ULP reservado).
#   2. El mismo ejemplo sin ejecucion desde PSRAM, para ver que cambia.
#   3. El ejemplo de ADC del ULP-RISC-V, para dimensionar la memoria RTC.
# Todo se compila en una carpeta temporal nueva; no se toca el repositorio.
#
# Uso:  . ~/esp/esp-idf/export.sh
#       docs/striker_v3/herramientas/medir_memoria_base.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
: "${IDF_PATH:?Primero carga ESP-IDF v5.5.1:  . ~/esp/esp-idf/export.sh}"
WORK="$(mktemp -d /tmp/striker_v3_memoria.XXXXXX)"
echo "Carpeta de trabajo: $WORK"

compilar_station() {  # $1 = nombre de la variante, $2 = 1 para quitar la ejecucion desde PSRAM
  local dir="$WORK/$1"
  cp -r "$IDF_PATH/examples/wifi/getting_started/station" "$dir"
  cp "$HERE/particiones_v3.csv" "$dir/"
  # El ejemplo compila solo los componentes que usa (MINIMAL_BUILD); sin estos, las
  # opciones de PSRAM, ULP y drivers del archivo de configuracion se ignorarian.
  sed -i 's/PRIV_REQUIRES esp_wifi nvs_flash/PRIV_REQUIRES esp_wifi nvs_flash esp_psram ulp esp_driver_gptimer esp_driver_mcpwm/' \
    "$dir/main/CMakeLists.txt"
  grep -q "esp_psram ulp" "$dir/main/CMakeLists.txt" || { echo "No se pudo ajustar main/CMakeLists.txt del ejemplo"; exit 1; }
  if [ "${2:-0}" = 1 ]; then
    grep -v "SPIRAM_XIP_FROM_PSRAM" "$HERE/sdkconfig_v3.defaults" > "$dir/sdkconfig.defaults"
  else
    cp "$HERE/sdkconfig_v3.defaults" "$dir/sdkconfig.defaults"
  fi
  echo
  echo "================ $1 ================"
  (cd "$dir" && idf.py set-target esp32s3 > build.log 2>&1 && idf.py build >> build.log 2>&1 \
      && idf.py size 2>/dev/null | sed -n '/Memory Type Usage Summary/,$p') \
    || { echo "Fallo la compilacion; ver $dir/build.log"; exit 1; }
}

compilar_station con_xip_psram 0
compilar_station sin_xip_psram 1

echo
echo "================ ULP-RISC-V: ejemplo de ADC ================"
dir="$WORK/ulp_adc"
cp -r "$IDF_PATH/examples/system/ulp/ulp_riscv/adc" "$dir"
(cd "$dir" && idf.py set-target esp32s3 > build.log 2>&1 && idf.py build >> build.log 2>&1) \
  || { echo "Fallo la compilacion; ver $dir/build.log"; exit 1; }
bin="$(find "$dir/build" -name 'ulp_main.bin' | head -1)"
echo "Programa ULP (codigo + datos): $(stat -c %s "$bin") bytes de $(grep -o 'CONFIG_ULP_COPROC_RESERVE_MEM=[0-9]*' "$dir/sdkconfig" | cut -d= -f2) reservados"
