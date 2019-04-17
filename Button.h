// kate: space-indent on; indent-width 2; mixedindent off; indent-mode cstyle;
/*
 *  The Closed Player - Kid-friendly MP3 player based on RFID tags
 *  
 *  See README.md for details and hardware setup.
 *  
 *  Copyright (c) 2019 Thomas Friedrichsmeier
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BUTTON_H
#define BUTTON_H

/** Helper class to debounce key presses, and detect long presses. This thing has been invented a
 *  million times, but it's still easier to roll my own, than to include a lib. */
template<uint16_t DEBOUNCE_T, uint16_t HOLD_T> class Button {
public:
  Button(bool initial_on=false) {
    was_clicked = false;
    state = initial_on ? On : Off;
    last_state_change = millis();
  }

  /** Return true, if button is currently pressed (NOT bouncing) */
  bool isPressed() {
    return (state == On) || (state == Falling);
  }

  /** Return true, if button is currently pressed, AND has been held for at least HOLD_T milliseconds */
  bool isHeld() const {
    return ((state == On) && (millis() - last_state_change >= HOLD_T));
  }

  /** Return true, if button has been short-pressed since the last call to this function */
  bool wasClicked() {
    if (was_clicked) {
      was_clicked = false;
      return true;
    }
    return false;
  }

  /** Update with a new reading */
  void update(bool current) {
    uint32_t now = millis();
    if (!current) {
      if (state == Off) {
        if (now - last_state_change > HOLD_T) last_state_change = now - HOLD_T; // avoid overflow
      } else if (state == Falling) {
        if (now - last_state_change >= DEBOUNCE_T) {
          last_state_change = now;
          state = Off;
        }
      } else if (state == Rising) {
        last_state_change = now;
        state = Off;
      } else if (state == On) {
        if (!isHeld()) was_clicked = true;
        last_state_change = now;
        state = Falling;
      }
    } else {
      if (state == On) {
        if (now - last_state_change > HOLD_T) last_state_change = now - HOLD_T; // avoid overflow
      } else if (state == Rising) {
        if (now - last_state_change >= DEBOUNCE_T) {
          last_state_change = now;
          state = On;
        }
      } else if (state == Falling) {
        last_state_change = now;
        state = Rising;
      } else if (state == Off) {
        last_state_change = now;
        state = Rising;
      }
    }
  }
private:
  enum {
    Off,
    Rising,
    On,
    Falling
  } state;
  bool was_clicked;
  uint32_t last_state_change;
};

#endif
