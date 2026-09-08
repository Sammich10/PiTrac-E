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
    python3-pip \
    python3-jinja2 \
    xz-utils \
    unzip \
    dos2unix \
    uncrustify \
    cppcheck \
    graphviz \
    curl \
    build-essential \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install Python-based code analysis tools
RUN pip3 install --no-cache-dir lizard

# Development stage - includes Yocto SDK
FROM base AS development

# Redeclare ARG variables for this stage
ARG SDK_VERSION=v0.3-alpha
ARG GITHUB_REPO=Sammich10/pitrac-yocto

# Download and extract the SDK installer
# RUN wget https://github.com/Sammich10/pitrac-yocto/releases/download/v0.2.1-alpha/pitrac-sdk.tar.gz \
#     && tar -xzf pitrac-sdk.tar.gz -C /tmp/ \
#     && rm pitrac-sdk.tar.gz

# Download the SDK toolchain installer parts and reassemble them
# Download and reassemble SDK from GitHub releases
RUN echo "Downloading SDK ${SDK_VERSION}..." && \
    # Check if the release exists first
    curl -f -s -I "https://github.com/${GITHUB_REPO}/releases/tag/${SDK_VERSION}" > /dev/null || \
        (echo "Release ${SDK_VERSION} not found at https://github.com/${GITHUB_REPO}" && exit 1) && \
    # Download parts with better error handling
    for part in aa ab; do \
        echo "Downloading sdk-part-${part}..."; \
        URL="https://github.com/${GITHUB_REPO}/releases/download/${SDK_VERSION}/sdk-part-${part}"; \
        echo "URL: ${URL}"; \
        # Download with retries and better error checking
        curl -L -f --retry 3 --retry-delay 2 -o sdk-part-${part} "${URL}" || \
            (echo "Failed to download sdk-part-${part} from ${URL}" && \
             echo "HTTP response:" && \
             curl -L -w "%{http_code}" "${URL}" && exit 1); \
        echo "Downloaded sdk-part-${part}, size: $(stat -c%s sdk-part-${part}) bytes"; \
        # Verify it's not an HTML error page
        if file sdk-part-${part} | grep -q "HTML\|text"; then \
            echo "ERROR: sdk-part-${part} appears to be HTML/text, not binary data"; \
            echo "Content preview:"; \
            head -20 sdk-part-${part}; \
            exit 1; \
        fi; \
    done && \
    # Reassemble the SDK
    echo "Reassembling SDK..." && \
    cat sdk-part-* > sdk.tgz && \
    echo "Reassembled SDK size: $(stat -c%s sdk.tgz) bytes" && \
    # Verify it's a valid gzip file
    if ! file sdk.tgz | grep -q "gzip"; then \
        echo "ERROR: Reassembled file is not gzip format"; \
        echo "File type: $(file sdk.tgz)"; \
        echo "First 100 bytes:"; \
        head -c 100 sdk.tgz | hexdump -C; \
        exit 1; \
    fi && \
    # Verify the tarball is valid
    echo "Verifying archive integrity..." && \
    tar -tzf sdk.tgz > /dev/null && \
    # Extract SDK
    echo "Extracting SDK..." && \
    tar -xzf sdk.tgz -C /tmp && \
    # Clean up parts and tarball
    rm -f sdk-part-* sdk.tgz && \
    echo "SDK installation complete"

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