# Stage 1: Builder
FROM ubuntu:24.04 AS builder

# Install build dependencies + g++14
RUN apt-get update && apt-get install -y --no-install-recommends \
    software-properties-common \
    build-essential \
    wget \
    curl \
    ca-certificates \
    git \
    unzip \
    autoconf \
    libtool \
    pkg-config \
    python3 \
    zlib1g-dev \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update && apt-get install -y g++-14 gcc-14 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-14 100 \
    && rm -rf /var/lib/apt/lists/*

# Install CMake 3.28
RUN wget https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz \
    && tar -xzf cmake-3.28.3-linux-x86_64.tar.gz --strip-components=1 -C /usr/local \
    && rm cmake-3.28.3-linux-x86_64.tar.gz

# Build and install Protobuf 3.21.6 (without tests)
WORKDIR /tmp
RUN git clone -b v3.21.6 --depth 1 https://github.com/protocolbuffers/protobuf.git \
    && cd protobuf \
    && mkdir -p cmake/build && cd cmake/build \
    && cmake ../.. \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -Dprotobuf_BUILD_TESTS=OFF \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    && cd /tmp && rm -rf protobuf

# Build and install gRPC 1.51.1
RUN git clone -b v1.51.1 --depth 1 https://github.com/grpc/grpc /tmp/grpc \
    && cd /tmp/grpc \
    && git submodule update --init \
    && mkdir -p cmake/build && cd cmake/build \
    && cmake ../.. \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
    && make grpc_cpp_plugin -j$(nproc) \
    && make install -j$(nproc) \
    && cp grpc_cpp_plugin /usr/local/bin/ \
    && ldconfig \
    && cd /tmp && rm -rf /tmp/grpc

# Copy project files
WORKDIR /app
COPY CMakeLists.txt ./ 
COPY proto/ proto/
COPY src/ src/
COPY config/ config/

# Ensure generated folder exists
RUN mkdir -p /app/generated

# Build project
RUN mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc)

# Stage 2: Minimal runtime image
FROM ubuntu:24.04

WORKDIR /app

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libstdc++6 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Copy built binaries
COPY --from=builder /app/build/Server ./Server
COPY --from=builder /app/build/Client ./Client
COPY --from=builder /app/build/Bot ./Bot

# Copy configs
COPY config/ /app/config/

CMD ["./Server"]
