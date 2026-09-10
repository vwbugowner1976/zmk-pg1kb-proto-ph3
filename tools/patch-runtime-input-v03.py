#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: patch-runtime-input-v03.py <zmk-module-runtime-input-processor-dir>")

root = Path(sys.argv[1])
path = root / "src/pointing/input_processor_runtime.c"
text = path.read_text()

replacements = {
    "zmk_keymap_layer_activate(data->temp_layer_layer, false)":
        "zmk_keymap_layer_activate(data->temp_layer_layer)",
    "zmk_keymap_layer_deactivate(data->temp_layer_layer, false)":
        "zmk_keymap_layer_deactivate(data->temp_layer_layer)",
}

changed = False
for old, new in replacements.items():
    if old in text:
        text = text.replace(old, new)
        changed = True

# Validate the desired v0.3-compatible form is present after patching.
for bad in replacements:
    if bad in text:
        raise SystemExit(f"failed to patch incompatible v0.3 keymap call: {bad}")

if "zmk_keymap_layer_activate(data->temp_layer_layer)" not in text:
    raise SystemExit("expected temp-layer activate call not found")
if "zmk_keymap_layer_deactivate(data->temp_layer_layer)" not in text:
    raise SystemExit("expected temp-layer deactivate call not found")

if changed:
    path.write_text(text)
    print("Patched runtime-input-processor keymap layer API for ZMK v0.3")
else:
    print("runtime-input-processor ZMK v0.3 keymap API patch already applied")
