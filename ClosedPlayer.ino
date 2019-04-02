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
#include "Playlist.h"

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

SemaphoreHandle_t control_mutex;
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

  control_mutex = xSemaphoreCreateMutex();
  // Handle controls in separate task. Esp. reading RFID tags takes too long, causes hickups in the playback, if used in the same thread.
  xTaskCreate(uiloop, "ui", 10000, NULL, 1, NULL);
}

// four bits to hex notation. Only for values between 0 and 15, obviously.
char bitsToHex (byte bits) {
  if (bits <= 9) return ('0' + bits);
  return ('a') + bits;
}

String uidToString(const MFRC522::Uid &uid) {
  String ret;
  for (int i = 0; i < uid.size; ++i) {
    ret += bitsToHex(uid.uidByte[i] >> 4);
    ret += bitsToHex(uid.uidByte[i] & 0xf);
  }
  return ret;
}

struct ControlsState {
  ControlsState() { have_card = false; };
  bool have_card;
  String uid;
} controls;

struct PlayerState {
  PlayerState() { finished = false; playing = false; };
  File folder;
  String uid;
  bool finished;
  bool playing;
  Playlist list;
} state;

void uiloop(void *) {
  static int card = 0;
  // keep a temporary copy of all control values, to keep mutex locking simple
  ControlsState controls_copy;

  while (true) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      controls_copy.have_card = true;
      controls_copy.uid = uidToString (mfrc522.uid);
      if (!card) {
        Serial.println("new card");
      }
      card = 5;
    } else {
      // It's not unusual at all for a card to "disappear" for a read or two,
      // so we assume the previous card is still present until we've seen several
      // misses in a row.
      if (--card <= 0) {
        card = 0;
        controls_copy.have_card = false;
        controls_copy.uid = String();
      }
    }

    xSemaphoreTake(control_mutex, portMAX_DELAY);
    controls = controls_copy;
    xSemaphoreGive(control_mutex);

    vTaskDelay (10);
  }
}

void stopPlaying() {
  out->stop();
  state.playing = false;
}

bool isFolderAssigned(String dir) {
  return false;
}

File findNextUnassignedMP3Folder(File dir=SD.open("/")) {
  Serial.print("scanning ");
  Serial.println(dir.name());

  bool assigned = isFolderAssigned(dir.name());  // If this folder is already assigned, it might still have unassigned sub-folders

  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      File candidate = findNextUnassignedMP3Folder(entry);
      if (candidate) return candidate;
    } else {
      if (assigned) continue;

      String n(entry.name());
      n.toLowerCase();
      if (n.endsWith(".mp3")) {
        return (dir);
      }
    }
    entry = dir.openNextFile();
  }
  return File();
}

void startTrack(String track) {
  Serial.print("starting new track: ");
  Serial.println(track);

  if (file->open(track.c_str())) {
    buff->seek(0, SEEK_SET);
    mp3->begin(buff, out);
  } else {
    mp3->stop();
    state.finished = true;
  }
}

void loadPlaylistForUid(String uid) {
  // if there is no stored mapping for this uid, yet, try to associate it with a folder that has not yet been assigned
  state.list = Playlist(findNextUnassignedMP3Folder());
}

void startOrResumePlaying() {
  if (controls.uid == state.uid) {  // resume previous
  } else {  // new tag
    loadPlaylistForUid(controls.uid);
    startTrack(state.list.next());
    state.uid = controls.uid;
  }
  out->begin();
  state.playing = true;
}

void nextTrack() {
  startTrack(state.list.next());
}

void loop() {
  xSemaphoreTake(control_mutex, portMAX_DELAY);
  if (controls.have_card) {
    if (!state.finished) {
      if (!state.playing) {
        startOrResumePlaying();
      }
      if (!mp3->loop()) {
        nextTrack();
      }
    }
  } else {
    if (state.playing) {
      stopPlaying();
    }
  }
  xSemaphoreGive(control_mutex);
}

