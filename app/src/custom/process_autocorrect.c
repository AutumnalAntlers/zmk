#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
// #include <zmk/events/autocorrect_state_changed.h>
#include <zmk/events/keycode_state_changed.h>

#include <dt-bindings/zmk/keys.h>

// #include <zmk/custom/process_autocorrect.h>
#include "process_autocorrect.h"

#if __has_include("autocorrect_data.h")
#    include "autocorrect_data.h"
#else
#    pragma message "Autocorrect is using the default library."
#    include "autocorrect_data_default.h"
#endif

#include <string.h>

static uint8_t typo_buffer[AUTOCORRECT_MAX_LENGTH] = {SPC};
static uint8_t typo_buffer_size                    = 1;

int bufferLength = 0;
int readIndex = 0;
int writeIndex = 0;

int autocorrect_event_listener(const zmk_event_t *eh) {
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev) {
    // count only key up events
    if (!ev->state) {
      process_autocorrect(ev->keycode, ev)
      LOG_DBG("key_pressed_count %d keycode %d", key_pressed_count, ev->keycode);
    }
  }
  return 0;
}

bool process_autocorrect(uint16_t keycode, zmk_event_t *record) {
  switch (keycode) {
    case A ... Z:
        // process normally
        break;
    case N1 ... N0:
    case TAB ... SEMICOLON:
    case GRAVE ... SLASH:
      // Set a word boundary if space, period, digit, etc. is pressed.
      keycode = SPC;
      break;
    case ENTER:
      // Behave more conservatively for the enter key. Reset, so that enter
      // can't be used on a word ending.
      typo_buffer_size = 0;
      keycode          = SPC;
      break;
    case BSPC:
      // Remove last character from the buffer.
      if (typo_buffer_size > 0) {
          --typo_buffer_size;
      }
      return true;
    case DQT:
      // Treat " as a word boundary.
      keycode = SPC;
      break;
    default:
      // Clear state if some other non-alpha key is pressed.
      typo_buffer_size = 0;
      return true;
  }

  // Rotate oldest character if buffer is full.
  if (typo_buffer_size >= AUTOCORRECT_MAX_LENGTH) {
      memmove(typo_buffer, typo_buffer + 1, AUTOCORRECT_MAX_LENGTH - 1);
      typo_buffer_size = AUTOCORRECT_MAX_LENGTH - 1;
  }

  // Append `keycode` to buffer.
  typo_buffer[typo_buffer_size++] = keycode;
  // Return if buffer is smaller than the shortest word.
  if (typo_buffer_size < AUTOCORRECT_MIN_LENGTH) {
      return true;
  }

  // Check for typo in buffer using a trie stored in `autocorrect_data`.
  uint16_t state = 0;
  uint8_t  code  = pgm_read_byte(autocorrect_data + state);
  for (int8_t i = typo_buffer_size - 1; i >= 0; --i) {
    uint8_t const key_i = typo_buffer[i];

    if (code & 64) { // Check for match in node with multiple children.
      code &= 63;
      for (; code != key_i; code = pgm_read_byte(autocorrect_data + (state += 3))) {
        if (!code) return true;
      }
      // Follow link to child node.
      state = (pgm_read_byte(autocorrect_data + state + 1) | pgm_read_byte(autocorrect_data + state + 2) << 8);
      // Check for match in node with single child.
    } else if (code != key_i) {
      return true;
    } else if (!(code = pgm_read_byte(autocorrect_data + (++state)))) {
      ++state;
    }

    // Stop if `state` becomes an invalid index. This should not normally
    // happen, it is a safeguard in case of a bug, data corruption, etc.
    if (state >= DICTIONARY_SIZE) {
      return true;
    }

    code = pgm_read_byte(autocorrect_data + state);

    if (code & 128) { // A typo was found! Apply autocorrect.
      const uint8_t backspaces = (code & 63); // + !record->event.pressed;
      for (uint8_t i = 0; i < backspaces; ++i) {
        ZMK_RAISE_EVENT(new_zmk_keycode_state_changed((struct zmk_keycode_state_changed){.usage_page = ev->usage_page record,
                                                                                         .keycode = BSPC,
                                                                                         .implicit_modifiers = 0,
                                                                                         .explicit_modifiers = 0,
                                                                                         .state = false,
                                                                                         .timestamp = k_uptime_get()}))
      }
      // send_string_P((char const *)(autocorrect_data + state + 1));

      if (keycode == SPC) {
        typo_buffer[0]  = SPC;
        typo_buffer_size = 1;
        return true;
      } else {
        typo_buffer_size = 0;
        return false;
      }
    }
  }
  return true;
}

ZMK_LISTENER(autocorrect, autocorrect_event_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);