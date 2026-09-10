#!/usr/bin/env bash
set -euo pipefail

ENV_ROOT="${ENV_ROOT:-$HOME/zmk-dev/v0.3}"
ZMK_DIR="$ENV_ROOT/zmk"
MSGS_DIR="$ZMK_DIR/modules/msgs/zmk-studio-messages"
STATE_FILE="$ENV_ROOT/.pg1kb-mykeeb-zmk-state"

fail() { echo "ERROR: $*" >&2; exit 1; }

[[ -f "$STATE_FILE" ]] || fail "saved revision file not found: $STATE_FILE"
[[ -z "$(git -C "$ZMK_DIR" status --porcelain)" ]] || \
    fail "ZMK checkout has local changes. Commit/stash/revert them before restoring."
[[ -z "$(git -C "$MSGS_DIR" status --porcelain)" ]] || \
    fail "zmk-studio-messages checkout has local changes."

# shellcheck disable=SC1090
source "$STATE_FILE"

[[ -n "${ZMK_ORIGINAL:-}" ]] || fail "ZMK_ORIGINAL missing from state file"
[[ -n "${MSGS_ORIGINAL:-}" ]] || fail "MSGS_ORIGINAL missing from state file"

git -C "$ZMK_DIR" checkout --detach "$ZMK_ORIGINAL"
git -C "$MSGS_DIR" checkout --detach "$MSGS_ORIGINAL"
rm -f "$STATE_FILE"

echo "Original ZMK v0.3 revisions restored"
echo "ZMK            : $(git -C "$ZMK_DIR" rev-parse HEAD)"
echo "Studio messages: $(git -C "$MSGS_DIR" rev-parse HEAD)"
