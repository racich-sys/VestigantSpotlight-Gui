#!/usr/bin/env bash
set -euo pipefail
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --config Release -j 2
./build-linux/VestigantSpotlightTests ./build-linux/selftest_out
