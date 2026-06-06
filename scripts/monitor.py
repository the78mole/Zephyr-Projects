#!/usr/bin/env python3
# Serieller Monitor mit automatischem Reconnect (z. B. nach Deep-Sleep).
# Beenden: Ctrl+C
import sys
import signal
import time
import serial
import serial.serialutil

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

_running = True

def _sigint(*_):
    global _running
    _running = False

signal.signal(signal.SIGINT, _sigint)

def open_port():
    """Versucht den Port zu oeffnen; wartet bei Fehler und wiederholt."""
    while _running:
        try:
            s = serial.Serial(port, baud, timeout=0.1)
            return s
        except (serial.serialutil.SerialException, OSError):
            time.sleep(0.5)
    return None

print(f"--- Monitor auf {port} ({baud}) --- Beenden: Ctrl+C ---", flush=True)

s = open_port()

while _running and s is not None:
    try:
        data = s.read(256)
        if data:
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
    except (serial.serialutil.SerialException, OSError):
        sys.stderr.write("\n[Port getrennt – warte auf Reconnect...]\n")
        sys.stderr.flush()
        try:
            s.close()
        except Exception:
            pass
        time.sleep(1.0)
        s = open_port()
        if s is not None:
            sys.stderr.write("[Reconnect OK]\n")
            sys.stderr.flush()

if s is not None:
    s.close()
sys.exit(0)
