#!/usr/bin/env bash
# list-serial.sh — Alle ttyACM/ttyUSB-Ports mit USB-Gerätedetails auflisten.
#
# Verwendung:
#   ./scripts/list-serial.sh              # alle seriellen Ports
#   ./scripts/list-serial.sh --flash      # nur Flash-/Bootloader-Geräte
#   ./scripts/list-serial.sh --monitor    # nur "normale" Firmware-Geräte
#   ./scripts/list-serial.sh --auto       # besten ESP32-Port ausgeben (für Makefile)

set -uo pipefail

# ── Farb-Helfer ───────────────────────────────────────────────────────────────
if [[ -t 1 ]]; then
    BOLD='\033[1m'; CYAN='\033[0;36m'; GREEN='\033[0;32m'
    YELLOW='\033[0;33m'; RED='\033[0;31m'; RESET='\033[0m'
else
    BOLD=''; CYAN=''; GREEN=''; YELLOW=''; RED=''; RESET=''
fi

# ── Bekannte VID:PID ──────────────────────────────────────────────────────────
declare -A KNOWN_DEVICES=(
    # Espressif / ESP32-S3 integrierter USB-JTAG/CDC
    ["303a:1001"]="ESP32-S3 USB-JTAG/CDC"
    ["303a:0002"]="ESP32-S2 USB-CDC"
    ["303a:4001"]="ESP32-C3 USB-JTAG/CDC"
    # WCH CH343P (häufig auf ESP32-S3-DevKitC-Clones)
    ["1a86:55d3"]="CH343P UART Bridge (ESP32-Clone)"
    ["1a86:7523"]="CH340 UART Bridge"
    ["1a86:5523"]="CH341 UART Bridge"
    # Silicon Labs CP210x (z. B. SparkFun ESP32)
    ["10c4:ea60"]="CP210x UART Bridge"
    # FTDI
    ["0403:6001"]="FTDI FT232 UART"
    ["0403:6010"]="FTDI FT2232 Dual UART"
    # Espressif ESP-Prog JTAG
    ["0403:6011"]="ESP-Prog JTAG (FTDI FT4232)"
    # Silicon Labs ESP8266/ESP32 DevKit
    ["10c4:ea70"]="CP2105 Dual UART Bridge"
)

# ── VID:PIDs die als Flash-/Programmier-Target gelten ────────────────────────
# (ROM-Bootloader über integriertes USB-CDC oder externer UART-Adapter)
FLASH_VIDS=("303a:1001" "303a:0002" "303a:4001" "1a86:55d3" "1a86:7523"
            "1a86:5523" "10c4:ea60" "0403:6001" "0403:6010" "0403:6011"
            "10c4:ea70")

# ── Filter-Flag ───────────────────────────────────────────────────────────────
FILTER=""
case "${1:-}" in
    --flash)   FILTER="flash"   ;;
    --monitor) FILTER="monitor" ;;
    --auto)    FILTER="auto"    ;;
esac

# ── Im --auto-Modus: nur den besten Port ausgeben ────────────────────────────
if [[ "$FILTER" == "auto" ]]; then
    best=""
    best_score=0

    for port in /dev/ttyACM* /dev/ttyUSB*; do
        [[ -c "$port" ]] || continue
        dev=$(basename "$port")
        if [[ "$dev" == ttyACM* ]]; then
            sysbase="/sys/class/tty/$dev/device/.."
        else
            sysbase="/sys/class/tty/$dev/device/../../.."
        fi
        [[ -d "$sysbase" ]] || sysbase="/sys/class/tty/$dev/device/.."

        vendor=$(cat  "$sysbase/idVendor"  2>/dev/null || echo "")
        product=$(cat "$sysbase/idProduct" 2>/dev/null || echo "")
        mfg=$(cat     "$sysbase/manufacturer" 2>/dev/null | tr -d '\n' || echo "")
        vidpid="${vendor}:${product}"

        # Kein echter USB-Seriell-Adapter
        [[ "$mfg" == "Linux "* ]] && continue

        score=0
        case "$vidpid" in
            # Integrierter ESP32-USB-JTAG/CDC: bevorzugt
            "303a:1001"|"303a:0002"|"303a:4001") score=3 ;;
            # CH343P auf ESP32-Clones: gut
            "1a86:55d3")                         score=2 ;;
            # Andere UART-Bridges: akzeptabel
            "1a86:7523"|"1a86:5523"|"10c4:ea60"|"0403:6001"|"0403:6010"|"0403:6011"|"10c4:ea70")
                                                  score=1 ;;
        esac

        if [[ $score -gt $best_score ]]; then
            best_score=$score
            best="$port"
        fi
    done

    if [[ -n "$best" ]]; then
        echo "$best"
    else
        # Fallback: erster verfügbarer ttyACM/ttyUSB
        for port in /dev/ttyACM* /dev/ttyUSB*; do
            [[ -c "$port" ]] && echo "$port" && exit 0
        done
        echo "/dev/ttyACM0"   # Hardcoded-Fallback
    fi
    exit 0
fi

# ── Tabellen-Header ───────────────────────────────────────────────────────────
printf "${BOLD}%-12s  %-9s  %-28s  %-24s  %-20s  %s${RESET}\n" \
    "PORT" "VID:PID" "HERSTELLER" "PRODUKT" "SERIENNR." "HINWEIS"
printf '%0.s─' {1..115}; echo

# ── Haupt-Schleife ────────────────────────────────────────────────────────────
found=0

for port in /dev/ttyACM* /dev/ttyUSB*; do
    [[ -c "$port" ]] || continue

    dev=$(basename "$port")
    if [[ "$dev" == ttyACM* ]]; then
        sysbase="/sys/class/tty/$dev/device/.."
    else
        sysbase="/sys/class/tty/$dev/device/../../.."
    fi
    [[ -d "$sysbase" ]] || sysbase="/sys/class/tty/$dev/device/.."

    vendor=$(cat  "$sysbase/idVendor"      2>/dev/null || echo "????")
    product=$(cat "$sysbase/idProduct"     2>/dev/null || echo "????")
    mfg=$(cat     "$sysbase/manufacturer"  2>/dev/null | tr -d '\n' || echo "")
    prod=$(cat    "$sysbase/product"       2>/dev/null | tr -d '\n' || echo "")
    serial=$(cat  "$sysbase/serial"        2>/dev/null | tr -d '\n' || echo "")
    vidpid="${vendor}:${product}"

    # Kein echter USB-Seriell-Adapter (z. B. Linux USB-Host-Controller)
    [[ "$mfg" == "Linux "* ]] && continue

    note="${KNOWN_DEVICES[$vidpid]:-}"

    # ── Flash-Target-Erkennung ────────────────────────────────────────────────
    is_flash=0
    for fvid in "${FLASH_VIDS[@]}"; do
        [[ "$vidpid" == "$fvid" ]] && is_flash=1 && break
    done

    # ── Filter anwenden ───────────────────────────────────────────────────────
    if [[ "$FILTER" == "flash"   && $is_flash -eq 0 ]]; then continue; fi
    if [[ "$FILTER" == "monitor" && $is_flash -eq 1 ]]; then continue; fi

    # ── Farbwahl ──────────────────────────────────────────────────────────────
    if   [[ -n "$note" && "$note" == *"ESP32"* ]]; then colour="$GREEN"
    elif [[ -n "$note" ]];                          then colour="$CYAN"
    else                                                 colour="$YELLOW"
    fi

    printf "${colour}%-12s  %-9s  %-28s  %-24s  %-20s  %s${RESET}\n" \
        "$port" "$vidpid" "${mfg:0:27}" "${prod:0:23}" "${serial:0:19}" "$note"
    found=$((found + 1))
done

echo
if [[ $found -eq 0 ]]; then
    printf "  ${RED}Keine seriellen Ports gefunden${RESET}${FILTER:+ (Filter: --$FILTER)}.\n"
else
    printf "  ${BOLD}%d Port(s) gefunden${RESET}\n" "$found"
fi

# ── Tipp: automatische Port-Erkennung ─────────────────────────────────────────
auto_port=$(bash "$(dirname "$0")/list-serial.sh" --auto 2>/dev/null || echo "")
if [[ -n "$auto_port" && "$FILTER" != "flash" ]]; then
    echo
    printf "  ${BOLD}Tipp:${RESET}  make flash-100 PORT=%s\n" "$auto_port"
fi
echo
