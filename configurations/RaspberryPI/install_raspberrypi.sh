#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

FIRMWARE_DIR="${REPO_ROOT}/firmware"
FIRMWARE_BUILD_DIR="${FIRMWARE_DIR}/build"
FIRMWARE_CONFIG_SOURCE="${SCRIPT_DIR}/config.h"
FIRMWARE_CONFIG_TARGET="${FIRMWARE_DIR}/inc/config.h"

HAL_DRIVER_DIR="${REPO_ROOT}/hal-driver"
TEST_CONFIG_SOURCE_DIR="${HAL_DRIVER_DIR}/test_config"
FUTURE_CONFIG_SOURCE_DIR="${SCRIPT_DIR}/configs/raspberrypi"

LINUXCNC_CONFIGS_DIR="${LINUXCNC_CONFIGS_DIR:-${HOME}/linuxcnc/configs}"
LINUXCNC_TARGET_DIR="${LINUXCNC_CONFIGS_DIR}/raspberrypi"

BOOT_CONFIG_FILES=(
    "/boot/firmware/config.txt"
    "/boot/config.txt"
)

die() {
    echo "ERROR: $*" >&2
    exit 1
}

info() {
    echo "[INFO] $*"
}

require_cmd() {
    local cmd="$1"
    command -v "$cmd" >/dev/null 2>&1 || die "Missing command: ${cmd}"
}

check_libgpiod() {
    require_cmd pkg-config
    if ! pkg-config --exists libgpiod; then
        die "libgpiod is not installed. Install with: sudo apt install libgpiod-dev"
    fi
}

check_spi_enabled() {
    local cfg
    for cfg in "${BOOT_CONFIG_FILES[@]}"; do
        if [[ -f "$cfg" ]] && grep -Eq '^[[:space:]]*dtparam=spi=on([[:space:]]*#.*)?$' "$cfg"; then
            info "SPI is enabled in ${cfg}"
            return 0
        fi
    done

    echo ""
    echo "SPI is not enabled in /boot config files."
    echo "Add this line to /boot/firmware/config.txt (or /boot/config.txt):"
    echo "  dtparam=spi=on"
    echo "Then reboot and run this script again."
    exit 1
}

select_board() {
    local answer
    while true; do
        echo ""
        read -r -p "Select target board: [1] Pico1 (RP2040), [2] Pico2 (RP2350): " answer
        case "${answer,,}" in
            1|pico|pico1|rp2040)
                echo "pico"
                return 0
                ;;
            2|pico2|rp2350)
                echo "pico2"
                return 0
                ;;
            *)
                echo "Please enter 1 or 2."
                ;;
        esac
    done
}

find_pico_mount() {
    local m

    for m in "/media/${USER}/RPI-RP2" "/run/media/${USER}/RPI-RP2" "/media/RPI-RP2"; do
        if [[ -d "$m" && -w "$m" ]]; then
            echo "$m"
            return 0
        fi
    done

    if command -v lsblk >/dev/null 2>&1; then
        m="$(lsblk -nr -o LABEL,MOUNTPOINT 2>/dev/null | awk '$1=="RPI-RP2" && $2!="" {print $2; exit}')"
        if [[ -n "$m" && -d "$m" && -w "$m" ]]; then
            echo "$m"
            return 0
        fi
    fi

    return 1
}

wait_for_pico_mount() {
    local timeout_sec="${1:-120}"
    local elapsed=0
    local mount_point=""

    while (( elapsed < timeout_sec )); do
        if mount_point="$(find_pico_mount)"; then
            echo "$mount_point"
            return 0
        fi
        sleep 1
        ((elapsed += 1))
    done

    return 1
}

copy_raspberrypi_config() {
    if [[ -f "$FIRMWARE_CONFIG_SOURCE" ]]; then
        info "Copying Raspberry Pi firmware config"
        cp "$FIRMWARE_CONFIG_SOURCE" "$FIRMWARE_CONFIG_TARGET"
    else
        die "Missing Raspberry Pi firmware config: ${FIRMWARE_CONFIG_SOURCE}"
    fi
}

build_firmware() {
    local board="$1"

    info "Configuring firmware build for ${board}"
    cmake -S "$FIRMWARE_DIR" -B "$FIRMWARE_BUILD_DIR" -DBOARD="$board"

    info "Building firmware"
    cmake --build "$FIRMWARE_BUILD_DIR" -- -j"$(nproc)"
}

find_uf2() {
    local board="$1"
    local preferred="${FIRMWARE_BUILD_DIR}/stepper-ninja-${board}-spi.uf2"
    local candidate

    if [[ -f "$preferred" ]]; then
        echo "$preferred"
        return 0
    fi

    candidate="$(find "$FIRMWARE_BUILD_DIR" -maxdepth 1 -type f -name "stepper-ninja-${board}*.uf2" | head -n 1 || true)"
    if [[ -n "$candidate" ]]; then
        echo "$candidate"
        return 0
    fi

    return 1
}

flash_uf2() {
    local uf2_file="$1"
    local pico_mount="$2"

    info "Copying $(basename "$uf2_file") to ${pico_mount}"
    cp "$uf2_file" "$pico_mount/"
    sync
    info "UF2 copied successfully"
}

build_hal_driver() {
    info "Building and installing LinuxCNC HAL driver"
    "${HAL_DRIVER_DIR}/install.sh"
}

has_future_config() {
    [[ -d "$FUTURE_CONFIG_SOURCE_DIR" ]] || return 1
    find "$FUTURE_CONFIG_SOURCE_DIR" -mindepth 1 -type f ! -name "README.md" ! -name ".gitkeep" | grep -q .
}

copy_linuxcnc_config() {
    local source_dir

    mkdir -p "$LINUXCNC_TARGET_DIR"

    if has_future_config; then
        source_dir="$FUTURE_CONFIG_SOURCE_DIR"
        info "Using sample LinuxCNC config from ${source_dir}"
    else
        source_dir="$TEST_CONFIG_SOURCE_DIR"
        info "Sample LinuxCNC config not ready yet, copying test config from ${source_dir}"
    fi

    cp -a "$source_dir"/. "$LINUXCNC_TARGET_DIR"/
    info "LinuxCNC config copied to ${LINUXCNC_TARGET_DIR}"
}

main() {
    require_cmd cmake
    require_cmd make
    require_cmd find
    require_cmd awk
    require_cmd grep
    require_cmd cp

    check_libgpiod
    check_spi_enabled

    local board
    local uf2_file
    local pico_mount

    board="$(select_board)"

    echo ""
    echo "Connect the ${board} board in BOOTSEL mode now."
    echo "Hold BOOTSEL while plugging USB, then press Enter."
    read -r

    info "Waiting for Pico mass storage (RPI-RP2)"
    if ! pico_mount="$(wait_for_pico_mount 120)"; then
        die "Could not find a mounted RPI-RP2 volume within 120 seconds"
    fi
    info "Detected Pico mount: ${pico_mount}"

    copy_raspberrypi_config
    build_firmware "$board"

    if ! uf2_file="$(find_uf2 "$board")"; then
        die "UF2 build artifact not found in ${FIRMWARE_BUILD_DIR}"
    fi

    flash_uf2 "$uf2_file" "$pico_mount"
    build_hal_driver
    copy_linuxcnc_config

    echo ""
    echo "Install finished."
    echo "- Firmware board: ${board}"
    echo "- Flashed UF2: ${uf2_file}"
    echo "- LinuxCNC config dir: ${LINUXCNC_TARGET_DIR}"
}

main "$@"