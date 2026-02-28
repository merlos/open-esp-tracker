# ESP32 build environment
#
# Self-contained Docker image for compiling the open-esp-tracker Arduino sketch.
# Has arduino-cli, the ESP32 board package and all required libraries pre-installed
# so no downloads happen at compile time.
#
# Build the image:
#   docker build -t open-esp-tracker-esp:build esp-client/
#
# Compile the sketch (from repo root):
#   docker run --rm -v $(pwd)/esp-client:/sketch open-esp-tracker-esp:build
#
# Flash over USB (Linux – replace /dev/ttyUSB0 with your port):
#   docker run --rm \
#     -v $(pwd)/esp-client:/sketch \
#     --device /dev/ttyUSB0 \
#     open-esp-tracker-esp:build \
#     arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 .

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Minimal set of runtime dependencies
RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      curl \
      git \
      python3 \
    && rm -rf /var/lib/apt/lists/*

# ── Arduino CLI ───────────────────────────────────────────────────────────────
# Pin to a specific version for reproducible builds.
ARG ARDUINO_CLI_VERSION=1.1.1
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
      | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}

# ── Board platform and libraries (pre-installed in the image) ─────────────────
# Downloading these once at build time means compile jobs start instantly.
RUN arduino-cli config init \
    && arduino-cli config add board_manager.additional_urls \
         https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json \
    && arduino-cli core update-index \
    && arduino-cli core install esp32:esp32 \
    && arduino-cli lib install "TinyGSM" "ArduinoJson" "Adafruit NeoPixel"

WORKDIR /sketch

# Default: compile whatever sketch is mounted at /sketch.
CMD ["arduino-cli", "compile", "--fqbn", "esp32:esp32:esp32s3", "."]
