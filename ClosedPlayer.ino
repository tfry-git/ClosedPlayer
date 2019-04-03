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
  return ('a') + (bits - 10);
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
  String uid;
  bool haveTag() const {
    return (uid.length() > 0);
  }
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
  static int tag = 0;
  // keep a temporary copy of all control values, to keep mutex locking simple
  ControlsState controls_copy;

  while (true) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      controls_copy.uid = uidToString (mfrc522.uid);
      if (!tag) {
        Serial.print("new tag: ");
        Serial.println(controls_copy.uid);
      }
      tag = 5;
    } else {
      // It's not unusual at all for a card to "disappear" for a read or two,
      // so we assume the previous card is still present until we've seen several
      // misses in a row.
      if (--tag <= 0) {
        tag = 0;
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

void startTrack(String track) {
  Serial.print("starting new track: ");
  Serial.println(track);

  if (track.length() && file->open(track.c_str())) {
    buff->seek(0, SEEK_SET);
    mp3->begin(buff, out);
  } else {
    mp3->stop();
    state.finished = true;
  }
}

String readLine(File &f) {
  String line;
  if (!f) return line;
  while (f.available()) {
    char c = f.read();
    if (c == '\n') return line;
    if (c != '\r') line += c;
  }
  return line;
}

const char* TAGS_FILE = "/tags.txt";

File findNextUnassignedMP3Folder(const std::vector<String> &known_directories, File dir=SD.open("/")) {
  Serial.print("Scanning for unassigned mp3-dir ");
  Serial.println(dir.name());

  // If this folder is already assigned, it might still have unassigned sub-folders
  bool assigned = false;
  for (int i = known_directories.size() - 1; i >= 0; --i) {
    if (known_directories[i] == dir.name()) {
      assigned = true;
      break;
    }
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      File candidate = findNextUnassignedMP3Folder(known_directories, entry);
      if (candidate) return candidate;
    } else {
      if (assigned) {
        entry = dir.openNextFile();
        continue;
      }

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

// tags.txt file format:
// UID\tFLAGS\tDIR1\tDIR2\t...
void getFilesInLine(const String &line, std::vector<String> *files) {
  int pos = line.indexOf('\t', line.indexOf('\t') + 1);
  if (pos >= 0) {
    String dirspec = line.substring(pos+1);
    while(dirspec.length()) {
      int next = dirspec.indexOf('\t');
      if (next >= 0) {
        files->push_back(dirspec.substring(0, next));
        dirspec = dirspec.substring(next+1);
      } else {
        files->push_back(dirspec);
        break;
      }
    }
  }
}

void loadPlaylistForUid(String uid) {
  File tagmap = SD.open(TAGS_FILE);
  std::vector<String> known_directories;
  while(tagmap && tagmap.available()) {
    String line = readLine(tagmap);
    Serial.println(line);
    std::vector<String> dummy;
    getFilesInLine(line, &dummy);
    known_directories.insert(known_directories.end(), dummy.begin(), dummy.end());

    if (line.startsWith(uid)) {
      Serial.print("Tag uid has ");
      Serial.print(dummy.size());
      Serial.println(" associated files");
      state.list = Playlist(dummy);
      return;
    }
  }
  tagmap.close();

  // if there is no stored mapping for this uid, yet, try to associate it with a folder that has not yet been assigned
  File f = findNextUnassignedMP3Folder(known_directories);
  state.list = Playlist(f);

  // and store the new association
  if (!f) return;
  Serial.print("Writing new association ");
  Serial.print(uid);
  Serial.print(" - > ");
  Serial.println(f.name());
  File newmap = SD.open(TAGS_FILE, FILE_WRITE);
  newmap.seek(newmap.size());  // Contrary to documentation, FILE_WRITE does not seem to imply APPEND?!
  newmap.print(uid.c_str());
  newmap.print("\tdefault\t");
  newmap.println(f.name());
  newmap.close();
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
  if (controls.haveTag()) {
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

