#!/usr/bin/env python3
"""Backport the custom-settings initialized event to the Zephyr 3.5/v0.3 fork.

The modern custom-settings module uses newer Kconfig syntax (configdefault),
which Zephyr 3.5 cannot parse.  The v03 fork is Kconfig-compatible but predates
the one-shot zmk_custom_settings_initialized event now consumed by the runtime
input processor.  Add only that event and raise it from the custom-settings
settings commit callback.

The patch is idempotent and intentionally refuses a module that contains
`configdefault`, so a modern checkout cannot be selected accidentally.
"""

from pathlib import Path
import sys


def fail(msg: str) -> None:
    raise SystemExit(f"ERROR: {msg}")


if len(sys.argv) != 2:
    fail("usage: patch-custom-settings-v03.py <zmk-feature-custom-settings-v03-dir>")

root = Path(sys.argv[1]).resolve()
header = root / "include/cormoran/zmk/custom_settings.h"
source = root / "src/custom_settings.c"
kconfig = root / "Kconfig"

for path in (header, source, kconfig):
    if not path.is_file():
        fail(f"missing {path}")

if "configdefault" in kconfig.read_text():
    fail(f"{root} is not the Zephyr-3.5-compatible v03 custom-settings checkout")

h = header.read_text()
s = source.read_text()

if "ZMK_EVENT_DECLARE(zmk_custom_settings_initialized);" not in h:
    anchor = "ZMK_EVENT_DECLARE(zmk_custom_setting_changed);"
    if anchor not in h:
        fail("custom-settings event declaration anchor not found")
    addition = r'''

/*
 * One-shot notification emitted after the boot settings_load() pass has
 * populated all persisted custom setting values.  Backported for ZMK v0.3 so
 * runtime consumers can safely apply saved values after settings are ready.
 */
struct zmk_custom_settings_initialized {
    uint8_t reserved;
};

ZMK_EVENT_DECLARE(zmk_custom_settings_initialized);'''
    h = h.replace(anchor, anchor + addition, 1)
    header.write_text(h)
    print("patched custom_settings.h: initialized event declaration")
else:
    print("custom_settings.h: initialized event already present")

# Re-read in case another run modified only one file.
s = source.read_text()
if "ZMK_EVENT_IMPL(zmk_custom_settings_initialized);" not in s:
    anchor = "ZMK_EVENT_IMPL(zmk_custom_setting_changed);"
    if anchor not in s:
        fail("custom-settings event implementation anchor not found")
    s = s.replace(
        anchor,
        anchor + "\nZMK_EVENT_IMPL(zmk_custom_settings_initialized);",
        1,
    )

old_handler = """SETTINGS_STATIC_HANDLER_DEFINE(custom_settings, SETTINGS_SUBTREE, NULL, custom_settings_handle_set,\n                               NULL, NULL);"""

if "custom_settings_handle_commit" not in s:
    if old_handler not in s:
        fail("custom-settings SETTINGS_STATIC_HANDLER_DEFINE anchor not found")
    replacement = r'''/*
 * settings_load() invokes the subtree commit callback after all custom setting
 * records have been loaded.  Raise this only once: later targeted
 * settings_load_subtree() calls (for example Discard) must not look like a
 * second boot initialization.
 */
static bool custom_settings_initialized_raised;

static int custom_settings_handle_commit(void) {
    if (custom_settings_initialized_raised) {
        return 0;
    }

    custom_settings_initialized_raised = true;
    raise_zmk_custom_settings_initialized((struct zmk_custom_settings_initialized){0});
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_settings, SETTINGS_SUBTREE, NULL, custom_settings_handle_set,
                               custom_settings_handle_commit, NULL);'''
    s = s.replace(old_handler, replacement, 1)

source.write_text(s)
print("custom_settings.c: initialized event backport ready")
