/*
 * Copyright (c) 2023 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_send_string

#include <drivers/behavior.h>
#include <drivers/character_map.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/toolchain.h>
#include <zmk/behavior.h>
#include <zmk/send_string.h>

struct behavior_send_string_config {
    const char *text;
    struct zmk_send_string_config config;
};

static int on_send_string_binding_pressed(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    const struct device *dev = device_get_binding(binding->behavior_dev);
    const struct behavior_send_string_config *config = dev->config;

    zmk_send_string(&config->config, event.position, config->text);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_send_string_binding_released(struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_send_string_driver_api = {
    .binding_pressed = on_send_string_binding_pressed,
    .binding_released = on_send_string_binding_released,
};

static int behavior_send_string_init(const struct device *dev) { return 0; }

#define GET_CHARACTER_MAP(n)                                                                       \
    DEVICE_DT_GET(DT_INST_PROP_OR(n, charmap, DT_CHOSEN(zmk_character_map)))

#define SEND_STRING_INST(n)                                                                        \
    BUILD_ASSERT(                                                                                  \
        DT_INST_NODE_HAS_PROP(n, charmap) || DT_HAS_CHOSEN(zmk_character_map),                     \
        "A character map must be chosen. See "                                                     \
        "https://zmk.dev/docs/behaviors/send-string#character-maps for more information.");        \
                                                                                                   \
    static const struct behavior_send_string_config behavior_send_string_config_##n = {            \
        .text = DT_INST_PROP(n, text),                                                             \
        .config =                                                                                  \
            {                                                                                      \
                .character_map = GET_CHARACTER_MAP(n),                                             \
                .wait_ms = DT_INST_PROP_OR(n, wait_ms, CONFIG_ZMK_SEND_STRING_DEFAULT_WAIT_MS),    \
                .tap_ms = DT_INST_PROP_OR(n, tap_ms, CONFIG_ZMK_SEND_STRING_DEFAULT_TAP_MS),       \
            },                                                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, behavior_send_string_init, NULL, NULL,                                \
                          &behavior_send_string_config_##n, APPLICATION,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_send_string_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SEND_STRING_INST);
