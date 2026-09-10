#!/usr/bin/env bash
set -euo pipefail

# PG1KB scroll-inertia freeze debug build
# Uses the existing local ZMK workspace on D: and never overwrites a UF2.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"
ZMK_DEV="${ZMK_DEV:-/mnt/d/zmk-dev}"
ZMK_APP="${ZMK_APP:-$ZMK_DEV/zmk/app}"
BUILD_DIR="${BUILD_DIR:-$ZMK_DEV/build-pg1kb-inertia-debug}"
WINDOWS_OUT="${WINDOWS_OUT:-/mnt/d/ZMK-UF2/PG1KB}"
EXPECTED_BRANCH="debug/scroll-inertia-freeze"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -d "$ZMK_DEV/.west" ]] || fail "west workspace not found: $ZMK_DEV"
[[ -d "$ZMK_APP" ]] || fail "ZMK app not found: $ZMK_APP"
[[ -f "$PROJECT_DIR/config/west.yml" ]] || fail "PG1KB repo not found: $PROJECT_DIR"

current_branch="$(git -C "$PROJECT_DIR" branch --show-current 2>/dev/null || true)"
[[ "$current_branch" == "$EXPECTED_BRANCH" ]] || fail "wrong branch: '$current_branch' (expected '$EXPECTED_BRANCH')"

# Reuse the already-created local Python environment when present.
if [[ -f "$ZMK_DEV/.venv/bin/activate" ]]; then
    # shellcheck disable=SC1091
    source "$ZMK_DEV/.venv/bin/activate"
elif [[ -f "$ZMK_DEV/venv/bin/activate" ]]; then
    # shellcheck disable=SC1091
    source "$ZMK_DEV/venv/bin/activate"
fi

command -v west >/dev/null 2>&1 || fail "west not found. Activate your existing zmk-dev venv first."

cd "$ZMK_DEV"

echo "==> Updating pinned scroll-inertia module"
west update zmk-input-processor-scroll-inertia

echo "==> Building PG1KB right / inertia debug"
west build -p always \
    -d "$BUILD_DIR" \
    -s "$ZMK_APP" \
    -b seeeduino_xiao_ble \
    -S studio-rpc-usb-uart \
    -S zmk-usb-logging \
    -- \
    -DSHIELD=pg1kb_proto_right \
    -DBOARD_ROOT="$PROJECT_DIR" \
    -DCONFIG_ZMK_STUDIO=y

UF2="$BUILD_DIR/zephyr/zmk.uf2"
[[ -f "$UF2" ]] || fail "UF2 was not generated: $UF2"

mkdir -p "$WINDOWS_OUT"

base="pg1kb_proto_right_inertia_debug"
dest="$WINDOWS_OUT/${base}.uf2"

# Never overwrite an existing file. Add a timestamp, then a counter if necessary.
if [[ -e "$dest" ]]; then
    stamp="$(date +%Y%m%d_%H%M%S)"
    dest="$WINDOWS_OUT/${base}_${stamp}.uf2"
    n=1
    while [[ -e "$dest" ]]; do
        dest="$WINDOWS_OUT/${base}_${stamp}_$n.uf2"
        n=$((n + 1))
    done
fi

cp -- "$UF2" "$dest"

echo
echo "BUILD OK"
echo "UF2: $dest"
if command -v wslpath >/dev/null 2>&1; then
    echo "Windows: $(wslpath -w "$dest")"
fi
