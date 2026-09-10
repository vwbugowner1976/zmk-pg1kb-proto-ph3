#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cormoran/zmk/custom_settings.h>
#include <zmk/event_manager.h>
#include <zmk/input_processors/scroll_inertia_runtime.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define PG1KB_INERTIA_SUBSYSTEM "cormoran_rip"

/*
 * All three inertia processors are downstream of the runtime input processor.
 * Therefore their direct input is already scaled by lscroll/lprec/rscroll and
 * inertia itself stays at 1/1. Defaults below are expressed in that post-scale
 * domain.
 */

/* Left / Base / Scroll (lscroll, default speed 1/2). */
ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lscroll_inertia_enabled,
    PG1KB_INERTIA_SUBSYSTEM,
    "lscroll.inertia.enabled",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,
    ZMK_CUSTOM_SETTING_VALUE_BOOL(true),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_NO_CONSTRAINT);

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lscroll_inertia_start,
    PG1KB_INERTIA_SUBSYSTEM,
    "lscroll.inertia.start",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(13),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 200));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lscroll_inertia_move,
    PG1KB_INERTIA_SUBSYSTEM,
    "lscroll.inertia.move",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(25),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 500));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lscroll_inertia_stop,
    PG1KB_INERTIA_SUBSYSTEM,
    "lscroll.inertia.stop",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(1),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 50));

/* Left / Sym / Precise Scroll (lprec, default speed 1/6). */
ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lprec_inertia_enabled,
    PG1KB_INERTIA_SUBSYSTEM,
    "lprec.inertia.enabled",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,
    ZMK_CUSTOM_SETTING_VALUE_BOOL(true),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_NO_CONSTRAINT);

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lprec_inertia_start,
    PG1KB_INERTIA_SUBSYSTEM,
    "lprec.inertia.start",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(4),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 200));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lprec_inertia_move,
    PG1KB_INERTIA_SUBSYSTEM,
    "lprec.inertia.move",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(8),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 500));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_lprec_inertia_stop,
    PG1KB_INERTIA_SUBSYSTEM,
    "lprec.inertia.stop",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(1),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 50));

/* Right / Sym / Scroll (rscroll, default speed 1/2). */
ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_rscroll_inertia_enabled,
    PG1KB_INERTIA_SUBSYSTEM,
    "rscroll.inertia.enabled",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,
    ZMK_CUSTOM_SETTING_VALUE_BOOL(true),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_NO_CONSTRAINT);

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_rscroll_inertia_start,
    PG1KB_INERTIA_SUBSYSTEM,
    "rscroll.inertia.start",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(13),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 200));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_rscroll_inertia_move,
    PG1KB_INERTIA_SUBSYSTEM,
    "rscroll.inertia.move",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(25),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(1, 500));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_rscroll_inertia_stop,
    PG1KB_INERTIA_SUBSYSTEM,
    "rscroll.inertia.stop",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(1),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_SECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 50));

struct pg1kb_inertia_group {
    const char *name;
    const struct device *dev;
    const struct zmk_custom_setting *enabled;
    const struct zmk_custom_setting *start;
    const struct zmk_custom_setting *move;
    const struct zmk_custom_setting *stop;
};

static const struct pg1kb_inertia_group inertia_groups[] = {
    {
        .name = "lscroll",
        .dev = DEVICE_DT_GET(DT_NODELABEL(scroll_inertia_left_base)),
        .enabled = &pg1kb_lscroll_inertia_enabled,
        .start = &pg1kb_lscroll_inertia_start,
        .move = &pg1kb_lscroll_inertia_move,
        .stop = &pg1kb_lscroll_inertia_stop,
    },
    {
        .name = "lprec",
        .dev = DEVICE_DT_GET(DT_NODELABEL(scroll_inertia_left_sym)),
        .enabled = &pg1kb_lprec_inertia_enabled,
        .start = &pg1kb_lprec_inertia_start,
        .move = &pg1kb_lprec_inertia_move,
        .stop = &pg1kb_lprec_inertia_stop,
    },
    {
        .name = "rscroll",
        .dev = DEVICE_DT_GET(DT_NODELABEL(scroll_inertia_right_sym)),
        .enabled = &pg1kb_rscroll_inertia_enabled,
        .start = &pg1kb_rscroll_inertia_start,
        .move = &pg1kb_rscroll_inertia_move,
        .stop = &pg1kb_rscroll_inertia_stop,
    },
};

static bool group_contains_setting(const struct pg1kb_inertia_group *group,
                                   const struct zmk_custom_setting *setting) {
    return setting == group->enabled || setting == group->start ||
           setting == group->move || setting == group->stop;
}

static int apply_group(const struct pg1kb_inertia_group *group) {
    bool enabled;
    int32_t start;
    int32_t move;
    int32_t stop;

    int ret = zmk_custom_setting_get_bool(group->enabled, &enabled);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(group->start, &start);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(group->move, &move);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(group->stop, &stop);
    if (ret < 0) {
        return ret;
    }

    struct zmk_scroll_inertia_runtime_config config = {
        .enabled = enabled,
        .start = start,
        .move = move,
        .stop = stop,
    };

    ret = zmk_scroll_inertia_runtime_set_config(group->dev, &config);
    if (ret < 0) {
        LOG_ERR("Failed to stage %s inertia settings: %d", group->name, ret);
        return ret;
    }

    LOG_INF("%s inertia staged: en=%d start=%d move=%d stop=%d",
            group->name, enabled, start, move, stop);
    return 0;
}

static int pg1kb_inertia_setting_changed_cb(const zmk_event_t *eh) {
    const struct zmk_custom_setting_changed *ev = as_zmk_custom_setting_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (size_t i = 0; i < ARRAY_SIZE(inertia_groups); i++) {
        if (group_contains_setting(&inertia_groups[i], ev->setting)) {
            (void)apply_group(&inertia_groups[i]);
            break;
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int pg1kb_inertia_settings_initialized_cb(const zmk_event_t *eh) {
    const struct zmk_custom_settings_initialized *ev = as_zmk_custom_settings_initialized(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (size_t i = 0; i < ARRAY_SIZE(inertia_groups); i++) {
        (void)apply_group(&inertia_groups[i]);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(pg1kb_inertia_setting_changed, pg1kb_inertia_setting_changed_cb);
ZMK_SUBSCRIPTION(pg1kb_inertia_setting_changed, zmk_custom_setting_changed);

ZMK_LISTENER(pg1kb_inertia_settings_initialized, pg1kb_inertia_settings_initialized_cb);
ZMK_SUBSCRIPTION(pg1kb_inertia_settings_initialized, zmk_custom_settings_initialized);
