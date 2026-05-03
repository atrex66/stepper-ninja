#!/usr/bin/env bash
set -euo pipefail

rm -rf build-cmake
rm -rf build-cmake-test

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-${script_dir}/build-cmake}"

cmake -S "$script_dir" -B "$build_dir" "$@"
cmake --build "$build_dir" --target stepgen-ninja stepper-ninja
sudo cmake --install "$build_dir" --component stepgen-ninja
sudo cmake --install "$build_dir" --component stepper-ninja
