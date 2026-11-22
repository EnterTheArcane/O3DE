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
    libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev libxrender-dev
)

apt_options=(-y)
if [[ ${1:-native} == cross ]]; then
    # Multiarch libraries need matching versions across the runner's two mirrors.
    if [[ ${GITHUB_ACTIONS:-} == true ]]; then
        apt_options+=(--allow-downgrades --no-remove)
    fi
    sudo dpkg --add-architecture arm64
    ubuntu_sources=/etc/apt/sources.list.d/ubuntu.sources
    sources=$(sed '/^Architectures:/d; /^Types: deb$/a Architectures: amd64' "$ubuntu_sources")
    printf '%s\n' "$sources" | sudo tee "$ubuntu_sources" >/dev/null
    suite=$(. /etc/os-release && printf '%s' "$VERSION_CODENAME")
    sudo tee /etc/apt/sources.list.d/o3de-arm64.sources >/dev/null <<EOF
Types: deb
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: $suite $suite-updates $suite-security
Components: main universe
Architectures: arm64
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
fi

sudo apt-get update
sudo apt-get install "${apt_options[@]}" gcc-13 g++-13 wayland-protocols 7zip "${qt_packages[@]}"
if [[ ${1:-native} == cross ]]; then
    sudo apt-get install "${apt_options[@]}" clang g++-13-aarch64-linux-gnu "${qt_packages[@]/%/:arm64}"
fi
