# syntax=docker/dockerfile:1
# check=error=true

#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

# == BASE == #
ARG BASE_IMAGE=docker.io/library/alpine:3.23
FROM ${BASE_IMAGE} AS base

SHELL ["/bin/ash", "-euxo", "pipefail", "-c"]
WORKDIR /work

RUN <<EOSH
apk add --no-cache \
    bash ca-certificates curl git libffi libgcc libstdc++ libxml2 make ncurses openssl readline tar unzip wget xz zlib
EOSH

# == BUILDER == #
FROM base AS builder

RUN <<EOSH
apk add --no-cache \
    bzip2-dev libffi-dev libxml2-dev linux-headers musl-dev ncurses-dev openssl-dev readline-dev sqlite-dev xz-dev zlib-dev \
    clang cmake compiler-rt lld llvm make python3 samurai
EOSH

ENV CC=clang
ENV CXX=clang++

ARG CMAKE_BUILD_TYPE=Release
ENV CMAKE_INSTALL_PREFIX=/usr/local

# == BINARYEN == #
FROM builder AS build-binaryen

ARG BINARYEN_VERSION="128"

ADD --unpack=true https://github.com/WebAssembly/binaryen/archive/refs/tags/version_${BINARYEN_VERSION}.tar.gz /tmp
RUN mv /tmp/binaryen-version_${BINARYEN_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build -DBUILD_TESTS=OFF
cmake --build build --target install
EOSH

FROM scratch AS binaryen
COPY --link --from=build-binaryen /usr/local /usr/local

# == CMAKE == #
FROM builder AS build-cmake

ARG CMAKE_VERSION="4.2.3"

ADD --unpack=true https://github.com/Kitware/CMake/archive/refs/tags/v${CMAKE_VERSION}.tar.gz /tmp
RUN mv /tmp/CMake-${CMAKE_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build
cmake --build build --target install
EOSH

FROM scratch AS cmake
COPY --link --from=build-cmake /usr/local /usr/local

# == EMSCRIPTEN == #
FROM builder AS build-emscripten

ARG EMSCRIPTEN_VERSION="4.0.23"

ADD --unpack=true https://github.com/emscripten-core/emscripten/archive/refs/tags/${EMSCRIPTEN_VERSION}.tar.gz /tmp
RUN mv /tmp/emscripten-${EMSCRIPTEN_VERSION} /usr/local/share/emscripten

RUN <<EOSH
mkdir -p /usr/local/bin
for f in /usr/local/share/emscripten/em*.py; do
    chmod +x "$f"
    ln -sf "$f" "/usr/local/bin/$(basename "${f%.py}")"
done
EOSH

FROM scratch AS emscripten
COPY --link --from=build-emscripten /usr/local /usr/local

# == LLVM == #
FROM builder AS build-llvm

ARG LLVM_VERSION="22.1.1"

ADD --unpack=true https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-${LLVM_VERSION}.tar.gz /tmp
RUN mv /tmp/llvm-project-llvmorg-${LLVM_VERSION} /work/src

RUN <<EOSH
MUSL_TRIPLE=$(clang -dumpmachine)
cmake -G Ninja -S src/llvm -B build \
    -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt" \
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64;WebAssembly" \
    -DLLVM_INSTALL_UTILS=ON \
    -DLLVM_HOST_TRIPLE=${MUSL_TRIPLE} \
    -DLLVM_DEFAULT_TARGET_TRIPLE=${MUSL_TRIPLE}
cmake --build build --target install
EOSH

FROM scratch AS llvm
COPY --link --from=build-llvm /usr/local /usr/local

# == NINJA == #
FROM builder AS build-ninja

ARG NINJA_VERSION="1.13.2"

ADD --unpack=true https://github.com/ninja-build/ninja/archive/refs/tags/v${NINJA_VERSION}.tar.gz /tmp
RUN mv /tmp/ninja-${NINJA_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build -DBUILD_TESTING=OFF
cmake --build build --target install
EOSH

FROM scratch AS ninja
COPY --link --from=build-ninja /usr/local /usr/local

# == NODEJS == #
FROM builder AS build-node

ARG NODEJS_VERSION="24.14.0"

ADD --unpack=true https://github.com/nodejs/node/archive/refs/tags/v${NODEJS_VERSION}.tar.gz /tmp
RUN mv /tmp/node-${NODEJS_VERSION} /work/src

RUN <<EOSH
cd src
./configure --prefix=/usr/local --ninja
make -j$(nproc)
make install
EOSH

FROM scratch AS node
COPY --link --from=build-node /usr/local /usr/local

# == PYTHON == #
FROM builder AS build-python

ARG PYTHON_VERSION="3.14.3"

ADD --unpack=true https://github.com/python/cpython/archive/refs/tags/v${PYTHON_VERSION}.tar.gz /tmp
RUN mv /tmp/cpython-${PYTHON_VERSION} /work/src

RUN <<EOSH
./src/configure --prefix=/usr/local --enable-optimizations --with-ensurepip=install
make -j$(nproc)
make install
EOSH

FROM scratch AS python
COPY --link --from=build-python /usr/local /usr/local

# == TOOLCHAIN == #
FROM base AS toolchain

COPY --link --from=binaryen /usr/local /usr/local
COPY --link --from=cmake /usr/local /usr/local
COPY --link --from=emscripten /usr/local /usr/local
COPY --link --from=llvm /usr/local /usr/local
COPY --link --from=ninja /usr/local /usr/local
COPY --link --from=node /usr/local /usr/local
COPY --link --from=python /usr/local /usr/local
