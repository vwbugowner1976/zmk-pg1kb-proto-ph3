#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <cormoran/zmk/custom_settings.h>
#include <zmk/event_manager.h>
#include <zmk/input_processors/scroll_inertia_runtime.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define INERTIA_NODE DT_NODELABEL(scroll_inertia_right_sym)
#define SETTINGS_OWNER "cormoran_custom_settings"

#if DT_NODE_HAS_STATUS(INERTIA_NODE, okay)

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_right_sym_inertia_enabled,
    SETTINGS_OWNER,
    "pg1kb.right_sym.enabled",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_BOOL,
    ZMK_CUSTOM_SETTING_VALUE_BOOL(true),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_NO_CONSTRAINT);

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_right_sym_inertia_start,
    SETTINGS_OWNER,
    "pg1kb.right_sym.start",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(DT_PROP(INERTIA_NODE, start)),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 200));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_right_sym_inertia_move,
    SETTINGS_OWNER,
    "pg1kb.right_sym.move",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(DT_PROP(INERTIA_NODE, move)),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 2000));

ZMK_CUSTOM_SETTING_DEFINE(
    pg1kb_right_sym_inertia_stop,
    SETTINGS_OWNER,
    "pg1kb.right_sym.stop",
    ZMK_CUSTOM_SETTING_VALUE_TYPE_INT32,
    ZMK_CUSTOM_SETTING_VALUE_INT32(DT_PROP(INERTIA_NODE, stop)),
    ZMK_CUSTOM_SETTING_CONFIDENTIALITY_RPC_PUBLIC,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_PERMISSION_UNSECURE,
    ZMK_CUSTOM_SETTING_RANGE_INT32(0, 100));

static const struct device *const inertia_dev = DEVICE_DT_GET(INERTIA_NODE);

static int apply_all_settings(void) {
    if (!device_is_ready(inertia_dev)) {
        return -ENODEV;
    }

    int32_t start;
    int32_t move;
    int32_t stop;
    bool enabled;
    int ret;

    ret = zmk_custom_setting_get_int32(&pg1kb_right_sym_inertia_start, &start);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(&pg1kb_right_sym_inertia_move, &move);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_int32(&pg1kb_right_sym_inertia_stop, &stop);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_custom_setting_get_bool(&pg1kb_right_sym_inertia_enabled, &enabled);
    if (ret < 0) {
        return ret;
    }

    /* Apply numeric parameters first and enabled last. Each runtime write
     * resets any active gesture, so the next flick starts from one coherent
     * set of values. */
    ret = zmk_scroll_inertia_runtime_set(inertia_dev, ZMK_SCROLL_INERTIA_RUNTIME_START, start);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_scroll_inertia_runtime_set(inertia_dev, ZMK_SCROLL_INERTIA_RUNTIME_MOVE, move);
    if (ret < 0) {
        return ret;
    }
    ret = zmk_scroll_inertia_runtime_set(inertia_dev, ZMK_SCROLL_INERTIA_RUNTIME_STOP, stop);
    if (ret < 0) {
        return ret;
    }
    return zmk_scroll_inertia_runtime_set(
        inertia_dev, ZMK_SCROLL_INERTIA_RUNTIME_ENABLED, enabled ? 1 : 0);
}

static bool is_our_setting(const struct zmk_custom_setting *setting) {
    return setting == &pg1kb_right_sym_inertia_enabled ||
           setting == &pg1kb_right_sym_inertia_start ||
           setting == &pg1kb_right_sym_inertia_move ||
           setting == &pg1kb_right_sym_inertia_stop;
}

static int pg1kb_inertia_setting_changed(const zmk_event_t *eh) {
    const struct zmk_custom_setting_changed *ev = as_zmk_custom_setting_changed(eh);
    if (ev == NULL || ev->setting == NULL || !is_our_setting(ev->setting)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    int ret = apply_all_settings();
    if (ret < 0) {
        LOG_WRN("PG1KB inertia runtime apply failed: %d", ret);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(pg1kb_inertia_runtime_settings, pg1kb_inertia_setting_changed);
ZMK_SUBSCRIPTION(pg1kb_inertia_runtime_settings, zmk_custom_setting_changed);

/* Custom Settings initializes during APPLICATION init, while settings_load()
 * happens from ZMK main afterwards. Delay the first apply so persisted values
 * have been loaded before they are copied into the live inertia config. */
static struct k_work_delayable initial_apply_work;

static void initial_apply_handler(struct k_work *work) {
    ARG_UNUSED(work);

    int ret = apply_all_settings();
    if (ret == -ENODEV) {
        k_work_reschedule(&initial_apply_work, K_MSEC(250));
    } else if (ret < 0) {
        LOG_WRN("PG1KB inertia initial runtime apply failed: %d", ret);
    }
}

static int pg1kb_inertia_runtime_settings_init(void) {
    k_work_init_delayable(&initial_apply_work, initial_apply_handler);
    k_work_schedule(&initial_apply_work, K_MSEC(500));
    return 0;
}

SYS_INIT(pg1kb_inertia_runtime_settings_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif /* DT_NODE_HAS_STATUS(INERTIA_NODE, okay) */
