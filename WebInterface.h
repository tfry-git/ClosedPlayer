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

#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

void startWebInterface(bool access_point, const char* sess_id=0, const char *sess_pass=0);
void stopWebInterface();
char bitsToHex(byte bits);

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

AsyncWebServer *server = 0;

String urlencode(String in) {
  String ret;
  for (int i =0; i < in.length(); i++){
    char c = in.charAt(i);
    if (isalnum(c)){
      ret += c;
    } else{
      ret += '%';
      ret += bitsToHex(c >> 4);
      ret += bitsToHex(c & 0xf);
    }
  }
  return ret;
}

void startWebInterface(bool access_point, const char* sess_id, const char *sess_pass) {
  if (access_point) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig (IPAddress (192,168,4,1), IPAddress (0,0,0,0), IPAddress (255,255,255,0));
    WiFi.softAP("ClosedPlayer", "12345678");
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(sess_id, sess_pass);
  }

  server = new AsyncWebServer(80);
  server->on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String path = "/";
    if (request->hasParam("path")) path = request->getParam("path")->value();
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    String backpath;
    String cur = "ROOT";
    int pos = 1;
    while (pos < path.length()) {
      backpath += "/<a href=\"/?path=" + urlencode(path.substring(0, pos)) + "\">" + cur + "</a>";
      int next = path.indexOf("/", pos);
      if (next < 0) {
        backpath += "/" + path.substring(pos);
        break;
      }   
      cur = path.substring(pos, next);
      pos = next + 1;
    }
    response->printf("<!DOCTYPE html><html><head><title>ClosedPlayer WebInterface</title></head><body><h1>Listing path:</h1><h2>%s</h2>\n", backpath.c_str());
    File dir = SD.open(path);
    if (!dir) {
      response->printf("<h2>Could not be opened!</h2>\n");
    } else {
      if (!dir.isDirectory()) {
        response->printf("<p>Is a file (and streaming not yet supported)</p>\n");
      } else {
        response->printf("<ul>");
        File entry = dir.openNextFile();
        while(entry)  {
          String url = "/?path=";
          url += urlencode(entry.name());
          response->printf("<li><a href=\"%s\">%s</a></li>\n", url.c_str(), entry.name());
          entry = dir.openNextFile();
          response->printf("</li>");
        }
        response->printf("</ul>\n");
        response->printf("<form action=\"/mkdir\">Create subdir: <input type=\"text\" name=\"dir\"><input type=\"hidden\" name=\"parent\" value=\"%s\"><input type=\"submit\" value=\"Create\"></form>\n", path.c_str());
      }
    }
    response->printf("</body></html>");
    request->send(response);
  });
  server->on("/mkdir", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String path = "/";
    if (request->hasParam("parent")) path = request->getParam("parent")->value();
    if (request->hasParam("dir")) {
      if (!path.endsWith("/")) path += "/";
      path += request->getParam("dir")->value();
    }
    Serial.println(path.c_str());
    SD.mkdir(path);
    request->send(200, "text/html", "<!DOCTYPE html><html><head><title>ClosedPlayer WebInterface</title></head><body><h1>Directory created</h1>(Click back in your browser)</body></html>");
  });
  server->begin();
}

#endif
