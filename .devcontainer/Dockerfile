# Development container for Open ESP Tracker
#
# Provides a single, ready-to-use environment for both sub-projects:
#   • Ruby 3.2.3 + Bundler + all server gems pre-installed
#   • Arduino CLI + ESP32 board support + all required libraries pre-installed
#   • sqlite3, git, jq, minicom and other handy development utilities
#
# Build context: repo root (set in devcontainer.json)
# This lets us COPY server/Gemfile… during the image build.

ARG RUBY_VERSION=3.2.3
FROM ruby:${RUBY_VERSION}-bullseye

# ── System packages ───────────────────────────────────────────────────────────
RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
      build-essential \
      curl \
      git \
      jq \
      libsqlite3-dev \
      minicom \
      nano \
      pkg-config \
      python3 \
      sqlite3 \
      sudo \
      tree \
      unzip \
      wget \
    && rm -rf /var/lib/apt/lists/*

# ── Non-root user (mirrors VS Code devcontainer convention) ───────────────────
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=${USER_UID}

RUN groupadd --gid ${USER_GID} ${USERNAME} && \
    useradd --uid ${USER_UID} --gid ${USER_GID} --create-home --shell /bin/bash ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} && \
    chmod 0440 /etc/sudoers.d/${USERNAME}

# ── Arduino CLI ───────────────────────────────────────────────────────────────
# Pin to a specific release for reproducible builds.
ARG ARDUINO_CLI_VERSION=1.1.1
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
      | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}

# Install ESP32 platform + required libraries as the vscode user so that
# Arduino CLI's per-user config (in $HOME/.arduino15) is owned correctly.
USER ${USERNAME}
RUN arduino-cli config init \
    && arduino-cli config add board_manager.additional_urls \
         https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json \
    && arduino-cli core update-index \
    && arduino-cli core install esp32:esp32 \
    && arduino-cli lib install "TinyGSM" "ArduinoJson" "Adafruit NeoPixel"

USER root

# ── Ruby gems for the Rails server ───────────────────────────────────────────
# Install gems at build time so the container is immediately usable.
# BUNDLE_PATH points outside /rails so a volume-mounted source tree does not
# shadow the installed gems at runtime.
ENV BUNDLE_PATH=/usr/local/bundle

COPY server/Gemfile server/Gemfile.lock /tmp/server/
RUN cd /tmp/server && bundle install && rm -rf /tmp/server

RUN chown -R ${USERNAME}:${USERNAME} ${BUNDLE_PATH}

USER ${USERNAME}
WORKDIR /workspace
