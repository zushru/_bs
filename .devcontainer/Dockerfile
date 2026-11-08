FROM mcr.microsoft.com/devcontainers/base:debian-12

ARG PLATFORMIO_VERSION=6.2.0

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake git curl wget unzip xz-utils pkg-config \
       python3 python3-dev python3-venv python3-pip libusb-1.0-0 \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/platformio \
    && /opt/platformio/bin/pip install --no-cache-dir "platformio==${PLATFORMIO_VERSION}" \
    && ln -s /opt/platformio/bin/pio /usr/local/bin/pio \
    && ln -s /opt/platformio/bin/platformio /usr/local/bin/platformio
