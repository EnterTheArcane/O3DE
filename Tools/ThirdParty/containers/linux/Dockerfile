# syntax=docker/dockerfile:1
FROM debian:13-slim

RUN set -eux; \
    dpkg --add-architecture amd64; \
    dpkg --add-architecture arm64; \
    apt-get update; \
    apt-get install -y --no-install-recommends bash binutils python3 \
    gcc-x86-64-linux-gnu g++-x86-64-linux-gnu \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    libc6-dev-amd64-cross libgcc-14-dev-amd64-cross libstdc++-14-dev-amd64-cross \
    libc6-dev-arm64-cross libgcc-14-dev-arm64-cross libstdc++-14-dev-arm64-cross; \
    rm -rf /var/lib/apt/lists/*

ENTRYPOINT ["bash"]
