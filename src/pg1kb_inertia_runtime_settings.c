#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include <cormoran/zmk/custom_settings.h>
#include <zmk/event_manager.h>
#include <zmk/input_processors/scroll_inertia_runtime.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define PG1KB_INERTIA_SUBSYSTEM "cormoran_rip"

/*
 * First milestone: Right / Sym / Scroll (runtime processor name: rscroll).
 * The inertia processor sits AFTER rscroll, so its input already includes the
 * live speed multiplier/divisor.  Therefore inertia itself uses scale=1/1 and
 * these thresholds are the old 1/2-profile defaults converted to post-scale
 * units: start 25 -> 13, move 50 -> 25, stop 2 @ 1/2 -> 1 @ 1/1.
 */
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

static const struct device *const rscroll_inertia_dev =
    DEVICE_DT_GET(DT_NODELABEL(scroll_inertia_right_sym));

static bool is_rscroll_inertia_setting(const struct zmk_custom_setting *setting) {
    return setting == &pg1kb_rscroll_inertia_enabled ||
           setting == &pg1kb_rscroll_inertia_start ||
           setting == &pg1kb_rscroll_inertia_move ||
           setting == &pg1kb_rscroll_inertia_stop;
}

static int apply_rscroll_inertia_settings(void) {
    bool enabled = true;
    int32_t start = 13;
    int32_t move = 25;
    int32_t stop = 1;

    int ret = zmk_custom_setting_get_bool(&pg1kb_rscroll_inertia_enabled, &enabled);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(&pg1kb_rscroll_inertia_start, &start);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(&pg1kb_rscroll_inertia_move, &move);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(&pg1kb_rscroll_inertia_stop, &stop);
    if (ret < 0) {
        return ret;
    }

    struct zmk_scroll_inertia_runtime_config config = {
        .enabled = enabled,
        .start = start,
        .move = move,
        .stop = stop,
    };

    ret = zmk_scroll_inertia_runtime_set_config(rscroll_inertia_dev, &config);
    if (ret < 0) {
        LOG_ERR("Failed to stage rscroll inertia settings: %d", ret);
        return ret;
    }

    LOG_INF("rscroll inertia staged: en=%d start=%d move=%d stop=%d",
            enabled, start, move, stop);
    return 0;
}

static int pg1kb_inertia_setting_changed_cb(const zmk_event_t *eh) {
    const struct zmk_custom_setting_changed *ev = as_zmk_custom_setting_changed(eh);
    if (ev == NULL || !is_rscroll_inertia_setting(ev->setting)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    (void)apply_rscroll_inertia_settings();
    return ZMK_EV_EVENT_BUBBLE;
}

static int pg1kb_inertia_settings_initialized_cb(const zmk_event_t *eh) {
    const struct zmk_custom_settings_initialized *ev = as_zmk_custom_settings_initialized(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    (void)apply_rscroll_inertia_settings();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(pg1kb_inertia_setting_changed, pg1kb_inertia_setting_changed_cb);
ZMK_SUBSCRIPTION(pg1kb_inertia_setting_changed, zmk_custom_setting_changed);

ZMK_LISTENER(pg1kb_inertia_settings_initialized, pg1kb_inertia_settings_initialized_cb);
ZMK_SUBSCRIPTION(pg1kb_inertia_settings_initialized, zmk_custom_settings_initialized);
