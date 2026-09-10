#!/usr/bin/env bash
set -euo pipefail

ENV_ROOT="${ENV_ROOT:-$HOME/zmk-dev/v0.3}"
ZMK_DIR="$ENV_ROOT/zmk"
MSGS_DIR="$ZMK_DIR/modules/msgs/zmk-studio-messages"
STATE_FILE="$ENV_ROOT/.pg1kb-mykeeb-zmk-state"

ZMK_REV="35bb5dafecd5931c432c74e1b630cf12097671e4"
MSGS_REV="89b81d2e587fce807b668dff2a6967a40beef421"

fail() { echo "ERROR: $*" >&2; exit 1; }

[[ -d "$ZMK_DIR/.git" ]] || fail "ZMK checkout not found: $ZMK_DIR"
[[ -d "$MSGS_DIR/.git" ]] || fail "zmk-studio-messages checkout not found: $MSGS_DIR"

[[ -z "$(git -C "$ZMK_DIR" status --porcelain)" ]] || \
    fail "ZMK checkout has local changes. Commit/stash/revert them before switching."
[[ -z "$(git -C "$MSGS_DIR" status --porcelain)" ]] || \
    fail "zmk-studio-messages checkout has local changes."

if [[ ! -f "$STATE_FILE" ]]; then
    printf 'ZMK_ORIGINAL=%s\nMSGS_ORIGINAL=%s\n' \
        "$(git -C "$ZMK_DIR" rev-parse HEAD)" \
        "$(git -C "$MSGS_DIR" rev-parse HEAD)" > "$STATE_FILE"
    echo "Saved original revisions: $STATE_FILE"
fi

if git -C "$ZMK_DIR" remote get-url cormoran >/dev/null 2>&1; then
    git -C "$ZMK_DIR" remote set-url cormoran https://github.com/cormoran/zmk.git
else
    git -C "$ZMK_DIR" remote add cormoran https://github.com/cormoran/zmk.git
fi

git -C "$ZMK_DIR" fetch cormoran v0.3+custom-studio-protocol
git -C "$ZMK_DIR" checkout --detach "$ZMK_REV"

if git -C "$MSGS_DIR" remote get-url cormoran >/dev/null 2>&1; then
    git -C "$MSGS_DIR" remote set-url cormoran https://github.com/cormoran/zmk-studio-messages.git
else
    git -C "$MSGS_DIR" remote add cormoran https://github.com/cormoran/zmk-studio-messages.git
fi

git -C "$MSGS_DIR" fetch cormoran "$MSGS_REV"
git -C "$MSGS_DIR" checkout --detach "$MSGS_REV"

echo
echo "My Keeb ZMK v0.3 Custom Studio environment active"
echo "ZMK            : $(git -C "$ZMK_DIR" rev-parse HEAD)"
echo "Studio messages: $(git -C "$MSGS_DIR" rev-parse HEAD)"
