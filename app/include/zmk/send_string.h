/*
 * Copyright (c) 2023 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/device.h>

struct zmk_send_string_config {
    //! zmk,character-map driver instance to use
    const struct device *character_map;
    //! Time in milliseconds to wait between key presses
    uint32_t wait_ms;
    //! Time in milliseconds to wait between the press and release of each key
    uint32_t tap_ms;
};

/**
 * Queues behaviors to type a string.
 *
 * @param position Key position to use for the key presses/releases
 * @param text UTF-8 encoded text to type
 */
void zmk_send_string(const struct zmk_send_string_config *config, uint32_t position,
                     const char *text);
