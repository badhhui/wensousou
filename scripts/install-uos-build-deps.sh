#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "Run this script on the connected UOS build machine." >&2
  exit 1
fi
if ! command -v apt-get >/dev/null 2>&1; then
  echo "Missing package manager: apt-get" >&2
  exit 1
fi

packages=(
  build-essential
  cmake
  curl
  pkg-config
  unzip
  xz-utils
  libx11-dev
  libx11-xcb-dev
  libxcb1-dev
  libxcb-icccm4-dev
  libxcb-util0-dev
  libxcb-image0-dev
  libxcb-keysyms1-dev
  libxcb-render-util0-dev
  libxcb-randr0-dev
  libxcb-render0-dev
  libxcb-shape0-dev
  libxcb-shm0-dev
  libxcb-sync-dev
  libxcb-xfixes0-dev
  libxcb-xinerama0-dev
  libxcb-xkb-dev
  libxkbcommon-dev
  libxkbcommon-x11-dev
  libfontconfig1-dev
  libfreetype6-dev
  libdbus-1-dev
  fcitx-frontend-qt5
)

sudo apt-get update
sudo apt-get install -y "${packages[@]}"

echo "UOS build dependencies installed."
