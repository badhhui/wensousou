#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export WENSOUSOU_OFFLINE=1

echo "Building from local dependency cache only. Network downloads are disabled."
exec "${ROOT_DIR}/scripts/build-offline-kit.sh"
