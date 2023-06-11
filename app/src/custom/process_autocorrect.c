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

const uint32_t HIGH_BIT_MASK = 1073741823; // (2**32 >> 2) - 1

static uint32_t typo_buffer[AUTOCORRECT_MAX_LENGTH] = {SPACE};
static uint32_t typo_buffer_size                    = 1;

int bufferLength = 0;
int readIndex = 0;
int writeIndex = 0;

int autocorrect_event_listener(const zmk_event_t *eh) {
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
  if (ev) {
    // count only key up events
    if (!ev->state) {
      LOG_DBG("[ANT-01] keycode: %d", ev->keycode);
      process_autocorrect(ev->keycode, eh);
    }
  }
  return 0;
}

bool process_autocorrect(uint32_t keycode, const zmk_event_t *record) {
  const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(record);
  switch (keycode) {
    case A ... Z:
      LOG_DBG("[ANT-02a]");
      // process normally
      break;
    case 4 ... 29:
      LOG_DBG("[ANT-02b]");
      // process normally
      break;
    case N1 ... N0:
      LOG_DBG("[ANT-03]");
    case TAB ... SEMICOLON:
      LOG_DBG("[ANT-04]");
    case GRAVE ... SLASH:
      LOG_DBG("[ANT-04]");
      // Set a word boundary if space, period, digit, etc. is pressed.
      keycode = SPACE;
      break;
    case ENTER:
      LOG_DBG("[ANT-05]");
      // Behave more conservatively for the enter key. Reset, so that enter
      // can't be used on a word ending.
      typo_buffer_size = 0;
      keycode          = SPACE;
      break;
    case BSPC:
      LOG_DBG("[ANT-06]");
      // Remove last character from the buffer.
      if (typo_buffer_size > 0) {
          --typo_buffer_size;
      }
      return true;
    case DQT:
      LOG_DBG("[ANT-07]");
      // Treat " as a word boundary.
      keycode = SPACE;
      break;
    default:
      LOG_DBG("[ANT-08]");
      // Clear state if some other non-alpha key is pressed.
      typo_buffer_size = 0;
      return true;
  }
  LOG_DBG("[ANT-09]");

  // Rotate oldest character if buffer is full.
  if (typo_buffer_size >= AUTOCORRECT_MAX_LENGTH) {
      LOG_DBG("[ANT-10]");
      memmove(typo_buffer, typo_buffer + 1, AUTOCORRECT_MAX_LENGTH - 1);
      typo_buffer_size = AUTOCORRECT_MAX_LENGTH - 1;
  }
  LOG_DBG("[ANT-11]");

  // Append `keycode` to buffer.
  typo_buffer[typo_buffer_size++] = keycode;
  // Return if buffer is smaller than the shortest word.
  if (typo_buffer_size < AUTOCORRECT_MIN_LENGTH) {
      LOG_DBG("[ANT-12]");
      return true;
  }
  LOG_DBG("[ANT-13]");

  // Check for typo in buffer using a trie stored in `autocorrect_data`.
  int state = 0;
  uint32_t code  = autocorrect_data[state];
  for (uint32_t i = typo_buffer_size - 1; i >= 0; --i) {
    LOG_DBG("[ANT-14] i: %d", i);
    uint32_t const key_i = typo_buffer[i];

    if (code & (HIGH_BIT_MASK + 1)) { // Check for match in node with multiple children.
      code &= HIGH_BIT_MASK;
      LOG_DBG("[ANT-15] code: %d", code);
      for (; code != key_i; code = autocorrect_data[(state += 3)]) {
        if (!code) return true;
      }
      // Follow link to child node.
      LOG_DBG("[ANT-16] pre-state: %d", state);
      state = (autocorrect_data[(state + 1)] | autocorrect_data[(state + 2)] << 8);
      LOG_DBG("[ANT-17] post-state: %d", state);
      // Check for match in node with single child.
    } else if (code != key_i) {
      LOG_DBG("[ANT-18]");
      return true;
    } else if (!(code = autocorrect_data[(++state)])) {
      ++state;
      LOG_DBG("[ANT-19] state: %d", state);
    }

    // XXX
    // // Stop if `state` becomes an invalid index. This should not normally
    // // happen, it is a safeguard in case of a bug, data corruption, etc.
    // if (state >= DICTIONARY_SIZE) {
    //   return true;
    // }

    code = autocorrect_data[state];
    LOG_DBG("[ANT-20] code: %d (%d-%d)", code, A, Z);

    if (code & (2 * (HIGH_BIT_MASK + 1))) { // A typo was found! Apply autocorrect.
      const uint32_t backspaces = (code & HIGH_BIT_MASK); // + !record->event.pressed;
      LOG_DBG("[ANT-21] backspaces: %d", backspaces);
      for (int i = 0; i < backspaces; ++i) {
        LOG_DBG("[ANT-22] i: %d", i);
        ZMK_EVENT_RAISE(
          new_zmk_keycode_state_changed(
            (struct zmk_keycode_state_changed){
              .usage_page = ev->usage_page,
              .keycode = BSPC,
              .implicit_modifiers = 0,
              .explicit_modifiers = 0,
              .state = false,
              .timestamp = k_uptime_get()}))
      }
      // send_string_P((char const *)(autocorrect_data + state + 1));

      if (keycode == SPACE) {
        LOG_DBG("[ANT-23]");
        typo_buffer[0]  = SPACE;
        typo_buffer_size = 1;
        return true;
      } else {
        LOG_DBG("[ANT-24]");
        typo_buffer_size = 0;
        return false;
      }
    }
  }
  return true;
  LOG_DBG("[ANT-25]");
}

ZMK_LISTENER(autocorrect, autocorrect_event_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);
