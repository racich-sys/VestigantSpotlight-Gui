#!/usr/bin/env bash
set -euo pipefail

if [[ "${ENABLE_SANITIZERS:-0}" == "1" || "${ENABLE_SANITIZERS:-}" == "ON" ]]; then
  BUILD_DIR="${BUILD_DIR:-build-linux-sanitize}"
  BUILD_TYPE="Debug"
  SANITIZER_FLAG="-DENABLE_SANITIZERS=ON"
else
  BUILD_DIR="${BUILD_DIR:-build-linux}"
  BUILD_TYPE="${BUILD_TYPE:-Release}"
  SANITIZER_FLAG=""
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $SANITIZER_FLAG
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "${BUILD_JOBS:-2}"
"./$BUILD_DIR/VestigantSpotlightTests" "./$BUILD_DIR/selftest_out"
