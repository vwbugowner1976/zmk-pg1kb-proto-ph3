#!/usr/bin/env bash
set -euo pipefail

# PG1KB ZMK v0.3 local build with scroll-inertia runtime tuning for My Keeb Studio.
# No GitHub Actions are used.

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

INERTIA_DIR="${INERTIA_DIR:-$ENV_ROOT/projects/zmk-input-processor-scroll-inertia}"
PAW3222_DIR="$ENV_ROOT/projects/zmk-driver-paw3222"
PMW3610_DIR="$ENV_ROOT/projects/zmk-pmw3610-driver"
NON_LIPO_DIR="$ENV_ROOT/projects/zmk-feature-non-lipo-battery-management"
CUSTOM_SETTINGS_DIR="$ENV_ROOT/projects/zmk-feature-custom-settings-v03"
RUNTIME_SETTINGS_DIR="$PROJECT_DIR/runtime-settings"
RUNTIME_PATCHER="$PROJECT_DIR/tools/patch-scroll-inertia-runtime.py"
STUDIO_MSGS_DIR="$WEST_TOPDIR/modules/msgs/zmk-studio-messages"

EXPECTED_BRANCH="feature/mykeeb-inertia-runtime"
ZMK_CUSTOM_STUDIO_REV="35bb5dafecd5931c432c74e1b630cf12097671e4"
STUDIO_MSGS_REV="89b81d2e587fce807b668dff2a6967a40beef421"
CUSTOM_SETTINGS_REV="419ffdc727a0bb09cac0298b74345b878473fbbc"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[[ -f "$ENV_ROOT/env.sh" ]] || fail "env.sh not found: $ENV_ROOT/env.sh"
[[ -d "$WEST_TOPDIR/.west" ]] || fail "west workspace not found: $WEST_TOPDIR"
[[ -d "$WEST_TOPDIR/.git" ]] || fail "ZMK git checkout not found: $WEST_TOPDIR"
[[ -d "$ZMK_APP" ]] || fail "ZMK app not found: $ZMK_APP"
[[ -f "$PROJECT_DIR/config/west.yml" ]] || fail "PG1KB repo not found: $PROJECT_DIR"
[[ -d "$INERTIA_DIR/.git" ]] || fail "local scroll-inertia module not found: $INERTIA_DIR"
[[ -d "$PAW3222_DIR" ]] || fail "PAW3222 module not found: $PAW3222_DIR"
[[ -d "$PMW3610_DIR" ]] || fail "PMW3610 module not found: $PMW3610_DIR"
[[ -d "$NON_LIPO_DIR" ]] || fail "non-LiPo module not found: $NON_LIPO_DIR"
[[ -d "$CUSTOM_SETTINGS_DIR/.git" ]] || fail "Custom Settings v0.3 module not found: $CUSTOM_SETTINGS_DIR (clone vwbugowner1976/zmk-feature-custom-settings-v03 first)"
[[ -d "$STUDIO_MSGS_DIR/.git" ]] || fail "zmk-studio-messages checkout not found: $STUDIO_MSGS_DIR"
[[ -f "$RUNTIME_SETTINGS_DIR/zephyr/module.yml" ]] || fail "runtime settings module missing: $RUNTIME_SETTINGS_DIR"
[[ -f "$RUNTIME_PATCHER" ]] || fail "runtime patcher missing: $RUNTIME_PATCHER"

current_branch="$(git -C "$PROJECT_DIR" branch --show-current 2>/dev/null || true)"
[[ "$current_branch" == "$EXPECTED_BRANCH" ]] || fail "wrong branch: '$current_branch' (expected '$EXPECTED_BRANCH')"

zmk_head="$(git -C "$WEST_TOPDIR" rev-parse HEAD 2>/dev/null || true)"
[[ "$zmk_head" == "$ZMK_CUSTOM_STUDIO_REV" ]] || \
    fail "ZMK revision is $zmk_head (expected cormoran v0.3 Custom Studio $ZMK_CUSTOM_STUDIO_REV)"

studio_msgs_head="$(git -C "$STUDIO_MSGS_DIR" rev-parse HEAD 2>/dev/null || true)"
[[ "$studio_msgs_head" == "$STUDIO_MSGS_REV" ]] || \
    fail "zmk-studio-messages revision is $studio_msgs_head (expected $STUDIO_MSGS_REV)"

custom_settings_head="$(git -C "$CUSTOM_SETTINGS_DIR" rev-parse HEAD 2>/dev/null || true)"
[[ "$custom_settings_head" == "$CUSTOM_SETTINGS_REV" ]] || \
    fail "Custom Settings revision is $custom_settings_head (expected $CUSTOM_SETTINGS_REV)"

# Reuse the existing environment exactly as created before.
if [[ -z "${VIRTUAL_ENV:-}" || ! -x "${VIRTUAL_ENV:-}/bin/west" ]]; then
    # shellcheck disable=SC1090
    source "$ENV_ROOT/env.sh"
fi

command -v west >/dev/null 2>&1 || fail "west not found after sourcing $ENV_ROOT/env.sh"
command -v python3 >/dev/null 2>&1 || fail "python3 not found"

cd "$WEST_TOPDIR"
actual_topdir="$(west topdir 2>/dev/null || true)"
[[ "$actual_topdir" == "$WEST_TOPDIR" ]] || fail "unexpected west topdir: '$actual_topdir' (expected '$WEST_TOPDIR')"

echo "Environment      : $ENV_ROOT"
echo "West topdir      : $WEST_TOPDIR"
echo "ZMK custom RPC   : $zmk_head"
echo "Studio messages  : $studio_msgs_head"
echo "PG1KB            : $PROJECT_DIR"
echo "Inertia          : $INERTIA_DIR"
echo "Custom Settings  : $CUSTOM_SETTINGS_DIR"
echo "Runtime adapter  : $RUNTIME_SETTINGS_DIR"
echo "Build dir        : $BUILD_DIR"
echo "Windows          : $WINDOWS_OUT"
echo

echo "==> Applying idempotent runtime API patch to pinned scroll-inertia"
python3 "$RUNTIME_PATCHER" "$INERTIA_DIR"

echo "==> Building PG1KB right / My Keeb inertia runtime"
EXTRA_MODULES="$INERTIA_DIR;$PAW3222_DIR;$PMW3610_DIR;$NON_LIPO_DIR;$CUSTOM_SETTINGS_DIR;$RUNTIME_SETTINGS_DIR"

west build -p always \
    -d "$BUILD_DIR" \
    -s "$ZMK_APP" \
    -b seeeduino_xiao_ble \
    -S studio-rpc-usb-uart \
    -- \
    -DSHIELD=pg1kb_proto_right \
    -DBOARD_ROOT="$PROJECT_DIR" \
    -DZMK_EXTRA_MODULES="$EXTRA_MODULES" \
    -DCONFIG_ZMK_STUDIO=y

UF2="$BUILD_DIR/zephyr/zmk.uf2"
[[ -f "$UF2" ]] || fail "UF2 was not generated: $UF2"

# Sanity-check the confirmed freeze fix in the actual generated config.
if ! grep -q '^CONFIG_INPUT_THREAD_STACK_SIZE=2048$' "$BUILD_DIR/zephyr/.config"; then
    fail "CONFIG_INPUT_THREAD_STACK_SIZE=2048 is not active in the generated build"
fi

mkdir -p "$WINDOWS_OUT"
base="pg1kb_proto_right_mykeeb_inertia"
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
