#!/usr/bin/env bash
#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set -euo pipefail

qt_packages=(
    libdbus-1-dev libegl1-mesa-dev libfontconfig1-dev libfreetype-dev libgl-dev libgtk-3-dev
    libwayland-dev libx11-xcb-dev libxcb-cursor-dev libxcb-glx0-dev
    libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-render-util0-dev
    libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev libxcb-xkb-dev
    libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev
)

apt_options=(-y)
if [[ ${1:-native} == cross ]]; then
    # Multiarch libraries need matching versions across the runner's two mirrors.
    if [[ ${GITHUB_ACTIONS:-} == true ]]; then
        apt_options+=(--allow-downgrades --no-remove)
        # libxtst-dev ships different compressed man pages for amd64 and arm64.
        apt_options+=(-o 'Dpkg::Options::=--path-exclude=/usr/share/man/*')
    fi
    sudo dpkg --add-architecture arm64
    ubuntu_sources=/etc/apt/sources.list.d/ubuntu.sources
    sources=$(sed '/^Architectures:/d; /^Types: deb$/a Architectures: amd64' "$ubuntu_sources")
    printf '%s\n' "$sources" | sudo tee "$ubuntu_sources" >/dev/null
    suite=$(. /etc/os-release && printf '%s' "$VERSION_CODENAME")
sudo tee /etc/apt/sources.list.d/cross-compile-arm64.sources >/dev/null <<EOF
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: $suite $suite-updates $suite-security
Components: main universe
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
fi

sudo apt-get update
sudo apt-get install "${apt_options[@]}" gcc-13 g++-13 wayland-protocols 7zip libxrender-dev "${qt_packages[@]}"
if [[ ${1:-native} == cross ]]; then
    # The amd64 and arm64 archives can publish libXrender updates at different times.
    native_arch=$(dpkg --print-architecture)
    xrender_version=$(dpkg-query -W -f='${Version}' "libxrender-dev:$native_arch")
    sudo apt-get install "${apt_options[@]}" clang g++-13-aarch64-linux-gnu \
        "libxrender-dev:arm64=$xrender_version" "libxrender1:arm64=$xrender_version" \
        "${qt_packages[@]/%/:arm64}"
fi
