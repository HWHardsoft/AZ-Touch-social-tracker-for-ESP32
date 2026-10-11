#pragma once

// Select ONE controller
#define AZTOUCH_ESP32_DEVKIT
// #define AZTOUCH_FEATHER_HUZZAH32

#if defined(AZTOUCH_ESP32_DEVKIT)
  #define TFT_CS       5
  #define TFT_DC       4
  #define TFT_RST      22
  #define TFT_LED      15
  #define TOUCH_CS     14
  #define TOUCH_IRQ    27
  #define BUZZER_PIN   21
#elif defined(AZTOUCH_FEATHER_HUZZAH32)
  #define TFT_CS       33
  #define TFT_DC       15
  #define TFT_RST      -1
  #define TFT_LED      13
  #define TOUCH_CS     32
  #define TOUCH_IRQ    14
  #define BUZZER_PIN   27
#else
  #error "Select one AZ-Touch controller"
#endif

#define TFT_ROTATION 1

// 2.8" yellow-header touch calibration
#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600
#define MINPRESSURE 10
