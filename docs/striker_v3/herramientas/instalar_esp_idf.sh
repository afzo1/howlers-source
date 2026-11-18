#!/usr/bin/env bash
# Instala ESP-IDF v5.5.1 (la version con la que se compilo todo el proyecto) en Linux,
# solo con las herramientas del ESP32-S3.
#
# Uso:   docs/striker_v3/herramientas/instalar_esp_idf.sh [carpeta_destino]
#        (por defecto: ~/esp/esp-idf)
# Luego, en cada terminal nueva:   . ~/esp/esp-idf/export.sh
#
# Requisitos previos (Ubuntu/Debian):
#   sudo apt-get install git wget flex bison gperf python3 python3-pip python3-venv \
#        cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
#
# Si la red bloquea dl.espressif.com (pasa en algunas redes y entornos en la nube), el
# script instala los paquetes de Python desde PyPI sin el archivo de restricciones de
# Espressif. En ese caso, antes de export.sh hay que definir IDF_PYTHON_CHECK_CONSTRAINTS=no.
set -euo pipefail

IDF_VER="v5.5.1"
DEST="${1:-$HOME/esp/esp-idf}"

if [ -d "$DEST/.git" ]; then
  echo "Ya existe $DEST; se usa esa copia."
else
  mkdir -p "$(dirname "$DEST")"
  git clone -b "$IDF_VER" --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "$DEST"
fi

cd "$DEST"
if ./install.sh esp32s3; then
  echo
  echo "Listo. En cada terminal nueva:  . $DEST/export.sh"
  exit 0
fi

echo
echo "install.sh fallo; se reintenta sin el archivo de restricciones de Espressif..."
python3 tools/idf_tools.py install --targets=esp32s3
IDF_PIP_WHEELS_URL=https://pypi.org/simple \
  python3 tools/idf_tools.py install-python-env --no-constraints
echo
echo "Listo. En cada terminal nueva:"
echo "  export IDF_PYTHON_CHECK_CONSTRAINTS=no"
echo "  . $DEST/export.sh"
