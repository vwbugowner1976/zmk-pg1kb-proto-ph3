#!/bin/bash
# 手編集した YAML から SVG を再生成する。
set -euo pipefail

cd "$(dirname "$0")/.."

LAYOUT=boards/shields/pg1kb_proto/pg1kb_proto.dtsi
YAML=keymap-drawer/pg1kb_proto.yaml
SVG=keymap-drawer/pg1kb_proto.svg

uvx --from keymap-drawer keymap draw -d "$LAYOUT" "$YAML" -o "$SVG"

echo "再描画しました: $SVG"