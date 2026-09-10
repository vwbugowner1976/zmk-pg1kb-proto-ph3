#!/usr/bin/env bash
set -euo pipefail

# Local-only PG1KB build for My Keeb Studio + runtime pointing + scroll inertia.
# No GitHub Actions.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${PROJECT_DIR:-$SCRIPT_DIR}"
ENV_ROOT="${ENV_ROOT:-$HOME/zmk-dev/v0.3}"
WEST_TOPDIR="${WEST_TOPDIR:-$ENV_ROOT/zmk}"
ZMK_APP="${ZMK_APP:-$WEST_TOPDIR/app}"
BUILD_DIR="${BUILD_DIR:-$ENV_ROOT/build/pg1kb-mykeeb-inertia}"
WINDOWS_OUT="${WINDOWS_OUT:-/mnt/d/ZMK-Firmware/UF2/PG1KB}"

fail() { echo "ERROR: $*" >&2; exit 1; }

module_path() {
    local name="$1"; shift
    local candidate
    for candidate in "$@"; do
        if [[ -n "$candidate" && -e "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    if command -v west >/dev/null 2>&1; then
        candidate="$(west list "$name" -f '{abspath}' 2>/dev/null || true)"
        if [[ -n "$candidate" && -e "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    fi
    return 1
}

[[ -f "$ENV_ROOT/env.sh" ]] || fail "env.sh not found: $ENV_ROOT/env.sh"
# shellcheck disable=SC1090
source "$ENV_ROOT/env.sh"
command -v west >/dev/null 2>&1 || fail "west not found after sourcing env.sh"
[[ -d "$WEST_TOPDIR/.west" ]] || fail "west workspace not found: $WEST_TOPDIR"
[[ -d "$ZMK_APP" ]] || fail "ZMK app not found: $ZMK_APP"

# This feature uses cormoran's ZMK v0.3 custom-Studio backport.  Keep the
# ordinary official-v0.3 checkout untouched and point ZMK_APP at the alternate
# checkout when invoking this script.
[[ -f "$ZMK_APP/include/zmk/studio/custom.h" ]] || fail \
  "Current ZMK does not contain custom Studio RPC (app/include/zmk/studio/custom.h missing). Use ZMK_APP=~/zmk-dev/v0.3/projects/zmk-cormoran-v03-custom/app"

INERTIA_DIR="$(module_path zmk-input-processor-scroll-inertia \
    "$ENV_ROOT/projects/zmk-input-processor-scroll-inertia")" || fail "scroll-inertia module not found"
PAW3222_DIR="$(module_path zmk-driver-paw3222 \
    "$ENV_ROOT/projects/zmk-driver-paw3222")" || fail "PAW3222 module not found"
PMW3610_DIR="$(module_path zmk-pmw3610-driver \
    "$ENV_ROOT/projects/zmk-pmw3610-driver")" || fail "PMW3610 module not found"
NON_LIPO_DIR="$(module_path zmk-feature-non-lipo-battery-management \
    "$ENV_ROOT/projects/zmk-feature-non-lipo-battery-management")" || fail "non-LiPo module not found"

# IMPORTANT: Zephyr 3.5 cannot parse modern custom-settings' `configdefault`
# Kconfig syntax.  Always prefer/use the dedicated v03 fork here.
CUSTOM_SETTINGS_DIR="$(module_path zmk-feature-custom-settings-v03 \
    "$ENV_ROOT/projects/zmk-feature-custom-settings-v03" \
    "$WEST_TOPDIR/modules/zmk-feature-custom-settings")" || fail \
    "zmk-feature-custom-settings-v03 not found. Clone vwbugowner1976/zmk-feature-custom-settings-v03 into $ENV_ROOT/projects first"

RUNTIME_INPUT_DIR="$(module_path zmk-module-runtime-input-processor \
    "$ENV_ROOT/projects/zmk-module-runtime-input-processor")" || fail "runtime input processor module not found"
PROSPECTOR_DIR="$(module_path prospector-zmk-module \
    "$ENV_ROOT/projects/prospector-zmk-module" \
    "$WEST_TOPDIR/modules/prospector-zmk-module")" || fail "prospector-zmk-module not found"

EXPECTED_INERTIA_SHA="f7dadefee453d555fe066d13a3de3bb60739b45e"
EXPECTED_CUSTOM_SETTINGS_SHA="419ffdc727a0bb09cac0298b74345b878473fbbc"
EXPECTED_RUNTIME_INPUT_SHA="43618985f8c9d5457cc333b7ca0733f2d361911e"

ACTUAL_INERTIA_SHA="$(git -C "$INERTIA_DIR" rev-parse HEAD)"
ACTUAL_CUSTOM_SETTINGS_SHA="$(git -C "$CUSTOM_SETTINGS_DIR" rev-parse HEAD)"
ACTUAL_RUNTIME_INPUT_SHA="$(git -C "$RUNTIME_INPUT_DIR" rev-parse HEAD)"

[[ "$ACTUAL_INERTIA_SHA" == "$EXPECTED_INERTIA_SHA" ]] || fail \
  "scroll-inertia is $ACTUAL_INERTIA_SHA; expected $EXPECTED_INERTIA_SHA"
[[ "$ACTUAL_CUSTOM_SETTINGS_SHA" == "$EXPECTED_CUSTOM_SETTINGS_SHA" ]] || fail \
  "custom-settings-v03 is $ACTUAL_CUSTOM_SETTINGS_SHA; expected $EXPECTED_CUSTOM_SETTINGS_SHA"
[[ "$ACTUAL_RUNTIME_INPUT_SHA" == "$EXPECTED_RUNTIME_INPUT_SHA" ]] || fail \
  "runtime-input-processor is $ACTUAL_RUNTIME_INPUT_SHA; expected $EXPECTED_RUNTIME_INPUT_SHA"

if grep -q '^configdefault ' "$CUSTOM_SETTINGS_DIR/Kconfig"; then
    fail "wrong Custom Settings checkout selected: Zephyr 3.5 cannot parse configdefault"
fi

# Backport only the one-shot settings-loaded event needed by the already-used
# runtime-input-processor persistence path, then add the scroll-inertia runtime
# API.  Both patchers are idempotent.
python3 "$PROJECT_DIR/tools/patch-custom-settings-v03.py" "$CUSTOM_SETTINGS_DIR"
python3 "$PROJECT_DIR/tools/patch-scroll-inertia-runtime.py" "$INERTIA_DIR"

grep -q 'ZMK_EVENT_DECLARE(zmk_custom_settings_initialized)' \
    "$CUSTOM_SETTINGS_DIR/include/cormoran/zmk/custom_settings.h" || fail \
    "Custom Settings initialized-event backport was not applied"

EXTRA_MODULES="$PROJECT_DIR;$INERTIA_DIR;$PAW3222_DIR;$PMW3610_DIR;$NON_LIPO_DIR;$CUSTOM_SETTINGS_DIR;$RUNTIME_INPUT_DIR;$PROSPECTOR_DIR"

cd "$WEST_TOPDIR"
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
[[ -f "$UF2" ]] || fail "UF2 not generated: $UF2"
grep -q '^CONFIG_INPUT_THREAD_STACK_SIZE=2048$' "$BUILD_DIR/zephyr/.config" || fail \
  "confirmed input-thread stack fix is missing"
grep -q '^CONFIG_PG1KB_INERTIA_RUNTIME_SETTINGS=y$' "$BUILD_DIR/zephyr/.config" || fail \
  "PG1KB inertia runtime settings are not enabled"

mkdir -p "$WINDOWS_OUT"
stamp="$(date +%Y%m%d_%H%M%S)"
dest="$WINDOWS_OUT/pg1kb_proto_right_mykeeb_inertia_${stamp}.uf2"
cp -- "$UF2" "$dest"

echo
echo "BUILD OK"
echo "UF2: $dest"
command -v wslpath >/dev/null 2>&1 && echo "Windows: $(wslpath -w "$dest")"
