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

// Uncomment exactly one of the following:
#define OUTPUT_NO_DAC   // For easy headphone-connectable mono-output via pin 22
//#define OUTPUT_INTERNAL_DAC  // Output via internal DAC: pins 25 and 26 - Stereo, but high impedance and not-so great resolution, esp. at low volume
//#define OUTPUT_I2S_DAC  // Output via external I2S DAC: pins 25, 26, and 22 - Best quality, but needs additional circuitry.

