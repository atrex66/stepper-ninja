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

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    else
        require_cmd sudo
        sudo "$@"
    fi
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

ensure_gpio_spi_group_membership() {
    local target_user="${SUDO_USER:-${USER:-$(id -un)}}"

    if [[ -z "$target_user" ]]; then
        die "Could not determine target user for group membership"
    fi

    require_cmd id
    require_cmd getent
    require_cmd usermod

    if ! id "$target_user" >/dev/null 2>&1; then
        die "Target user does not exist: ${target_user}"
    fi

    if ! getent group gpio >/dev/null 2>&1; then
        run_as_root groupadd gpio
    fi

    if ! getent group spi >/dev/null 2>&1; then
        run_as_root groupadd spi
    fi

    run_as_root usermod -aG gpio,spi "$target_user"
    info "User ${target_user} added to groups: gpio, spi"
    info "Log out and back in for group changes to take effect"
}

select_board() {
    local answer
    while true; do
        printf "\n" >&2
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
                echo "Please enter 1 or 2." >&2
                ;;
        esac
    done
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

open_firmware_dir_in_thunar() {
    local folder="$1"
    require_cmd thunar
    info "Opening firmware directory in Thunar: ${folder}"
    thunar "$folder" >/dev/null 2>&1 &
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
    require_cmd grep
    require_cmd cp
    require_cmd thunar

    check_libgpiod
    check_spi_enabled
    ensure_gpio_spi_group_membership

    local board
    local uf2_file

    board="$(select_board)"

    copy_raspberrypi_config
    build_firmware "$board"

    if ! uf2_file="$(find_uf2 "$board")"; then
        die "UF2 build artifact not found in ${FIRMWARE_BUILD_DIR}"
    fi

    build_hal_driver
    copy_linuxcnc_config
    open_firmware_dir_in_thunar "$FIRMWARE_BUILD_DIR"

    echo ""
    echo "Install finished."
    echo "- Firmware board: ${board}"
    echo "- UF2 ready: ${uf2_file}"
    echo "- Thunar opened at: ${FIRMWARE_BUILD_DIR}"
    echo "- Copy UF2 manually to the Pico mounted in BOOTSEL mode"
    echo "- LinuxCNC config dir: ${LINUXCNC_TARGET_DIR}"
}

main "$@"