#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="wensousou-builder-buster-arm64"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Run this script on an Apple Silicon Mac." >&2
  exit 1
fi
if [[ "$(uname -m)" != "arm64" ]]; then
  echo "This script requires an Apple Silicon Mac." >&2
  exit 1
fi
if ! command -v docker >/dev/null 2>&1; then
  echo "Docker Desktop is required." >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "Docker Desktop is not running. Start it, then rerun this script." >&2
  exit 1
fi

docker build --platform linux/arm64 \
  -t "${IMAGE}" \
  -f "${ROOT_DIR}/packaging/docker/Dockerfile.buster-arm64" \
  "${ROOT_DIR}/packaging/docker"

mkdir -p "${ROOT_DIR}/.docker-cache"
docker run --rm --platform linux/arm64 \
  -v "${ROOT_DIR}:/workspace" \
  -v "${ROOT_DIR}/.docker-cache:/docker-cache" \
  -e QT_PREFIX=/docker-cache/wensousou-qt-5.15.2 \
  -e WENSOUSOU_QT_WORK_ROOT=/docker-cache/wensousou-qt-build \
  "${IMAGE}" \
  bash -lc "cd /workspace && ./scripts/build-offline-local.sh"

echo "Mac Docker build completed:"
echo "  ${ROOT_DIR}/dist/offline-kit/wensousou_1.1.5_arm64.deb"
