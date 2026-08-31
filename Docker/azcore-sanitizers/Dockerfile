# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04
FROM ubuntu:${UBUNTU_VERSION}

ARG LLVM_VERSION=23
ARG LLVM_RELEASE=23.1.0
ARG LUA_RELEASE=5.4.4
ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Keep this package set aligned with O3DE's Ubuntu Noble build-node dependencies, but omit
# editor, GPU, Java, Android, and ROS packages that are not needed by the AzCore test module.
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        file \
        git \
        git-lfs \
        gnupg \
        libcurl4-openssl-dev \
        libegl1 \
        libfontconfig1-dev \
        libglu1-mesa-dev \
        libpcre2-16-0 \
        libssl-dev \
        libunwind-dev \
        libwayland-dev \
        libxcb-cursor0 \
        libxcb-image0-dev \
        libxcb-keysyms1-dev \
        libxcb-randr0-dev \
        libxcb-render-util0-dev \
        libxcb-xfixes0-dev \
        libxcb-xinerama0 \
        libxcb-xinput-dev \
        libxcb-xinput0 \
        libxcb-xkb-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libzstd-dev \
        mesa-common-dev \
        ninja-build \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        software-properties-common \
        tar \
        unzip \
        wayland-protocols \
        xz-utils \
        zip \
        zlib1g-dev \
    && install -d -m 0755 /usr/share/keyrings \
    && curl --fail --silent --show-error --location https://apt.llvm.org/llvm-snapshot.gpg.key \
        | gpg --dearmor --output /usr/share/keyrings/llvm-archive-keyring.gpg \
    && source /etc/os-release \
    && echo "deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] https://apt.llvm.org/${VERSION_CODENAME}/ llvm-toolchain-${VERSION_CODENAME}-${LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm-${LLVM_VERSION}.list \
    && apt-get update \
    && apt-get install --yes --no-install-recommends \
        clang-${LLVM_VERSION} \
        clang-tools-${LLVM_VERSION} \
        libc++-${LLVM_VERSION}-dev \
        libc++abi-${LLVM_VERSION}-dev \
        libclang-rt-${LLVM_VERSION}-dev \
        lld-${LLVM_VERSION} \
        llvm-${LLVM_VERSION} \
        llvm-${LLVM_VERSION}-runtime \
    && rm -rf /var/lib/apt/lists/*

# MSan cannot reliably follow initialization through an uninstrumented C++
# standard library. Build the matching libc++/libc++abi release with LLVM's
# upstream MemoryWithOrigins cache and install it outside the system toolchain.
RUN git clone --depth 1 --branch llvmorg-${LLVM_RELEASE} \
        https://github.com/llvm/llvm-project.git /tmp/llvm-project \
    && cmake -S /tmp/llvm-project/runtimes -B /tmp/llvm-runtimes-build -G Ninja \
        -C /tmp/llvm-project/libcxx/cmake/caches/Generic-msan.cmake \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang-${LLVM_VERSION} \
        -DCMAKE_CXX_COMPILER=clang++-${LLVM_VERSION} \
        -DCMAKE_INSTALL_PREFIX=/opt/msan \
        -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
        -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
        -DLIBCXX_CXX_ABI=libcxxabi \
        -DLIBCXX_ENABLE_STATIC=OFF \
        -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
        -DLIBCXX_INCLUDE_TESTS=OFF \
        -DLIBCXXABI_ENABLE_STATIC=OFF \
        -DLIBCXXABI_INCLUDE_TESTS=OFF \
        -DLIBUNWIND_INCLUDE_TESTS=OFF \
    && cmake --build /tmp/llvm-runtimes-build \
        --target install-cxx install-cxxabi install-unwind --parallel "$(nproc)" \
    && rm -rf /tmp/llvm-project /tmp/llvm-runtimes-build

RUN install -d -m 0755 /opt/msan/lib/pkgconfig
COPY libunwind.pc /opt/msan/lib/pkgconfig/libunwind.pc

# AzCore links Lua statically, so the prebuilt O3DE package cannot communicate
# initialized memory to MSan. Reproduce O3DE's Lua recipe with the same upstream
# release and project patch, then instrument its single onelua.c translation unit.
COPY lua.CMakeLists.txt /tmp/lua.CMakeLists.txt
COPY lua_5.4.4_o3de.patch /tmp/lua_5.4.4_o3de.patch
RUN git clone --depth 1 --branch v${LUA_RELEASE} https://github.com/lua/lua.git /tmp/lua \
    && git -C /tmp/lua apply /tmp/lua_5.4.4_o3de.patch \
    && cp /tmp/lua.CMakeLists.txt /tmp/lua/CMakeLists.txt \
    && cmake -S /tmp/lua -B /tmp/lua-build -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang-${LLVM_VERSION} \
        -DCMAKE_C_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer -fPIC -DLUA_USE_LINUX" \
        -DCMAKE_INSTALL_PREFIX=/opt/msan-lua \
    && cmake --build /tmp/lua-build --target install --parallel "$(nproc)" \
    && rm -rf /tmp/lua /tmp/lua-build /tmp/lua.CMakeLists.txt /tmp/lua_5.4.4_o3de.patch

ENV LLVM_VERSION=${LLVM_VERSION}
ENV PATH=/usr/lib/llvm-${LLVM_VERSION}/bin:${PATH}
ENV CC=clang-${LLVM_VERSION}
ENV CXX=clang++-${LLVM_VERSION}
ENV MSAN_SYMBOLIZER_PATH=/usr/lib/llvm-${LLVM_VERSION}/bin/llvm-symbolizer
ENV MSAN_LIBCXX_ROOT=/opt/msan
ENV MSAN_LUA_ROOT=/opt/msan-lua

WORKDIR /work

COPY --chmod=0755 run-azcore-sanitizer.sh /usr/local/bin/run-azcore-sanitizer

ENTRYPOINT ["/usr/local/bin/run-azcore-sanitizer"]
