#!/usr/bin/env python3
"""Patch the pinned scroll-inertia checkout with a tiny runtime tuning API.

The upstream repository remains pinned at f7dade...; this script modifies only
its local checkout before a PG1KB build. Runtime changes are staged and become
active only when the inertia state machine is IDLE, so a Studio write never
mutates the configuration used by an in-flight coast.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

EXPECTED_SHA = "f7dadefee453d555fe066d13a3de3bb60739b45e"
MARKER = "PG1KB_SCROLL_INERTIA_RUNTIME_V2"


def die(msg: str) -> None:
    raise SystemExit(f"ERROR: {msg}")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        die(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def main() -> None:
    if len(sys.argv) != 2:
        die("usage: patch-scroll-inertia-runtime.py <scroll-inertia checkout>")

    root = Path(sys.argv[1]).expanduser().resolve()
    src_path = root / "src" / "input_processor_scroll_inertia.c"
    cmake_path = root / "CMakeLists.txt"
    header_path = root / "include" / "zmk" / "input_processors" / "scroll_inertia_runtime.h"

    if not src_path.is_file() or not cmake_path.is_file():
        die(f"not a scroll-inertia checkout: {root}")

    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if head != EXPECTED_SHA:
        die(f"unexpected scroll-inertia revision {head}; expected {EXPECTED_SHA}")

    src = src_path.read_text(encoding="utf-8")
    if MARKER in src:
        print("scroll-inertia runtime API already applied")
        return

    status = subprocess.check_output(
        ["git", "-C", str(root), "status", "--porcelain", "--", "src/input_processor_scroll_inertia.c", "CMakeLists.txt"],
        text=True,
    ).strip()
    if status:
        die("scroll-inertia source/CMakeLists.txt has local changes; restore the pinned checkout first")

    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(
        """/* PG1KB_SCROLL_INERTIA_RUNTIME_V2 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

struct zmk_scroll_inertia_runtime_config {
    bool enabled;
    int32_t start;
    int32_t move;
    int32_t stop;
};

int zmk_scroll_inertia_runtime_set_config(
    const struct device *dev, const struct zmk_scroll_inertia_runtime_config *config);
int zmk_scroll_inertia_runtime_get_config(
    const struct device *dev, struct zmk_scroll_inertia_runtime_config *config);
""",
        encoding="utf-8",
    )

    cmake = cmake_path.read_text(encoding="utf-8")
    if "zephyr_include_directories(include)" not in cmake:
        cmake = "zephyr_include_directories(include)\n\n" + cmake
        cmake_path.write_text(cmake, encoding="utf-8")

    src = replace_once(
        src,
        '#include "scroll_inertia_math.h"\n',
        '#include "scroll_inertia_math.h"\n#include <errno.h>\n#include <zmk/input_processors/scroll_inertia_runtime.h>\n',
        "runtime includes",
    )

    src = replace_once(
        src,
        "    const struct device *dev;\n\n    enum scroll_state state;\n",
        "    const struct device *dev;\n\n"
        "    /* PG1KB_SCROLL_INERTIA_RUNTIME_V2: DT remains the default; Studio writes\n"
        "     * are staged in pending_cfg and applied only while IDLE. */\n"
        "    struct scroll_inertia_config runtime_cfg;\n"
        "    struct scroll_inertia_config pending_cfg;\n"
        "    bool runtime_enabled;\n"
        "    bool pending_enabled;\n"
        "    bool runtime_dirty;\n"
        "    struct k_spinlock runtime_lock;\n\n"
        "    enum scroll_state state;\n",
        "runtime data fields",
    )

    helper_anchor = "/* ------------------------------------------------------------------ */\n/* State transitions                                                   */\n"
    helper = r'''/* PG1KB_SCROLL_INERTIA_RUNTIME_V2
 * Apply pending Studio values only between gestures.  A running COASTING or
 * TRACKING gesture keeps one coherent config until it reaches IDLE. */
static void runtime_apply_pending_if_idle(struct scroll_inertia_data *data) {
    if (data->state != SS_IDLE || !data->runtime_dirty) {
        return;
    }

    k_spinlock_key_t key = k_spin_lock(&data->runtime_lock);
    if (data->runtime_dirty) {
        data->runtime_cfg = data->pending_cfg;
        data->runtime_enabled = data->pending_enabled;
        data->runtime_dirty = false;
    }
    k_spin_unlock(&data->runtime_lock, key);
}

'''
    src = replace_once(src, helper_anchor, helper + helper_anchor, "runtime apply helper")

    old_work_cfg = "    const struct scroll_inertia_config *cfg = data->dev->config;\n"
    if src.count(old_work_cfg) != 2:
        die(f"work-handler cfg pointers: expected 2 matches, found {src.count(old_work_cfg)}")
    src = src.replace(
        old_work_cfg,
        "    const struct scroll_inertia_config *cfg = &data->runtime_cfg;\n\n"
        "    if (!data->runtime_enabled) {\n"
        "        return;\n"
        "    }\n",
    )

    src = replace_once(
        src,
        "    struct scroll_inertia_data *data = dev->data;\n    const struct scroll_inertia_config *cfg = dev->config;\n\n    int32_t ax = effective_axis(cfg);\n",
        "    struct scroll_inertia_data *data = dev->data;\n"
        "    runtime_apply_pending_if_idle(data);\n"
        "    const struct scroll_inertia_config *cfg = &data->runtime_cfg;\n\n"
        "    if (!data->runtime_enabled) {\n"
        "        return ZMK_INPUT_PROC_CONTINUE;\n"
        "    }\n\n"
        "    int32_t ax = effective_axis(cfg);\n",
        "input handler runtime cfg",
    )

    runtime_api = r'''/* ------------------------------------------------------------------ */
/* Runtime tuning API                                                  */
/* ------------------------------------------------------------------ */

int zmk_scroll_inertia_runtime_set_config(
    const struct device *dev, const struct zmk_scroll_inertia_runtime_config *config) {
    if (dev == NULL || config == NULL) {
        return -EINVAL;
    }
    if (config->start < 0 || config->move < 0 || config->stop < 0 ||
        config->start > INT32_MAX / FP_SCALE || config->stop > INT32_MAX / FP_SCALE) {
        return -ERANGE;
    }

    struct scroll_inertia_data *data = dev->data;
    k_spinlock_key_t key = k_spin_lock(&data->runtime_lock);
    data->pending_cfg.start_fp = config->start * FP_SCALE;
    data->pending_cfg.move = config->move;
    data->pending_cfg.stop_fp = config->stop * FP_SCALE;
    data->pending_enabled = config->enabled;
    data->runtime_dirty = true;
    k_spin_unlock(&data->runtime_lock, key);
    return 0;
}

int zmk_scroll_inertia_runtime_get_config(
    const struct device *dev, struct zmk_scroll_inertia_runtime_config *config) {
    if (dev == NULL || config == NULL) {
        return -EINVAL;
    }

    struct scroll_inertia_data *data = dev->data;
    k_spinlock_key_t key = k_spin_lock(&data->runtime_lock);
    config->enabled = data->pending_enabled;
    config->start = data->pending_cfg.start_fp / FP_SCALE;
    config->move = data->pending_cfg.move;
    config->stop = data->pending_cfg.stop_fp / FP_SCALE;
    k_spin_unlock(&data->runtime_lock, key);
    return 0;
}

'''
    device_anchor = "/* ------------------------------------------------------------------ */\n/* Device boilerplate                                                  */\n"
    src = replace_once(src, device_anchor, runtime_api + device_anchor, "runtime API")

    src = replace_once(
        src,
        "    struct scroll_inertia_data *data = dev->data;\n    data->dev = dev;\n    data->state = SS_IDLE;\n",
        "    struct scroll_inertia_data *data = dev->data;\n"
        "    const struct scroll_inertia_config *initial = dev->config;\n"
        "    data->dev = dev;\n"
        "    data->runtime_cfg = *initial;\n"
        "    data->pending_cfg = *initial;\n"
        "    data->runtime_enabled = true;\n"
        "    data->pending_enabled = true;\n"
        "    data->runtime_dirty = false;\n"
        "    data->state = SS_IDLE;\n",
        "runtime init",
    )

    src_path.write_text(src, encoding="utf-8")
    print(f"scroll-inertia runtime API applied to {EXPECTED_SHA[:12]}")


if __name__ == "__main__":
    main()
