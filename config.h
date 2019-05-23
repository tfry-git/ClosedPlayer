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

// This file is for your local config: PIN assignements, output mode, WIFI credentials, etc.
// Make changes here as you want. However, if you plan on contributing anything
// back to this project (i.e. submit a PR), you should
// git update-index --assume-unchanged config.h
// so your local config does not get uploaded.

#ifndef CONFIG_H
#define CONFIG_H

// Uncomment exactly one of the following:
#define OUTPUT_NO_DAC   // For easy headphone-connectable mono-output via pin 22
//#define OUTPUT_INTERNAL_DAC  // Output via internal DAC: pins 25 and 26 - Stereo, but high impedance and not-so great resolution, esp. at low volume
//#define OUTPUT_I2S_DAC  // Output via external I2S DAC: pins 25, 26, and 22 - Best quality, but needs additional circuitry.

// pin mapping
// RFID reader. NOTE: The MFRC522 reader uses the default VSPI pins in addition to these!
#define MFRC522_RST_PIN         4
#define MFRC522_CS_PIN          5

// SD card. This needs to be on a separate SPI bus, as transactions with the RFID reader take too long for decent playback.
// Could be remapped to other pins, and in fact, these are not - quite - the standard pins. I had trouble uploading new code, while using the default pins
#define SD_SCK_PIN             14
#define SD_MISO_PIN            13
#define SD_MOSI_PIN            27
#define SD_CS_PIN              15

#define VOL_PIN                39  // Volume control. 0...3.3v
#define VOL_THRESHOLD          60  // Volume control change sensitivity
#define FORWARD_PIN            32  // Forward button. INPUT_PULLUP, i.e. button should connect to ground
#define REWIND_PIN             33  // REWIND button. INPUT_PULLUP, i.e. button should connect to ground

// Status indicator
#define LED_BLUE_PIN           21
#define LED_GREEN_PIN          17
#define LED_RED_PIN            16

// Power management
#define BAT_SENSE_PIN          36
#define POWER_CONTROL_PIN      12
#define BAT_WARN_THRESHOLD   ((int) (4096 * (3.15 / 2) / 3.3)) // You may want to determine the best value, empirically, but the idea is that the bat sense pin is connected to
                                                               // the battery voltage (after power control mosfet) via a 50/50 voltage divider. Then the warning will trigger around 3.4 (3.15 in my circuit)
                                                               // volts divided by 2.
#define BAT_WARN_RELEASE     (BAT_WARN_THRESHOLD + 100)  // Release value for battery low warning. A little higher than the warning threshold to avoid flicker
#define BAT_CUTOUT_THRESHOLD ((int) (4096 * (2.95 / 2) / 3.3))  // See above for details. Cut out around 3.1 (2.95 in my circuit) volts to protect the battery.

#define IDLE_SHUTDOWN_TIMEOUT 120  // Cut the power after this many seconds of being idle (no card present, or finished playing)

#endif

