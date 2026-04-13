#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)

create_link() {
    local target_rel="$1"
    local link_name="$2"
    local target_abs="$REPO_ROOT/$target_rel"
    local link_path="$SCRIPT_DIR/$link_name"

    if [[ ! -e "$target_abs" ]]; then
        echo "Missing target: $target_rel" >&2
        return 1
    fi

    if [[ -L "$link_path" ]]; then
        rm -f "$link_path"
    elif [[ -e "$link_path" ]]; then
        local backup_path="${link_path}.bak"
        rm -rf "$backup_path"
        mv "$link_path" "$backup_path"
        echo "Backed up existing file: $link_name -> $(basename "$backup_path")"
    fi

    ln -s "$target_abs" "$link_path"
    echo "Linked $link_name -> $target_rel"
}

create_link "firmware/inc/config.h" "config.h"
create_link "firmware/inc/footer.h" "footer.h"
create_link "firmware/inc/internals.h" "internals.h"
create_link "firmware/inc/kbmatrix.h" "kbmatrix.h"
create_link "firmware/modules/inc/jump_table.h" "jump_table.h"
create_link "firmware/modules/inc/pio_settings.h" "pio_settings.h"
create_link "firmware/modules/inc/transmission.h" "transmission.h"
create_link "firmware/modules/transmission.c" "transmission.c"

echo "Shared symlinks are ready."