#!/usr/bin/env bash
set -euo pipefail

# PG1KB scroll-inertia freeze debug build
# Actual local layout:
#   ~/zmk-dev/v0.3/              <- environment root (env.sh, build/, projects/)
#     ├─ env.sh
#     ├─ build/
#     ├─ projects/
#     │   └─ zmk-pg1kb-proto-ph3/   <- this repo
#     └─ zmk/                    <- west topdir (.west/ lives here)
#         └─ app/
#
# The generated UF2 is copied to Windows without overwriting an existing file.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"
PROJECTS_DIR="$(cd -- "$PROJECT_DIR/.." && pwd)"

if [[ "$(basename "$PROJECTS_DIR")" == "projects" ]]; then
    ENV_ROOT="${ENV_ROOT:-$(cd -- "$PROJECTS_DIR/.." && pwd)}"
else
    ENV_ROOT="${ENV_ROOT:-$HOME/zmk-dev/v0.3}"
fi

WEST_TOPDIR="${WEST_TOPDIR:-$ENV_ROOT/zmk}"
ZMK_APP="${ZMK_APP:-$WEST_TOPDIR/app}"
BUILD_DIR="${BUILD_DIR:-$ENV_ROOT/build/pg1kb-inertia-debug}"
WINDOWS_OUT="${WINDOWS_OUT:-/mnt/d/ZMK-Firmware/UF2/PG1KB}"
EXPECTED_BRANCH="debug/scroll-inertia-freeze"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -f "$ENV_ROOT/env.sh" ]] || fail "env.sh not found: $ENV_ROOT/env.sh"
[[ -d "$WEST_TOPDIR/.west" ]] || fail "west workspace not found: $WEST_TOPDIR"
[[ -d "$ZMK_APP" ]] || fail "ZMK app not found: $ZMK_APP"
[[ -f "$PROJECT_DIR/config/west.yml" ]] || fail "PG1KB repo not found: $PROJECT_DIR"

current_branch="$(git -C "$PROJECT_DIR" branch --show-current 2>/dev/null || true)"
[[ "$current_branch" == "$EXPECTED_BRANCH" ]] || fail "wrong branch: '$current_branch' (expected '$EXPECTED_BRANCH')"

# Reuse the existing environment exactly as created before.
# If the venv is not active, source env.sh automatically.
if [[ -z "${VIRTUAL_ENV:-}" || ! -x "${VIRTUAL_ENV:-}/bin/west" ]]; then
    # shellcheck disable=SC1090
    source "$ENV_ROOT/env.sh"
fi

command -v west >/dev/null 2>&1 || fail "west not found after sourcing $ENV_ROOT/env.sh"

actual_topdir="$(west topdir 2>/dev/null || true)"
[[ "$actual_topdir" == "$WEST_TOPDIR" ]] || fail "unexpected west topdir: '$actual_topdir' (expected '$WEST_TOPDIR')"

cd "$WEST_TOPDIR"

echo "Environment: $ENV_ROOT"
echo "West topdir: $WEST_TOPDIR"
echo "Project    : $PROJECT_DIR"
echo "Build dir  : $BUILD_DIR"
echo "Windows    : $WINDOWS_OUT"
echo

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

# Never overwrite an existing file. Add timestamp, then a counter if necessary.
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
