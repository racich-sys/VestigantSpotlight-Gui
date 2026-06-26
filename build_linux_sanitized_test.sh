#!/usr/bin/env bash
set -euo pipefail
export ENABLE_SANITIZERS=1
export BUILD_DIR="${BUILD_DIR:-build-linux-sanitize}"
exec "$(dirname "$0")/build_linux_test.sh"
