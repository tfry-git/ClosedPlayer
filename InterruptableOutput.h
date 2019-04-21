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

#ifndef INTERRUPTABLEOUTPUT_H
#define INTERRUPTABLEOUTPUT_H

#include <AudioOutputBuffer.h>

/** The standard ESP8266Audio output classes are "greedy": They take samples until The
  * buffer is full, without returning from the (generator) loop. Sometimes it would be
  * nice to be able to do something (such as recording file position, adjusting volume)
  * after a defined number of samples, instead. That's what this class is for.
  * NOTE: Based on AudioOutputBuffer, as that already
  */
class InterruptableOutput : public AudioOutputBuffer {
public:
  InterruptableOutput(AudioOutput *out) : AudioOutputBuffer(0, out) {
    _timeout = 0;
    mode = Normal;
    _out = out;
  }
  bool ConsumeSample(int16_t sample[2]) override {
    if (mode == Normal) return _out->ConsumeSample(sample);

    bool ret;
    if (mode == Interrupt) {
      ret = _out->ConsumeSample(sample);
    } else if (mode == Swallow) {
      ret = true; // Do not forward anything to output, i.e. consume the sample in "no" time
    } else {
      int16_t f_sample[2];
      // expensive floating point stuff needed while seeking, only
      float scale = ((float) _timeout) / fade_scale;
      if (mode == FadeIn) scale = 1 - scale;
      f_sample[0] = sample[0] * scale;
      f_sample[1] = sample[1] * scale;
      ret = _out->ConsumeSample(f_sample);
    }

    if (!--_timeout) {
      mode = Normal;
      return false;
    }
    return ret;
  }

  /** Consume at most @param timeout samples before returning from the loop. Check timeoutActive()
   *  to find out, whether the timeout has been reached (alternatively, the real output buffer might
   *  be full). Set timeout to 0 for regular operation. */
  void setTimeout(uint16_t timeout) {
    _timeout = timeout;
    mode = Interrupt;
  }
  void fadeOut(uint16_t duration) {
    _timeout = duration;
    mode = FadeOut;
    fade_scale = _timeout;
  }
  void fadeIn(uint16_t duration) {
    _timeout = duration;
    mode = FadeIn;
    fade_scale = _timeout;
  }
  void setSwallow(uint16_t duration) {
    _timeout = duration;
    mode = Swallow;
  }
  bool isSpecialModeActive() {
    return (mode != Normal);
  }
  /** Swallow samples, instead of forwarding them to the real output. Might be useful
   *  for seeking, for instance.
  void setSwallow(bool on) {
    swallow = on;
  } */
  int getRate() {
    return hertz;
  }
private:
  uint16_t _timeout;
  uint16_t fade_scale;
  AudioOutput *_out;
  enum {
    Normal,
    Interrupt,
    FadeIn,
    FadeOut,
    Swallow,
  } mode;
};

#endif
