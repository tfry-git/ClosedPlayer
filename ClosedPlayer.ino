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

#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
//#include "AudioOutputBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
//#include "AudioOutputI2S.h"

AudioFileSourceSD *file;
AudioGeneratorMP3 *mp3;
AudioFileSourceBuffer *buff;
//AudioOutputBuffer *obuff;
AudioOutput *out;

SPIClass sdspi(HSPI);
File root;

#define RST_PIN         4
#define SS_PIN          5

MFRC522 mfrc522(SS_PIN, RST_PIN);

void uiloop(void *);

void setup() {
	Serial.begin(38400);

	SPI.begin();			// Init SPI bus
  sdspi.begin(14, 13, 27, 15); // Separate SPI bus for SD card. Note that these are not - quite - the standard pins. I had trouble uploading new code, while using the default pins
  pinMode(5, OUTPUT); //VSPI CS
  pinMode(15, OUTPUT); //HSPI CS

	mfrc522.PCD_Init();		// Init MFRC522
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  if (!SD.begin(15, sdspi)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  Serial.println("Hardware init complete");

  file = new AudioFileSourceSD("/test.mp3");
  buff = new AudioFileSourceBuffer(file, 2048);

  //out = new AudioOutputI2S(0, true); // Output via internal DAC: pins 25 and 26
  out = new AudioOutputI2SNoDAC(); // Output as PDM via I2S: pin 22
  //obuff = new AudioOutputBuffer(32600, out);
  mp3 = new AudioGeneratorMP3();
  file->seek (399999, SEEK_SET); // test seeking
  mp3->begin(buff, out);

  // Handle controls in separate task. Esp. reading RFID tags takes too long, causes hickups in the playback, if used in the same thread.
  xTaskCreate(uiloop, "ui", 10000, NULL, 1, NULL);
}

int card = 0;
bool playing = true;
int iteration = 0;

void uiloop(void *) {
  while (true) {
    --card;
    if (card < 0) card = 0;
    if (mfrc522.PICC_IsNewCardPresent()) {
      //if (mfrc522.PICC_ReadCardSerial())
      card = 5;
    }
    Serial.println(card);
    vTaskDelay (10);
  }
}

void loop() {
  if (mp3->isRunning()) {
    if (card) {
      if (!playing) {
        out->begin ();
        playing = true;
      }
      if (!mp3->loop()) mp3->stop();
    } else {
      if (playing) {
        out->stop ();
        playing = false;
      }
    }
  }
}

