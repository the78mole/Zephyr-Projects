#!/usr/bin/env bash
# Devcontainer-Feature: Zephyr SDK Installation
#
# Läuft als root während "docker build". Der Docker-Layer-Cache sorgt dafür,
# dass bei unveränderter Feature-Version/-Konfiguration kein erneuter Download
# stattfindet.
#
# Optionen werden vom devcontainer-CLI als Umgebungsvariablen übergeben
# (Option-ID in Großbuchstaben):
#   sdkVersion  → SDKVERSION
#   toolchains  → TOOLCHAINS
set -euo pipefail

ZEPHYR_SDK_VERSION="${SDKVERSION:-0.16.8}"
TOOLCHAINS_CSV="${TOOLCHAINS:-xtensa-espressif_esp32_zephyr-elf,xtensa-espressif_esp32s2_zephyr-elf,xtensa-espressif_esp32s3_zephyr-elf,riscv64-zephyr-elf,arm-zephyr-eabi}"

SDK_INSTALL_DIR="/opt"
SDK_DIR="${SDK_INSTALL_DIR}/zephyr-sdk-${ZEPHYR_SDK_VERSION}"

if [ -d "${SDK_DIR}" ]; then
    echo "[zephyr-sdk] SDK ${ZEPHYR_SDK_VERSION} bereits vorhanden in ${SDK_DIR} – überspringe."
    exit 0
fi

ARCHIVE_NAME="zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz"
DOWNLOAD_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${ARCHIVE_NAME}"
TMP_ARCHIVE="/tmp/${ARCHIVE_NAME}"

echo "[zephyr-sdk] Lade Zephyr SDK ${ZEPHYR_SDK_VERSION} herunter ..."
wget -q --show-progress "${DOWNLOAD_URL}" -O "${TMP_ARCHIVE}"

echo "[zephyr-sdk] Entpacke nach ${SDK_INSTALL_DIR} ..."
tar -xf "${TMP_ARCHIVE}" -C "${SDK_INSTALL_DIR}"
rm -f "${TMP_ARCHIVE}"

# Toolchain-Flags aus CSV-Liste aufbauen
TOOLCHAIN_FLAGS=""
IFS=',' read -ra TC_ARRAY <<< "${TOOLCHAINS_CSV}"
for tc in "${TC_ARRAY[@]}"; do
    tc="$(echo "${tc}" | xargs)"   # führende/abschließende Leerzeichen entfernen
    TOOLCHAIN_FLAGS="${TOOLCHAIN_FLAGS} -t ${tc}"
done

echo "[zephyr-sdk] Installiere Toolchains: ${TOOLCHAINS_CSV} ..."
# shellcheck disable=SC2086
"${SDK_DIR}/setup.sh" ${TOOLCHAIN_FLAGS} -h -c

echo "[zephyr-sdk] Zephyr SDK erfolgreich installiert: ${SDK_DIR}"
