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
#include "WebInterface.h"  // optional!

#include "AudioFileSourceSD.h"
#include "AudioFileSourceBuffer.h"
//#include "AudioOutputBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputI2S.h"

AudioFileSourceSD *file;
AudioGeneratorMP3 *mp3;
AudioFileSourceBuffer *buff;
AudioOutput *out;

SPIClass sdspi(HSPI);
File root;

#define RST_PIN         4
#define SS_PIN          5

MFRC522 mfrc522(SS_PIN, RST_PIN);

#define VOL_PIN  39
#define VOL_THRESHOLD 100

SemaphoreHandle_t control_mutex;
void uiloop(void *);

void setup() {
	Serial.begin(38400);

	SPI.begin();			// Init SPI bus
  sdspi.begin(14, 13, 27, 15); // Separate SPI bus for SD card. Note that these are not - quite - the standard pins. I had trouble uploading new code, while using the default pins
  pinMode(5, OUTPUT); //VSPI CS
  pinMode(15, OUTPUT); //HSPI CS
  stopWebInterface();

	mfrc522.PCD_Init();		// Init MFRC522
	mfrc522.PCD_DumpVersionToSerial();	// Show details of PCD - MFRC522 Card Reader details

  if (!SD.begin(15, sdspi)) {
    Serial.println("SD card initialization failed!");
    // TODO: Indicate error.
  }

  Serial.println("Hardware init complete");

  file = new AudioFileSourceSD();
  buff = new AudioFileSourceBuffer(file, 2048);

//  out = new AudioOutputI2S(0, true); // Output via internal DAC: pins 25 and 26
//  out = new AudioOutputI2S(); // Output via external I2S DAC: pins 25, 26, and 22
  out = new AudioOutputI2SNoDAC(); // Output as PDM via I2S: pin 22
//  out->SetGain(.8);
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
  ControlsState() { volume = 0; }
  String uid;
  bool haveTag() const {
    return (uid.length() > 0);
  }
  int volume;
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
  byte adc_count = 0;
  int adc_sum = 0;

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

    // Analog read is terribly noisy, and changing volume too often causes audio artefacts.
    // Therefore, we average over 20 samples (the lazy way, no moving average), and then check wether the new value
    // is more than a threshold value away from the previous reading.
    if (adc_count < 20) {
      ++adc_count;
      adc_sum += analogRead(VOL_PIN);
    } else {
      int new_vol = adc_sum / adc_count;
      adc_sum = 0;
      adc_count = 0;
      if ((controls_copy.volume > new_vol + VOL_THRESHOLD) || (controls_copy.volume < new_vol - VOL_THRESHOLD)) {
        Serial.println(new_vol);
        controls_copy.volume = new_vol;
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
    state.finished = false;
  } else {
    Serial.print("Empty track. stopping...");
    stopPlaying();
    state.finished = true;
    Serial.println(" stopped.");
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

void split(String in, char sep, std::vector<String> *out) {
  while(in.length()) {
    int next = in.indexOf(sep);
    if (next >= 0) {
      out->push_back(in.substring(0, next));
      in = in.substring(next+1);
    } else {
      out->push_back(in);
      break;
    }
  }
}

// tags.txt file format:
// UID\t[FLAG1[;FLAG2[;FLAG3...]]]\tDIR1[;DIR2[;DIR3...]]
void parseConfigLine(const String &line, std::vector<String> *options, std::vector<String> *files) {
  int tab1 = line.indexOf('\t');
  if (tab1 < 0) return;
  int tab2 = line.indexOf('\t', tab1 + 1);
  String config = line.substring(tab1+1, tab2);
  split(config, ';', options);

  // now parse files
  if (tab2 < 0) return;
  String dirspec = line.substring(tab2+1);
  split(dirspec, ';', files);
}

void loadPlaylistForUid(String uid) {
  File tagmap = SD.open(TAGS_FILE);
  // If there is no tags-file (yet), assign the current uid to be the "master control" tag, i.e. the one
  // to enable wifi.
  if (!tagmap) {
    tagmap = SD.open(TAGS_FILE, FILE_WRITE);
    tagmap.print(uid.c_str());
    tagmap.println("\twifi\t");
    tagmap.close();
    tagmap = SD.open(TAGS_FILE);
  }

  // Look for a stored mapping of this tag to options / playlist
  std::vector<String> known_directories;
  while(tagmap && tagmap.available()) {
    String line = readLine(tagmap);
    Serial.println(line);
    std::vector<String> files, options;
    parseConfigLine(line, &options, &files);
    known_directories.insert(known_directories.end(), files.begin(), files.end());

    if (line.startsWith(uid)) {
      Serial.print("Tag uid has ");
      Serial.print(options.size());
      Serial.print(" options and ");
      Serial.print(files.size());
      Serial.println(" associated files");
      state.list = Playlist(files);
      for (unsigned int i = 0; i < options.size(); ++i) {
        if (options[i] == "wifi") state.list.wifi_enabled = true;
      }
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
    nextTrack();
    state.uid = controls.uid;
  }

  if (state.list.wifi_enabled) {
    startWebInterface(true);
  }

  if (!state.finished) {
    out->begin();
    state.playing = true;
  }
}

void nextTrack() {
  startTrack(state.list.next());
}

void loop() {
  static int vol = controls.volume;
  xSemaphoreTake(control_mutex, portMAX_DELAY);
  if (controls.haveTag()) {
    if (vol != controls.volume) {
      vol = controls.volume;
      out->SetGain(controls.volume / 2048.0);
    }
    if (!state.playing) {
      startOrResumePlaying();
    } else if (!mp3->isRunning() || !mp3->loop()) {
      nextTrack();
    }
  } else {
    if (state.playing) {
      stopPlaying();
    }
    stopWebInterface();
  }
  xSemaphoreGive(control_mutex);
}

