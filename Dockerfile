FROM ubuntu:24.04

# Install all necessary tooling and dependencies for building the custom C++17 client
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    x11-xserver-utils \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Compile project
RUN mkdir build && cd build && cmake .. && make

# Entrypoint runs the resulting MWB Linux Bridge binary.
# Note: For this image to function, you must run it with access to the host input subsystem:
# docker run --device /dev/uinput:/dev/uinput ...
# Pass WINDOWS_IP at runtime and enter the security key at the hidden prompt:
#   docker run --device /dev/uinput:/dev/uinput mwb-linux-bridge <WINDOWS_IP>
ENTRYPOINT ["/app/build/mwb_client"]
