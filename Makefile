# SPDX-License-Identifier: Apache-2.0
#
# Targets: build-NNN, flash-NNN, monitor-NNN, monitor-rst-NNN
#   NNN = Verzeichnispräfix des Beispiels (z. B. 010 für 010-blinky)
#
# Verwendung:
#   make help              – Diese Hilfe anzeigen
#   make list-ports        – Alle seriellen Ports auflisten
#   make build-010
#   make flash-010 [PORT=…]
#   make monitor-010 [PORT=…]
#   make monitor-rst-010 [PORT=…]

BOARD   ?= esp32s3_devkitc/esp32s3/procpu

# Automatische Port-Erkennung: scripts/list-serial.sh --auto liefert den
# besten verfügbaren ESP32-Port. Kann mit PORT=… überschrieben werden.
PORT    ?= $(shell bash scripts/list-serial.sh --auto 2>/dev/null || echo /dev/ttyACM0)

# Verzeichnisname anhand des Präfixes ermitteln
dir-010 := 010-blinky
dir-100 := 100-US-Ranging
dir-200 := 200-BThome-Ranging

MONITOR_CMD  = python3 scripts/monitor.py $(PORT) 115200
RESET_CMD    = python3 -m esptool --port $(PORT) run

# ----------------------------------------------------------------------------
# 010-blinky
# ----------------------------------------------------------------------------
.PHONY: build-010
build-010:
	west build -b $(BOARD) $(dir-010)

.PHONY: flash-010
flash-010:
	west flash --esp-device $(PORT)

.PHONY: monitor-010
monitor-010:
	$(MONITOR_CMD)

.PHONY: monitor-rst-010
monitor-rst-010:
	$(RESET_CMD)
	$(MONITOR_CMD)

# ----------------------------------------------------------------------------
# 100-US-Ranging
# ----------------------------------------------------------------------------
.PHONY: build-100
build-100:
	west build -b $(BOARD) $(dir-100)

.PHONY: flash-100
flash-100:
	west flash --esp-device $(PORT)

.PHONY: monitor-100
monitor-100:
	$(MONITOR_CMD)

.PHONY: monitor-rst-100
monitor-rst-100:
	$(RESET_CMD)
	$(MONITOR_CMD)

# ----------------------------------------------------------------------------
# 200-BThome-Ranging
# ----------------------------------------------------------------------------
.PHONY: build-200
build-200:
	west build -b $(BOARD) $(dir-200)

.PHONY: flash-200
flash-200:
	west flash --esp-device $(PORT)

.PHONY: monitor-200
monitor-200:
	$(MONITOR_CMD)

.PHONY: monitor-rst-200
monitor-rst-200:
	$(RESET_CMD)
	$(MONITOR_CMD)

# ----------------------------------------------------------------------------
# Hilfsziele
# ----------------------------------------------------------------------------
.PHONY: help
help:
	@echo ""
	@echo "  ESP32-Zephyr-Test – Makefile-Hilfe"
	@echo ""
	@echo "  Konfiguration (Überschreibbar mit VAR=Wert):"
	@echo "    BOARD  = $(BOARD)"
	@echo "    PORT   = $(PORT)   (auto-erkannt; überschreiben: PORT=/dev/ttyACM1)"
	@echo ""
	@echo "  Allgemein:"
	@echo "    make help           – Diese Hilfe"
	@echo "    make list-ports     – Alle seriellen Ports mit USB-Details"
	@echo "    make clean          – Build-Verzeichnis löschen"
	@echo ""
	@echo "  010-blinky  (RGB-LED fade, WS2812 GPIO48):"
	@echo "    make build-010"
	@echo "    make flash-010          [PORT=…]"
	@echo "    make monitor-010        [PORT=…]"
	@echo "    make monitor-rst-010    [PORT=…]  – Reset + Monitor"
	@echo ""
	@echo "  100-US-Ranging  (HC-SR04, TRIG=GPIO9, ECHO=GPIO8):"
	@echo "    make build-100"
	@echo "    make flash-100          [PORT=…]"
	@echo "    make monitor-100        [PORT=…]"
	@echo "    make monitor-rst-100    [PORT=…]  – Reset + Monitor"
	@echo ""
	@echo "  200-BThome-Ranging  (HC-SR04 + BThome V2 BLE, Distanz in mm):"
	@echo "    make build-200"
	@echo "    make flash-200          [PORT=…]"
	@echo "    make monitor-200        [PORT=…]"
	@echo "    make monitor-rst-200    [PORT=…]  – Reset + Monitor"
	@echo ""

.PHONY: list-ports
list-ports:
	@bash scripts/list-serial.sh

.PHONY: clean
clean:
	rm -rf build/
