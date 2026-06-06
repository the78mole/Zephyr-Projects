FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

# Ubuntu 24.04 statt 26.04, da Zephyr auf 24.04 getestet und zertifiziert ist

# Zephyr Build-Abhängigkeiten (gemäß Zephyr Getting Started Guide)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler wget curl \
    python3-dev python3-pip python3-setuptools python3-wheel \
    xz-utils file make \
    gcc gcc-multilib g++-multilib \
    libsdl2-dev libmagic1 \
    picocom \
    udev \
    && rm -rf /var/lib/apt/lists/*

# west (Zephyr Meta-Build-Tool) und ESP32-Flash-Tool
RUN pip3 install --break-system-packages \
    west \
    esptool \
    pyelftools
