# Containerfile
FROM ubuntu:22.04 AS base

# Set non-interactive mode for apt
ENV DEBIAN_FRONTEND=noninteractive

# Install required dependencies for Yocto
RUN apt-get update && apt-get install -y \
    file \
    wget \
    bash \
    locales \
    sudo \
    git \
    python3 \
    xz-utils \
    unzip \
    dos2unix \
    uncrustify \
    clang-format-16 \
    cppcheck \
    cpplint \
    build-essential \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Development stage - includes Yocto SDK
FROM base AS development

# Download and extract the SDK installer
RUN wget https://github.com/Sammich10/pitrac-yocto/releases/download/v0.2.1-alpha/pitrac-sdk.tar.gz \
    && tar -xzf pitrac-sdk.tar.gz -C /tmp/ \
    && rm pitrac-sdk.tar.gz

# Install the SDK toolchain
RUN chmod +x /tmp/*.sh && /tmp/pitrac-glibc-x86_64-pitrac-image-base-cortexa76-raspberrypi5-toolchain-5.0.12.sh -y -d /tmp/pitrac/5.0.12/

# Source the environment setup script for the installed toolchain
RUN . /tmp/pitrac/5.0.12/environment-setup-cortexa76-pitrac-linux

# Set up SDK environment
ENV PITRAC_SDK_PATH=/tmp/pitrac/5.0.12/environment-setup-cortexa76-pitrac-linux

# Set the locale
RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

# Set the working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]