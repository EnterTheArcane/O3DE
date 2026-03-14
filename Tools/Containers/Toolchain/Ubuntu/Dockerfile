# syntax=docker/dockerfile:1
# check=error=true

#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#

# == BASE == #
ARG BASE_IMAGE=docker.io/library/ubuntu:24.04
FROM ${BASE_IMAGE} AS base

SHELL ["/bin/bash", "-euxo", "pipefail", "-c"]
WORKDIR /work

ARG DEBIAN_FRONTEND=noninteractive

RUN <<EOSH
apt-get update
apt-get install -y --no-install-recommends \
    libncurses6 libssl3t64 libreadline8t64 zlib1g \
    ca-certificates curl git gpg libxml2 make openjdk-21-jdk sudo tar unzip wget xz-utils
rm -rf /var/lib/apt/lists/*
EOSH

# == BUILDER == #
FROM base AS builder

RUN <<EOSH
curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /usr/share/keyrings/kitware.gpg
echo 'deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ noble main' > /etc/apt/sources.list.d/kitware.list
apt-get update
apt-get install -y --no-install-recommends \
    libbz2-dev libclang-rt-dev libffi-dev liblzma-dev libncurses-dev libreadline-dev libsqlite3-dev libssl-dev libxml2-dev zlib1g-dev \
    clang cmake lld llvm make ninja-build python3
rm -rf /var/lib/apt/lists/*
EOSH

ENV CC=clang
ENV CXX=clang++

ARG CMAKE_BUILD_TYPE=Release
ENV CMAKE_INSTALL_PREFIX=/usr/local

# == ANDROID == #
FROM builder AS android

ENV ANDROID_SDK_ROOT=/usr/local/android-sdk

ADD https://dl.google.com/android/repository/commandlinetools-linux-14742923_latest.zip /tmp/commandlinetools.zip
RUN <<EOSH
unzip -q /tmp/commandlinetools.zip -d /tmp
mkdir -p ${ANDROID_SDK_ROOT}/cmdline-tools/latest
mv /tmp/cmdline-tools/* ${ANDROID_SDK_ROOT}/cmdline-tools/latest
rm /tmp/commandlinetools.zip
EOSH

ENV PATH="${PATH}:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin"

ENV ANDROID_NDK_VERSION="29.0.14206865"
ENV ANDROID_PLATFORM_VERSION="36"

RUN <<EOSH
(yes || true) | sdkmanager --licenses > /dev/null
sdkmanager --update
sdkmanager --install "ndk;${ANDROID_NDK_VERSION}" "platform-tools" "platforms;android-${ANDROID_PLATFORM_VERSION}"
EOSH

# == BINARYEN == #
FROM builder AS binaryen

ARG BINARYEN_VERSION="128"

ADD --unpack=true https://github.com/WebAssembly/binaryen/archive/refs/tags/version_${BINARYEN_VERSION}.tar.gz /tmp
RUN mv /tmp/binaryen-version_${BINARYEN_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build -DBUILD_TESTS=OFF
cmake --build build --target install
EOSH

# == CMAKE == #
FROM builder AS cmake

ARG CMAKE_VERSION="4.2.3"

ADD --unpack=true https://github.com/Kitware/CMake/archive/refs/tags/v${CMAKE_VERSION}.tar.gz /tmp
RUN mv /tmp/CMake-${CMAKE_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build
cmake --build build --target install
EOSH

# == EMSCRIPTEN == #
FROM builder AS emscripten

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

# == GCC == #
FROM builder AS gcc

ARG GCC_VERSION="15.2.0"

RUN <<EOSH
apt-get update
apt-get install -y --no-install-recommends \
    diffutils libgmp-dev libmpfr-dev libmpc-dev libisl-dev flex bison texinfo perl
rm -rf /var/lib/apt/lists/*
EOSH

ADD --unpack=true https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz /tmp
RUN mv /tmp/gcc-${GCC_VERSION} /work/src

RUN <<EOSH
mkdir -p build && cd build
../src/configure --prefix=/usr/local --enable-languages=c,c++ --disable-multilib --disable-bootstrap --with-system-zlib
make -j$(nproc)
make install
EOSH

# == LLVM == #
FROM builder AS llvm

ARG LLVM_VERSION="22.1.1"

ADD --unpack=true https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-${LLVM_VERSION}.tar.gz /tmp
RUN mv /tmp/llvm-project-llvmorg-${LLVM_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src/llvm -B build \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" \
    -DLLVM_ENABLE_RUNTIMES="compiler-rt;libcxx;libcxxabi;libunwind;openmp" \
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64;WebAssembly" \
    -DLLVM_INSTALL_UTILS=ON
cmake --build build --target install
EOSH

# == NINJA == #
FROM builder AS ninja

ARG NINJA_VERSION="1.13.2"

ADD --unpack=true https://github.com/ninja-build/ninja/archive/refs/tags/v${NINJA_VERSION}.tar.gz /tmp
RUN mv /tmp/ninja-${NINJA_VERSION} /work/src

RUN <<EOSH
cmake -G Ninja -S src -B build -DBUILD_TESTING=OFF
cmake --build build --target install
EOSH

# == NODEJS == #
FROM builder AS nodejs

ARG NODEJS_VERSION="24.14.0"

ADD --unpack=true https://github.com/nodejs/node/archive/refs/tags/v${NODEJS_VERSION}.tar.gz /tmp
RUN mv /tmp/node-${NODEJS_VERSION} /work/src

RUN <<EOSH
cd src
./configure --prefix=/usr/local --ninja
make -j$(nproc)
make install
EOSH

# == PYTHON == #
FROM builder AS python

ARG PYTHON_VERSION="3.14.3"

ADD --unpack=true https://github.com/python/cpython/archive/refs/tags/v${PYTHON_VERSION}.tar.gz /tmp
RUN mv /tmp/cpython-${PYTHON_VERSION} /work/src

RUN <<EOSH
./src/configure --prefix=/usr/local --enable-optimizations --with-ensurepip=install
make -j$(nproc)
make install
EOSH

# == TOOLCHAIN == #
FROM base AS toolchain

COPY --link --from=binaryen /usr/local /usr/local
COPY --link --from=cmake /usr/local /usr/local
COPY --link --from=emscripten /usr/local /usr/local
COPY --link --from=gcc /usr/local /usr/local
COPY --link --from=llvm /usr/local /usr/local
COPY --link --from=ninja /usr/local /usr/local
COPY --link --from=nodejs /usr/local /usr/local
COPY --link --from=python /usr/local /usr/local

RUN <<EOSH
echo "/usr/local/lib" > /etc/ld.so.conf.d/local.conf
echo "/usr/local/lib64" >> /etc/ld.so.conf.d/local.conf
ldconfig
EOSH


