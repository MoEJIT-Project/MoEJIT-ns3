FROM ubuntu:24.04

# Set non-interactive mode for `apt-get`
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies for NS-3 and development environment
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-dev \
    pkg-config \
    libsqlite3-dev \
    libxml2-dev \
    libgsl-dev \
    libopenmpi-dev \
    libboost-all-dev \
    gdb \
    wget \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# Set up working directory inside the container
WORKDIR /ns-3

# Copy the NS-3 source code from the host into the container
# .dockerignore handles excluding build/ artifacts
COPY . /ns-3/

# Ensure scripts have execute permissions
RUN chmod +x scratch/net-jit/*.sh

# Optional: Run a build test to ensure setup is correct
# Running 'configure' first (assuming similar to ./ns3 configure if waf or cmake)
# Assuming CMake build based on previous interactions
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc) test-moe-parser-basic || true

# Set LD_LIBRARY_PATH so that the compiled NS-3 libraries are found
ENV LD_LIBRARY_PATH=/ns-3/build/lib:$LD_LIBRARY_PATH

# Default command to start a bash shell
CMD ["/bin/bash"]
