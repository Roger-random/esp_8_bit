/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "esp_system.h"
#include "esp_int_wdt.h"

#define PERF  // some stats about where we spend our time
#include "src/emu.h"
#include "src/video_out.h"

// esp_8_bit
// Atari 8 computers, NES and SMS game consoles on your TV with nothing more than a ESP32 and a sense of nostalgia
// Supports NTSC/PAL composite video, Bluetooth Classic keyboards and joysticks

//  Choose one of the video standards: PAL,NTSC
#define VIDEO_STANDARD NTSC

//  Choose one of the following emulators: EMU_NES,EMU_SMS,EMU_ATARI
#define EMULATOR EMU_SMS

// The filesystem should contain folders named for each of the emulators i.e.
//    atari800
//    nofrendo
//    smsplus
// Folders will be auto-populated on first launch with a built in selection of sample media.
// Use 'ESP32 Sketch Data Upload' from the 'Tools' menu to copy a prepared data folder to ESP32

uint32_t _frame_time = 0;
uint32_t _drawn = 1;
bool _inited = false;

uint8_t** _bufLines;

// dual core mode runs emulator on comms core
void emu_task(void* arg)
{
    printf("Not Emulator running on core %d at %dmhz\n",
      xPortGetCoreID(),rtc_clk_cpu_freq_value(rtc_clk_cpu_freq_get()));
    _drawn = _frame_counter;
    while(true)
    {
      // wait for blanking before drawing to avoid tearing
      video_sync();

      // Draw a frame
      uint32_t t = xthal_get_ccount();
      _frame_time = xthal_get_ccount() - t;
      _lines = _bufLines;
      _drawn++;
    }
}

void setup()
{ 
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);  

  xTaskCreatePinnedToCore(emu_task, "emu_task", EMULATOR == EMU_NES ? 5*1024 : 3*1024, NULL, 0, NULL, 0); // nofrendo needs 5k word stack, start on core 0

  uint8_t* bufFlat = new uint8_t[256*240];
  _bufLines = new uint8_t*[240];
  uint8_t* linePointer = bufFlat;
  uint8_t yMask;
  for (int y = 0; y < 240; y++)
  {
    _bufLines[y] = linePointer;
    linePointer += 256;
    yMask = (((y/15)<<4)&0xF0);
    for (int x = 0; x < 256; x++)
    {
      _bufLines[y][x] = ((x>>4)&0x0F) | yMask;
    }
  }
}

#ifdef PERF
void perf()
{
  static int _next = 0;
  if (_drawn >= _next) {
    float elapsed_us = 120*1000000/(VIDEO_STANDARD ? 60 : 50);
    _next = _drawn + 120;
    
    printf("frame_time:%d drawn:%d displayed:%d blit_ticks:%d->%d, isr time:%2.2f%%\n",
      _frame_time/240,_drawn,_frame_counter,_blit_ticks_min,_blit_ticks_max,(_isr_us*100)/elapsed_us);
      
    _blit_ticks_min = 0xFFFFFFFF;
    _blit_ticks_max = 0;
    _isr_us = 0;
  }
}
#else
void perf(){};
#endif

// ntsc phase representation of a rrrgggbb pixel
// must be in RAM so VBL works
const uint32_t ntsc_RGB332[256] = {
    0x18181818,0x18171A1C,0x1A151D22,0x1B141F26,0x1D1C1A1B,0x1E1B1C20,0x20191F26,0x2119222A,
    0x23201C1F,0x241F1E24,0x251E222A,0x261D242E,0x29241F23,0x2A232128,0x2B22242E,0x2C212632,
    0x2E282127,0x2F27232C,0x31262732,0x32252936,0x342C232B,0x352B2630,0x372A2936,0x38292B3A,
    0x3A30262F,0x3B2F2833,0x3C2E2B3A,0x3D2D2E3E,0x40352834,0x41342B38,0x43332E3E,0x44323042,
    0x181B1B18,0x191A1D1C,0x1B192022,0x1C182327,0x1E1F1D1C,0x1F1E2020,0x201D2326,0x211C252B,
    0x24232020,0x25222224,0x2621252A,0x2720272F,0x29272224,0x2A262428,0x2C25282E,0x2D242A33,
    0x2F2B2428,0x302A272C,0x32292A32,0x33282C37,0x352F272C,0x362E2930,0x372D2C36,0x382C2F3B,
    0x3B332930,0x3C332B34,0x3D312F3A,0x3E30313F,0x41382C35,0x42372E39,0x4336313F,0x44353443,
    0x191E1E19,0x1A1D211D,0x1B1C2423,0x1C1B2628,0x1F22211D,0x20212321,0x21202627,0x221F292C,
    0x24262321,0x25252525,0x2724292B,0x28232B30,0x2A2A2625,0x2B292829,0x2D282B2F,0x2E272D34,
    0x302E2829,0x312E2A2D,0x322C2D33,0x332B3038,0x36332A2D,0x37322C31,0x38303037,0x392F323C,
    0x3B372D31,0x3C362F35,0x3E35323B,0x3F343440,0x423B2F36,0x423A313A,0x44393540,0x45383744,
    0x1A21221A,0x1B20241E,0x1C1F2724,0x1D1E2A29,0x1F25241E,0x20242622,0x22232A28,0x23222C2D,
    0x25292722,0x26292926,0x27272C2C,0x28262E30,0x2B2E2926,0x2C2D2B2A,0x2D2B2E30,0x2E2A3134,
    0x31322B2A,0x32312E2E,0x332F3134,0x342F3338,0x36362E2E,0x37353032,0x39343338,0x3A33363C,
    0x3C3A3032,0x3D393236,0x3E38363C,0x3F373840,0x423E3337,0x433E353B,0x453C3841,0x463B3A45,
    0x1A24251B,0x1B24271F,0x1D222B25,0x1E212D29,0x2029281F,0x21282A23,0x22262D29,0x23252F2D,
    0x262D2A23,0x272C2C27,0x282A2F2D,0x292A3231,0x2C312C27,0x2C302F2B,0x2E2F3231,0x2F2E3435,
    0x31352F2B,0x3234312F,0x34333435,0x35323739,0x3739312F,0x38383333,0x39373739,0x3A36393D,
    0x3D3D3433,0x3E3C3637,0x3F3B393D,0x403A3B41,0x43423637,0x4441383B,0x453F3C42,0x463F3E46,
    0x1B28291C,0x1C272B20,0x1D252E26,0x1E25302A,0x212C2B20,0x222B2D24,0x232A312A,0x2429332E,
    0x26302D24,0x272F3028,0x292E332E,0x2A2D3532,0x2C343028,0x2D33322C,0x2F323532,0x30313836,
    0x3238322C,0x33373430,0x34363836,0x35353A3A,0x383C3530,0x393B3734,0x3A3A3A3A,0x3B393C3E,
    0x3D403734,0x3E403938,0x403E3C3E,0x413D3F42,0x44453A38,0x45443C3C,0x46433F42,0x47424147,
    0x1C2B2C1D,0x1D2A2E21,0x1E293227,0x1F28342B,0x212F2E21,0x222E3125,0x242D342B,0x252C362F,
    0x27333125,0x28323329,0x2A31362F,0x2B303933,0x2D373329,0x2E36352D,0x2F353933,0x30343B37,
    0x333B362D,0x343B3831,0x35393B37,0x36383D3B,0x38403831,0x393F3A35,0x3B3D3E3B,0x3C3C403F,
    0x3E443A35,0x3F433D39,0x4141403F,0x42414243,0x44483D39,0x45473F3D,0x47464243,0x48454548,
    0x1C2E301E,0x1D2E3222,0x1F2C3528,0x202B382C,0x22333222,0x23323426,0x2530382C,0x262F3A30,
    0x28373526,0x2936372A,0x2A343A30,0x2B343C34,0x2E3B372A,0x2F3A392E,0x30393C34,0x31383F38,
    0x333F392E,0x343E3C32,0x363D3F38,0x373C413C,0x39433C32,0x3A423E36,0x3C41413C,0x3D404440,
    0x3F473E36,0x4046403A,0x41454440,0x42444644,0x454C413A,0x464B433E,0x47494644,0x49494949,
};

// this loop always runs on app_core (1).
void loop()
{    
  // start the video after emu has started
  if (!_inited) {
    if (_lines) {
      printf("video_init\n");
      video_init(4,EMU_SMS,ntsc_RGB332,VIDEO_STANDARD); // start the A/V pump
      _inited = true;
    } else {
      vTaskDelay(1);
    }
  }
  
  // Dump some stats
  perf();
}
