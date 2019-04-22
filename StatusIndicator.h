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

#ifndef STATUSINDICATOR_H
#define STATUSINDICATOR_H

#include "config.h"

class StatusIndicator {
public:
  StatusIndicator() {
    transient_bits = 0;
    permanent_bits = 0;
    transient_timeout = 0;
    pinMode(LED_BLUE_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
  };
  enum StatusBits {
    WIFIEnabled  = 1,
    WIFIActivity = 1 << 1,
    BatteryLow   = 1 << 2,
    Playing      = 1 << 3,
    AtFileEOF    = 1 << 4,
    Error        = 1 << 5
  };
  bool isIdle() {
    uint16_t bits = permanent_bits | transient_bits;
    if (bits & (Playing | AtFileEOF | WIFIActivity | WIFIEnabled)) return false;
    return true;
  }
  void update() {
    uint16_t bits = permanent_bits;
    uint32_t now = millis();
    if (transient_bits) {
      uint32_t delta = now - transient_timeout;
      if (delta < 0xffff) { // no overflow, i.e. time now is larger than timeout
        transient_bits = 0;
      } else {
        bits |= transient_bits;
      }
    }

    if (bits & WIFIActivity) {
      digitalWrite(LED_BLUE_PIN, (now / 500) % 2);
    } else {
      digitalWrite(LED_BLUE_PIN, bits & WIFIEnabled);
    }

    if (bits & Error) {
      digitalWrite(LED_RED_PIN, (now / 250) % 2);
    } else {
      digitalWrite(LED_RED_PIN, bits & (BatteryLow | AtFileEOF));
    }

    uint16_t error_bits = bits & (Error | AtFileEOF);
    if ((bits & Playing) && !error_bits) {
      digitalWrite(LED_GREEN_PIN, true);
    } else if (isIdle()) {
      //analogWrite(LED_GREEN_PIN, abs(((now / 10) % 512) - 256));
      digitalWrite(LED_GREEN_PIN, (now / 2000) % 2);
    } else {
      digitalWrite(LED_GREEN_PIN, false);
    }
  }
  // Set a status bit that will be clearer, automatically, after 500ms
  void setTransientStatus(StatusBits which) {
    transient_bits |= which;
    transient_timeout = millis() + 500;
  }
  // Set a status bit that will stick, until cleared, explicitly
  void setPermanentStatus(StatusBits which, bool on=true) {
    if (on) permanent_bits |= which;
    else permanent_bits -= (permanent_bits & which);
  }
private:
  uint16_t transient_bits;
  uint16_t permanent_bits;
  uint32_t transient_timeout;
};

StatusIndicator indicator;

#endif
