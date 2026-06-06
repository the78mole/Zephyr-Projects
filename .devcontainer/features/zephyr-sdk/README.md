# Feature: Zephyr SDK

Lädt das [Zephyr SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases) herunter und installiert die gewünschten Toolchains **während `docker build`**. Dadurch profitiert die Installation vom Docker-Layer-Cache – bei unveränderter Konfiguration wird das SDK bei einem Rebuild **nicht erneut heruntergeladen**.

## Verwendung

In `devcontainer.json`:

```json
"features": {
  "./features/zephyr-sdk": {
    "sdkVersion": "0.16.8",
    "toolchains": "xtensa-espressif_esp32_zephyr-elf,riscv64-zephyr-elf,arm-zephyr-eabi"
  }
}
```

## Optionen

| Option        | Typ    | Standard                                                                                                                                              | Beschreibung                                                                                        |
|---------------|--------|-------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------|
| `sdkVersion`  | string | `0.16.8`                                                                                                                                              | Zephyr SDK Version. Aktuelle Releases: [sdk-ng/releases](https://github.com/zephyrproject-rtos/sdk-ng/releases) |
| `toolchains`  | string | `xtensa-espressif_esp32_zephyr-elf,`<br>`xtensa-espressif_esp32s2_zephyr-elf,`<br>`xtensa-espressif_esp32s3_zephyr-elf,`<br>`riscv64-zephyr-elf,`<br>`arm-zephyr-eabi` | Kommagetrennte Liste der zu installierenden Toolchains.                                             |

### Verfügbare Toolchains (Auswahl)

| Toolchain                                 | Zielarchitektur                  |
|-------------------------------------------|----------------------------------|
| `xtensa-espressif_esp32_zephyr-elf`       | ESP32 (Xtensa LX6)               |
| `xtensa-espressif_esp32s2_zephyr-elf`     | ESP32-S2                         |
| `xtensa-espressif_esp32s3_zephyr-elf`     | ESP32-S3                         |
| `riscv64-zephyr-elf`                      | ESP32-C3 / C6 (RISC-V)           |
| `arm-zephyr-eabi`                         | ARM Cortex-M                     |
| `x86_64-zephyr-elf`                       | x86_64                           |

Die vollständige Liste der unterstützten Targets liefert das SDK selbst:

```bash
/opt/zephyr-sdk-<version>/setup.sh --help
```

## Installationsverzeichnis

Das SDK wird nach `/opt/zephyr-sdk-<version>` installiert. CMake-Konfiguration und Host-Tools werden über den `-c`- bzw. `-h`-Flag von `setup.sh` registriert.

## SDK-Version aktualisieren

1. `sdkVersion` in `devcontainer.json` auf die neue Version setzen.
2. Den Devcontainer neu bauen (**Rebuild Container**).  
   Docker baut nur den Zephyr-SDK-Layer neu – alle anderen Layer bleiben gecacht.

## Hintergrund: Feature vs. postCreateCommand

| | `postCreateCommand` | Dieses Feature |
|---|---|---|
| Ausführungszeitpunkt | Nach Container-Start | Während `docker build` |
| Layer-Cache | Nein | Ja – kein erneuter Download bei Rebuild |
| Läuft als | `remoteUser` (vscode) | `root` |
