#!/bin/bash
# ZMK keymap から編集用 YAML を更新し、その内容で SVG も再生成する。
set -euo pipefail

cd "$(dirname "$0")/.."

KEYMAP=boards/shields/pg1kb_proto/pg1kb_proto.keymap
LAYOUT=boards/shields/pg1kb_proto/pg1kb_proto.dtsi
YAML=keymap-drawer/pg1kb_proto.yaml
SVG=keymap-drawer/pg1kb_proto.svg

uvx --from keymap-drawer keymap parse -z "$KEYMAP" -o "$YAML"

# 物理レイアウトは dtsi を正とし、編集対象 YAML には持ち込まない。
sed -i '/^layout: /d' "$YAML"

uvx --from keymap-drawer keymap draw -d "$LAYOUT" "$YAML" -o "$SVG"

echo "更新しました: $YAML / $SVG"