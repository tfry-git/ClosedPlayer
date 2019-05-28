# The Closed Player

This project is an effort to create a very easy to use mp3 player for kids, based on open source and readily available cheap components (in short: an ESP32, an RFID module, an SD-card reader, an amplifier and a speaker).

The key idea of the player is that tracks are selected simply by placing an RFID-tagged object on / next to the player, thus creating a physical key to a file, rather than having to navigate complex menus.

## Status

The project is functional, and I have turned it into a birthday gift, successfully. However, I'll admit there are some rough edges left to address, esp. in the "administrative backend": Importantly uploading tracks, and making non-standard links to tags over wifi works, but is not a pretty sight. Also, documentation is still fairly rough, so if you want to build this, some experience with microcontrollers, or a high willingness to learn are definitely recommended. Further, some obvious features such as shuffle / loop / equalizer are still missing.

## Design objctives

Design objectives for this project:
- Easy and intuitive to use
- Buildable from cheap commonly available components
- Managable without network access (and optionally with local wireless network access)
- Text-based / form-based configuration - Note: Only text based configuration is currently implemented, and not feature complete
- Tinker-friendly
- Low power consumptions and instant on - Note: Boot time about half a second
- Ability to resume anywhere in a track
- Ability for basic seek

## Building the project

### Libraries
- RFID library (https://github.com/miguelbalboa/rfid)
- ESP8266Audio (https://github.com/earlephilhower/ESP8266Audio)
  - This library can do much more than just MP3 decoding, so if you need other formats that should be really easy to add!
- ESP8266Audio's unneccessary dependency ESP8266_Spiram (https://github.com/Gianbacchio/ESP8266_Spiram)

### Building the "essential" version
You can start with a trimmed down version, to find out, wether this is the right project for you, and then work your way from there:

#### Hardware
- ESP32 dev board (should not matter which one)
- RFID-RC522 card reader. Readily available at the time of this writing, but easy to replace by similar modules
- 3.3V SD card reader (purely passive component, readily available)
- some croc-clamps to connect a headphone

### Wiring

ESP32 to RFID-RC522:
- 3.3V -> 3.3V
- GPIO4 -> RST
- Gnd -> Gnd
- N/C -> IRQ
- GPIO19 -> MISO
- GPIO23 -> MOSI
- GPIO18 -> SCK
- GPIO5 -> SDA

ESP32 to SD-Card:
- 3.3V -> 3.3V
- Gnd -> Gnd
- GPIO13 -> MISO
- GPIO14 -> CLK
- GPIO27 -> MOSI
- GPIO15 -> CS

ESP32 to Headphone (PDM output via I2S; alternatives available):
- GPIO22 -> in
- Gnd -> Gnd

Dummy:
- GPIO39 -> Gnd (for max volume) or VCC/2 (for normal volume)

### Building the "full" version

#### Additional hardware
- An I2S DAC (e.g. PT8211)
- Audio amplifier - Note: There are some modules sporting an I2S DAC *and* amplifier. Consider those, if you want an easy solution.
- Speaker
- Two buttons
- A potentionmeter 50k or so, logarithmic is best but not required
- An RGB-LED
- Battery, and probably voltage booster, e.g. a powerbank circuit
- Power management circuit (description to follow, essentially some diodes, NPN, P-FET, resistors)

### Additional wiring
Controls:
- GPIO39 volume control (connect to a potentiomenter from 0 to 3.3v)
- GPIO32 and GPIO33: buttons, connect to ground; previous / next track (short press) - fast forward / rewind (long press)

RGB-Status LED (common kathode, add resistors as required):
- GPIO21 -> Blue (lights if WIFI enabled, blinks on WIFI activity)
- GPIO17 -> Green (lights while playing, blinks while idle)
- GPIO16 -> Red (indicates error states, low battery)

Power:
- GPIO39 -> Connected to battery voltage for sensing battery state. **Be sure to limit the voltage range**, e.g. using a voltage divider. You may also have to adjust the margins in config.h. If you want to skip this, connect to 3.3v.
- GPIO12 -> Goes high, when power should be on, goes low to shut down.

## Basic operation

- You will probably want to prepare your SD card with a few MP3 files (see below), before first start.
- The first RFID tag scanned by the reader will become the "master tag". This one will not start any track, but will enable the WIFI interface while present (by default: AP mode, SSID "ClosedPlayer", PASS "123456789"). Note that some boards may brown out when starting WIFI while powered from USB. Should you have trouble getting WIFI to work, first thing to try will be running from a dedicated power supply (strong USB chargers or powerbanks are the easiest option).
- The next RIFD tags scanned will become associated with folders containing MP3 files, automatically, one by one.
- Association between RFID tags and files are stored in a file "tags.txt" in the root folder of the SD card. If auto-association does not produce the desired results, you can simply edit this in a text editor.
- Buttons to skip / seek forward backward
- To upload new tracks to a closed ClosedPlayer, scan the "master tag", connect to the ClosedPlayer AP (see above), and navigate to http://192.168.4.1 . Upload tracks (usually one directory). Remove WIFI tag, and scan a new unassigned tag to associate it with the newly uploaded directory.

### SD-card file layout

- Copy your MP3 files onto the SD-card, organized in directories.
  - E.g. one directory per album / play.
  - Directories can be nested, arbitrarily, but each directory should usually contain only *either* MP3 files *or* subdirectories
- If the auto-association of key to folders is not correct, you can edit "tags.txt", manually. You can also associate a tag with several directories, or arbitrary files.

## Background ##

### Predecessors and similar projects

My inspiration for this project came from the Tonuino (https://www.voss.earth/tonuino/). Essentially, the Tonuino does pretty much the same thing as this project, but based on an Arduino Nano (or Uno, or Pro Mini), and a "DFplayer mini" module. The latter is a ready to use diy-friendly mp3-player module, sporting its own SD card slot and amplifier. This choice of integrated hardware make the Tonuino *very* easy to build, however it also introduces a few annoying limitations: The first is that we cannot freely access the SD card, which is a shame, because it means that cannot simply keep the configuration on the SD-card as text files. In lieu of this, the Tonuino sports an elaborate audio menu, which is flexible enough, but reminds me of a support hotline, uncomfortably. As a further minor nuisance, the DFplayer does not allow to seek in any way: We cannot even store and resume the position inside an audio track. Still, if you are looking for a really easy, really cheap DIY solution, and in particular, if you are already familiar with the classic Arduinos, I recommend the Tonuino whole-heartedly.

Without a doubt, there are both DIY and commercial precursors to the Tonuino. (One of?) the earliest commerical products seems to be the "Toniebox" (https://tonies.com/). But even before that, the year 2010 "Anybook reader" is/was based on a rather similar idea, and I would not be surprised to be pointed at yet earlier implementations. Anyway, using the Toniebox as a point of reference, this is clearly an extremely smooth and cute product, and it will be hard to achieve anything close in DIY, wiht a reasonable effort (who's talking about reaonable efforts, though). However, I am a tinkerer, and I'd like to explore some concepts, myself, such as replacing the (cute) "ear buttons" for volume control with a time-tested analog dial. I'd like further flexibility for creating custom tags (toy figures are cool, but they won't cut it, when it comes to managing four dozen episodes of your kid's favorite series). And most importantly, when a company expects me not only to download *their* content from the cloud, but also to require *me* to upload my custom content to their cloud, in order to get it onto the box, I'll just decline to play along, as a matter of principle.

### Hardware

Since the integrated DFplayer mini does not quite meet my requirements, I was looking for a cheap MCU/board that could handle the mp3 decoding on its own. I'm not usually a big fan of Espressif, with their half-baked documentation, and add-riddled forums, but their ESP8266 and ESP32 are hard to argue with in terms of power / cost. While you will find examples of ESP8266-based mp3 players, those are sitting on the edge in terms of RAM usage, and on their single SPI bus, it seems hard to impossible to accomodate both an SD-card with low latency, and a slow RFID reader module. While a bit more expensive, the ESP32 can handle both extremely well, bringing loads of RAM, two SPI busses, and two separate cores. So the ESP32 will form the core of this project. Beyond that, as far as RFID readers go, MFRC522 is widely available for 1$ and below at the time of this writing, and does the job. Next, we'll need an SD-card reader, again available for a few cents (make sure to use a 3.3volt variant). For the audio output, for simplicity we'll rely on the ESP32 to provide analog output (the alternative would be to use a cheap I2S-DAC; PT8211), and then an audio amplifier (PAM8403 is one that sells on ready to use boards for mere cents; PCM5102 is an I2S-DAC *with* amplifier), and finally a speaker. In this project I'll also be using a potentiometer with switch for volume control and on/off, and two buttons for seeking. I'll encourage you to experiment with your own controls, though.

### The name "The Closed Player"

- Refers to the fact that I'd like my files nearby (close), rather than in the cloud
- Hints that no media will have to be inserted (for the most part)
- Hints, tongue-in-cheek, that this player is open source, not closed
- Is a reverence to the late 1960's "Close N'Play", which was quit a nice shot at a kid-friendly audio player in its time.
