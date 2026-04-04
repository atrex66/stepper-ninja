#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./halrun-stepper-ninja.sh [ip:port]
# Examples:
#   ./halrun-stepper-ninja.sh
#   ./halrun-stepper-ninja.sh 192.168.0.177:8888
#
# Optional environment variables:
#   THREAD_NAME      (default: servo-thread)
#   THREAD_PERIOD_NS (default: 1000000)

IP_ADDRESS="${1:-${IP_ADDRESS:-192.168.0.177:8888}}"
THREAD_NAME="${THREAD_NAME:-servo-thread}"
THREAD_PERIOD_NS="${THREAD_PERIOD_NS:-1000000}"

if ! command -v halrun >/dev/null 2>&1; then
    echo "Error: halrun not found in PATH. Install LinuxCNC first." >&2
    exit 1
fi

echo "Starting halrun with component 'stepper-ninja'"
echo "IP_ADDRESS=${IP_ADDRESS}, THREAD=${THREAD_NAME}, PERIOD_NS=${THREAD_PERIOD_NS}"

{
cat <<EOF
loadrt threads name1=${THREAD_NAME} period1=${THREAD_PERIOD_NS}
loadrt stepper-ninja ip_address="${IP_ADDRESS}"
addf stepper-ninja.0.watchdog-process ${THREAD_NAME}
addf stepper-ninja.0.process-send ${THREAD_NAME}
addf stepper-ninja.0.process-recv ${THREAD_NAME}
start
show thread
show funct
show pin stepper-ninja.0.*
EOF

# Keep halrun alive and attached to your terminal for live HAL commands.
cat
} | halrun

echo "halrun finished."
