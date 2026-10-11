#!/bin/sh
set -eu
cd "$(dirname "$0")"
exec "${CC:-gcc}" -Wall -Wextra -O3 -I ../firmware/inc -o benchmark_udp benchmark.c
