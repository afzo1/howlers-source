#!/usr/bin/env bash
# Compila las apps de referencia de howlers-source sobre una COPIA del árbol firmware/
# (para no ensuciar el repositorio) y resume el resultado de cada una.
#
# Uso:   compilar_referencias.sh <ruta_al_repo> <directorio_de_salida>
# Requisito: ESP-IDF instalado. Si idf.py no está en el PATH, se usa $IDF_PATH/export.sh.
set -u

if [ $# -ne 2 ]; then
  echo "Uso: $0 <ruta_al_repo> <directorio_de_salida>" >&2
  exit 2
fi
REPO="$(cd "$1" && pwd)"
OUT="$2"

if ! command -v idf.py > /dev/null 2>&1; then
  if [ -z "${IDF_PATH:-}" ]; then
    echo "idf.py no está en el PATH y IDF_PATH no está definido." >&2
    exit 2
  fi
  # shellcheck disable=SC1091
  . "$IDF_PATH/export.sh" > /dev/null 2>&1
fi

rm -rf "$OUT/firmware"
mkdir -p "$OUT"
cp -r "$REPO/firmware" "$OUT/firmware"

APPS="striker_v1/robot-mov striker_v1/wheel-movement striker_v2/Robot-Control striker_v2/Robot-Navigation striker_v2/Velocity-Control bench_pid_autotuning"
RESUMEN="$OUT/resumen.txt"
: > "$RESUMEN"

for app in $APPS; do
  dir="$OUT/firmware/apps/$app"
  name=$(echo "$app" | tr '/' '_')
  log_cfg="$OUT/${name}_configure.log"
  log_bld="$OUT/${name}_build.log"
  ( cd "$dir" && idf.py -B build reconfigure ) > "$log_cfg" 2>&1
  cfg_rc=$?
  bld_rc="-"
  if [ $cfg_rc -eq 0 ]; then
    # -k 0: seguir compilando tras un error para listar todos los fallos
    ninja -C "$dir/build" -k 0 > "$log_bld" 2>&1
    bld_rc=$?
  fi
  errores=$(grep -h -E "error:|multiple definition|undefined reference|CMake Error" "$log_cfg" "$log_bld" 2>/dev/null | head -3 | tr '\n' ' ')
  echo "$app | configure=$cfg_rc | build=$bld_rc | $errores" >> "$RESUMEN"
done
cat "$RESUMEN"
