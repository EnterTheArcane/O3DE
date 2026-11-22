# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
# SPDX-License-Identifier: Apache-2.0 OR MIT

FROM ubuntu:26.04@sha256:d66b366dac61b575bee2aef07e9c26e8070096fbe84a3d4d3eb0da069b5d76db AS base

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git git-lfs patch python3 python3-venv unzip xz-utils zip \
        build-essential pkg-config msitools \
        clang-21=1:21.1.8-6ubuntu1 lld-21=1:21.1.8-6ubuntu1 llvm-21=1:21.1.8-6ubuntu1 \
        cmake=4.2.3-2ubuntu2 ninja-build=1.13.2-1 \
        wine=10.0~repack-12ubuntu1 wine64=10.0~repack-12ubuntu1 \
    && rm -rf /var/lib/apt/lists/*

ENV PATH=/usr/lib/llvm-21/bin:/usr/lib/x86_64-linux-gnu/wine:$PATH
ENV WINEDEBUG=-all WINEARCH=win64 WINEDLLOVERRIDES=mscoree,mshtml=

FROM base AS microsoft-manifest
ARG ACCEPT_MICROSOFT_LICENSE
RUN test "$ACCEPT_MICROSOFT_LICENSE" = yes || \
    { echo 'Build with ACCEPT_MICROSOFT_LICENSE=yes after accepting the Microsoft license'; exit 1; }

WORKDIR /opt/microsoft
RUN curl -fsSL --retry 3 -o channel.json \
        https://download.visualstudio.microsoft.com/download/pr/95995f57-8355-4ff5-8d90-a7e1669fb3b3/a788dfa3e6f50a33f7dee8396fd40d2946c69a776c0cfa5208f56d461fd1a825/VisualStudio.17.Release.chman \
    && echo '44a3e7dc44486c945abdc1fae97e7e22de4d05e80f03b00e9302ba4b72c3e0e3  channel.json' | sha256sum -c - \
    && curl -fsSL --retry 3 -o packages.json \
        https://download.visualstudio.microsoft.com/download/pr/95995f57-8355-4ff5-8d90-a7e1669fb3b3/da49c304df0fff4e75c20849171840a06840e845ce814104d709dfb5cd461668/VisualStudio.vsman \
    && echo 'd1d8109899558618035724949673f69f8c1b5704f542a2e76f31d87e425d5710  packages.json' | sha256sum -c -

FROM microsoft-manifest AS xwin
RUN curl -fsSL --retry 3 -o /tmp/xwin.tar.gz \
        https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz \
    && echo 'd870eb4b2f390878af6da1ccd3cf321d22fcb72720984853b4be732ae597fc88  /tmp/xwin.tar.gz' | sha256sum -c - \
    && tar -xzf /tmp/xwin.tar.gz -C /usr/local/bin --wildcards --strip-components=1 '*/xwin' \
    && rm /tmp/xwin.tar.gz

RUN <<'EOF'
set -eu
xwin_selected() {
    xwin --accept-license --manifest /opt/microsoft/channel.json --cache-dir /tmp/xwin \
        --crt-version 14.44.17.14 --sdk-version 10.0.26100 --include-atl "$@"
}
xwin_selected list > /opt/microsoft/xwin-packages.txt
cat /opt/microsoft/xwin-packages.txt
xwin_selected splat --output /opt/xwin --use-winsysroot-style --preserve-ms-arch-notation
find /tmp/xwin /opt/xwin \( -iname cl.exe -o -iname link.exe \) -print > /opt/microsoft/xwin-compiler-files.txt
if [ ! -s /opt/microsoft/xwin-compiler-files.txt ]; then
    echo 'xwin selected payloads contain neither cl.exe nor link.exe' | tee /opt/microsoft/xwin-compiler-files.txt
fi
cat /opt/microsoft/xwin-compiler-files.txt
rm -rf /tmp/xwin
EOF

# The prebuilt PhysX Foundation library embeds /DEFAULTLIB:Ws2_32.
RUN ln -s WS2_32.Lib '/opt/xwin/Windows Kits/10/Lib/10.0.26100/um/x64/Ws2_32.lib'

FROM microsoft-manifest AS msvc
RUN <<'EOF'
set -eu
mkdir /tmp/msvc-wine
curl -fsSL --retry 3 https://codeload.github.com/mstorsjo/msvc-wine/tar.gz/5dd115047b7ddcbb53cbc8b5b14163a20f4334fe \
    | tar -xz -C /tmp/msvc-wine --strip-components=1
export WINEPREFIX=/tmp/wine
wineboot --init
wineserver -w
PYTHONUNBUFFERED=1 /tmp/msvc-wine/vsdownload.py --accept-license --major 17 \
    --manifest /opt/microsoft/packages.json --architecture x64 --msvc-version 14.44 \
    --sdk-version 10.0.26100 --with-devcmd no --with-msbuild no --dest /opt/msvc
/tmp/msvc-wine/install.sh /opt/msvc
wineserver -w
rm -rf /tmp/msvc-wine /tmp/wine
EOF

FROM msvc AS msvc-wrapper
COPY Builder/wine-msvc.patch /tmp/wine-msvc.patch
RUN patch --batch -d /opt/msvc -p1 < /tmp/wine-msvc.patch

FROM base AS runner
RUN apt-get update && apt-get install -y --no-install-recommends libpcre2-16-0 \
    && rm -rf /var/lib/apt/lists/*

ENV LANG=C.UTF-8
COPY Builder/build.sh Builder/windows-toolchain.cmake Builder/wine-test.sh Builder/wine-pytest.sh /opt/o3de/
COPY Builder/Probe /opt/o3de/Probe
ENTRYPOINT ["bash", "/opt/o3de/build.sh"]

FROM runner AS clang-builder
COPY --link --from=xwin /opt/xwin /opt/xwin
COPY --link --from=xwin /opt/microsoft/xwin-*.txt /opt/microsoft/

FROM runner AS msvc-builder
COPY --link --from=msvc /opt/msvc /opt/msvc
COPY --from=msvc-wrapper /opt/msvc/bin/x64/wine-msvc.sh /opt/msvc/bin/x64/wine-msvc.sh

FROM clang-builder AS builder
COPY --link --from=msvc /opt/msvc /opt/msvc
COPY --from=msvc-wrapper /opt/msvc/bin/x64/wine-msvc.sh /opt/msvc/bin/x64/wine-msvc.sh

FROM builder AS wine-test-runner
RUN curl -fsSL --retry 3 https://dl.winehq.org/wine-builds/winehq.key -o /etc/apt/keyrings/winehq-archive.asc \
    && printf 'deb [arch=amd64 signed-by=/etc/apt/keyrings/winehq-archive.asc] https://dl.winehq.org/wine-builds/ubuntu/ resolute main\n' \
        > /etc/apt/sources.list.d/winehq.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        winehq-devel=11.22~resolute-1 wine-devel=11.22~resolute-1 \
        xvfb xauth libgl1-mesa-dri mesa-vulkan-drivers \
    && rm -rf /var/lib/apt/lists/*
ENV PATH=/opt/wine-devel/bin:$PATH

FROM builder AS default
