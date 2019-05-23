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

#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <SD.h>
#include <vector>

/** Holds a collection of files to play. The collection may contain sub-directories, in which case a sub-playlist will be created for the files in the directory, as and when needed.
 *  Files in a directory are sorted, but based on 8.3 naming. Still useful, as long as files are named with an numeric prefix for ordering. */
class Playlist {
public:
  Playlist() {
    current = -1;
    sublist = 0;
    wifi_enabled = false;
  }
  Playlist(std::vector<String> items) : Playlist(){
    entries = items;
  }
  Playlist(File directory) : Playlist() {
    Serial.print("Creating playlist for ");
    Serial.print(directory.name());

    directory.rewindDirectory();
    File entry = directory.openNextFile();
    while (entry) {
      if (entry.isDirectory()) {
         entries.push_back(entry.name());
      } else {
        String n = String (entry.name());
        n.toLowerCase();
        if (n.endsWith(".mp3")) {
          entries.push_back(entry.name());
        }
      }
      entry = directory.openNextFile();
    }

    std::sort(entries.begin(), entries.end());

    Serial.print(entries.size());
    Serial.println(" entries.");
  }
  ~Playlist() {
    delete sublist;
  }
  String next() {
    if (sublist) {
      String ret = sublist->next();
      if (ret.length() > 0) return ret;
      // If we get here, sublist was depleted
      delete sublist;
      sublist = 0;
    }

    if (++current >= entries.size()) return String();

    String ret = entries[current];
    File f = SD.open(ret);
    if (f.isDirectory()) {
      sublist = new Playlist(f);
      return next();  // NOTE:  Calling on self, as the sublist _might_ be empty
    }

    return ret;
  }

  String previous() {
    if (sublist) {
      String ret = sublist->previous();
      if (ret.length() > 0) return ret;
      // If we get here, sublist was depleted
      delete sublist;
      sublist = 0;
    }

    if (--current < 0) return String();

    String ret = entries[current];
    File f = SD.open(ret);
    if (f.isDirectory()) {
      sublist = new Playlist(f);
      sublist->current = sublist->entries.size() - 1;
      return previous();  // NOTE:  Calling on self, as the sublist _might_ be empty
    }

    return ret;
  }

  String getCurrent() const {
    if (sublist) return sublist->getCurrent();
    if (current < 0 || isEmpty()) return String();
    return entries[current];
  }

  bool isEmpty() const {
    return entries.size() < 1;
  }

  void reset() {
    if (sublist) {
      delete sublist;
      sublist = 0;
    }
    current = -1;
  }

  String serialize() const {
    String ret = String(current);
    if (sublist) {
      ret += ",";
      ret += sublist->serialize();
    }
    return ret;
  }

  void unserialize(std::vector<String> positions) {
    if(positions.size() < 1) return;
    int pos = positions[0].toInt();
    current = pos - 1;
    next();
    if (sublist) {
      positions.erase(positions.begin(), positions.begin() + 1);
      sublist->unserialize(positions);
    }
  }

  bool wifi_enabled;
protected:
  std::vector<String> entries;
  int current;
  Playlist *sublist;
};

#endif
