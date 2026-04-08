#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-${script_dir}/build-cmake}"

# Remove stale cache if it was generated from a different source directory
cache_file="${build_dir}/CMakeCache.txt"
if [[ -f "$cache_file" ]]; then
    cached_src=$(grep -m1 '^CMAKE_HOME_DIRECTORY:INTERNAL=' "$cache_file" | cut -d= -f2-)
    if [[ -n "$cached_src" && "$cached_src" != "$script_dir" ]]; then
        echo "Removing stale build dir (was: $cached_src, now: $script_dir)"
        rm -rf "$build_dir"
    fi
fi

cmake -S "$script_dir" -B "$build_dir" "$@"
cmake --build "$build_dir" --target stepgen-ninja stepper-ninja
sudo cmake --install "$build_dir" --component stepgen-ninja
sudo cmake --install "$build_dir" --component stepper-ninja
