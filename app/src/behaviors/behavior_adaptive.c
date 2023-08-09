/*
 * Copyright (c) 2023 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_adaptive

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_adaptive_config {
    uint8_t index;
    uint8_t bindings_count;
    struct zmk_behavior_binding (*bindings_ptr)[];
    uint8_t usage_pages_count;
    uint16_t usage_pages[];
};

struct behavior_adaptive_data {
    struct zmk_keycode_state_changed last_keycode_pressed;
    struct zmk_keycode_state_changed current_keycode_pressed;
    uint32_t wait_ms;
};

#define ZM_IS_NODE_MATCH(a, b) (strcmp(a, b) == 0)

#define WAIT_TIME DT_PROP(DT_INST(0, zmk_adaptive_control_wait_time), label)

#define IS_WAIT_TIME(dev) ZM_IS_NODE_MATCH(dev, WAIT_TIME)

static bool handle_control_binding(struct behavior_adaptive_data *state,
                                   const struct zmk_behavior_binding *binding) {
    if (IS_WAIT_TIME(binding->behavior_dev)) {
        state->wait_ms = binding->param1;
        // LOG_DBG("macro wait time set: %s %d %d", state->wait_ms->behavior_dev, state->wait_ms->param1, state->wait_ms->param2);
        LOG_DBG("macro wait time set: %d", state->wait_ms);
    } else {
        return false;
    }

    return true;
}

static int on_adaptive_binding_pressed(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct behavior_adaptive_data *data = dev->data;
    const struct behavior_adaptive_config *cfg = dev->config;

    if (data->last_keycode_pressed.usage_page == 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    memcpy(&data->current_keycode_pressed, &data->last_keycode_pressed,
           sizeof(struct zmk_keycode_state_changed));
    data->current_keycode_pressed.timestamp = k_uptime_get();

    ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(data->current_keycode_pressed));

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_adaptive_binding_released(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    struct behavior_adaptive_data *data = dev->data;

    if (data->current_keycode_pressed.usage_page == 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->current_keycode_pressed.timestamp = k_uptime_get();
    data->current_keycode_pressed.state = false;

    ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(data->current_keycode_pressed));
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_adaptive_driver_api = {
    .binding_pressed = on_adaptive_binding_pressed,
    .binding_released = on_adaptive_binding_released,
};

static int adaptive_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_adaptive, adaptive_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_adaptive, zmk_keycode_state_changed);

static const struct device *devs[DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)];

static int adaptive_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT); i++) {
        const struct device *dev = devs[i];
        if (dev == NULL) {
            continue;
        }

        struct behavior_adaptive_data *data = dev->data;
        const struct behavior_adaptive_config *config = dev->config;

        for (int u = 0; u < config->usage_pages_count; u++) {
            if (config->usage_pages[u] == ev->usage_page) {
                memcpy(&data->last_keycode_pressed, ev, sizeof(struct zmk_keycode_state_changed));
                data->last_keycode_pressed.implicit_modifiers |= zmk_hid_get_explicit_mods();
                break;
            }
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int behavior_adaptive_init(const struct device *dev) {
    const struct behavior_adaptive_config *config = dev->config;
    devs[config->index] = dev;
    return 0;
}

#define TRANSFORMED_BEHAVIORS(n)                                                                    \
    {LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), n)},

#define KR_INST(n)                                                                                  \
    static struct behavior_adaptive_data behavior_adaptive_data_##n = {};                           \
    static struct behavior_adaptive_config behavior_adaptive_config_##n = {                         \
        .index = n,                                                                                 \
        .bindings_count = DT_INST_PROP_LEN(n, bindings),                                            \
        .bindings_ptr = (*void)(TRANSFORMED_BEHAVIORS(n)),                                          \
        .usage_pages_count = DT_INST_PROP_LEN(n, usage_pages),                                      \
        .usage_pages = DT_INST_PROP(n, usage_pages),                                                \
    };                                                                                              \
    DEVICE_DT_INST_DEFINE(n, behavior_adaptive_init, NULL, &behavior_adaptive_data_##n,             \
                          &behavior_adaptive_config_##n, APPLICATION,                               \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_adaptive_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KR_INST)

#endif
