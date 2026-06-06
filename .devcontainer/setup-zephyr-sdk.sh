#!/usr/bin/env bash
# Einmaliges Setup des Zephyr SDK im Container.
# Wird von postCreateCommand aufgerufen und prüft, ob das SDK bereits vorhanden ist.
set -euo pipefail

# ---------------------------------------------------------------------------
# Konfiguration – bei neuer Zephyr-Version hier anpassen:
#   https://github.com/zephyrproject-rtos/sdk-ng/releases
# ---------------------------------------------------------------------------
ZEPHYR_SDK_VERSION="0.16.8"
SDK_INSTALL_DIR="/opt"
SDK_DIR="${SDK_INSTALL_DIR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}"

# Toolchains die installiert werden (ESP32-Varianten).
# Weitere auskommentierte Targets bei Bedarf aktivieren:
TOOLCHAINS=(
    "xtensa-espressif_esp32_zephyr-elf"    # ESP32 (Xtensa LX6)
    "xtensa-espressif_esp32s2_zephyr-elf"  # ESP32-S2
    "xtensa-espressif_esp32s3_zephyr-elf"  # ESP32-S3
    "riscv64-zephyr-elf"                   # ESP32-C3/C6 (RISC-V)
    "arm-zephyr-eabi"                      # ARM Cortex-M (andere Boards)
)
# ---------------------------------------------------------------------------

if [ -d "${SDK_DIR}" ]; then
    echo "[setup-zephyr-sdk] SDK ${ZEPHYR_SDK_VERSION} bereits installiert in ${SDK_DIR}"
    exit 0
fi

ARCHIVE_NAME="zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz"
DOWNLOAD_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${ARCHIVE_NAME}"
TMP_ARCHIVE="/tmp/${ARCHIVE_NAME}"

echo "[setup-zephyr-sdk] Lade Zephyr SDK ${ZEPHYR_SDK_VERSION} herunter ..."
wget -q --show-progress "${DOWNLOAD_URL}" -O "${TMP_ARCHIVE}"

echo "[setup-zephyr-sdk] Entpacke nach ${SDK_INSTALL_DIR} ..."
sudo tar -xf "${TMP_ARCHIVE}" -C "${SDK_INSTALL_DIR}"
rm -f "${TMP_ARCHIVE}"

# Toolchain(s) installieren, Host-Tools einrichten und CMake-Config registrieren
TOOLCHAIN_FLAGS=""
for tc in "${TOOLCHAINS[@]}"; do
    TOOLCHAIN_FLAGS="${TOOLCHAIN_FLAGS} -t ${tc}"
done

echo "[setup-zephyr-sdk] Installiere Toolchains: ${TOOLCHAINS[*]} ..."
# shellcheck disable=SC2086
sudo "${SDK_DIR}/setup.sh" ${TOOLCHAIN_FLAGS} -h -c

echo "[setup-zephyr-sdk] Zephyr SDK erfolgreich installiert: ${SDK_DIR}"
echo ""
echo "Nächste Schritte (einmalig im Workspace):"
echo "  west init -l ."
echo "  west update"
