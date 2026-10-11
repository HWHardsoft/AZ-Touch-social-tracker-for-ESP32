/*
 * AZ-Touch MOD 2.8" - Social Media Tracker 2026
 * v1.4 - final release, screenshot grabber removed
 *
 * Supported:
 *   - AZ-Touch MOD + ESP32 DevKit
 *   - AZ-Touch MOD Feather + Adafruit HUZZAH32 ESP32
 *
 * Libraries:
 *   Adafruit GFX Library
 *   Adafruit ILI9341
 *   XPT2046_Touchscreen
 */

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "hardware_config.h"
#include "credentials.h"
#include "graphics.h"

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

enum PlatformId {
  PLATFORM_YOUTUBE,
  PLATFORM_INSTAGRAM,
  PLATFORM_TIKTOK,
  PLATFORM_THREADS,
  PLATFORM_PINTEREST,
  PLATFORM_COUNT
};

struct SocialStats {
  const char *name;
  const char *metric;
  uint32_t value;
};

struct PlatformState {
  bool configured;
  bool validated;
  bool hasValue;
  unsigned long lastSuccess;
};

SocialStats stats[PLATFORM_COUNT] = {
  {"YouTube",   "SUBSCRIBERS", 0},
  {"Instagram", "FOLLOWERS",   0},
  {"TikTok",    "FOLLOWERS",   0},
  {"Threads",   "FOLLOWERS",   0},
  {"Pinterest", "FOLLOWERS",   0}
};

PlatformState platformState[PLATFORM_COUNT] = {};

uint8_t activePlatforms[PLATFORM_COUNT];
uint8_t activePlatformCount = 0;
uint8_t currentActiveIndex = 0;

unsigned long lastPageChange = 0;

const unsigned long PAGE_TIME = 5000;

// Intervals are configured in credentials.h.
const unsigned long HONESTHOOK_UPDATE_TIME =
  HONESTHOOK_UPDATE_MINUTES * 60UL * 1000UL;
const unsigned long DIRECT_UPDATE_TIME =
  DIRECT_UPDATE_MINUTES * 60UL * 1000UL;

TaskHandle_t apiTaskHandle = nullptr;

volatile bool initialLoadDone = false;
volatile bool platformDataDirty = false;
volatile int8_t pendingIncreasePlatform = -1;

enum LoadingStatus : uint8_t {
  LOAD_WIFI,
  LOAD_INSTAGRAM,
  LOAD_THREADS,
  LOAD_PINTEREST,
  LOAD_YOUTUBE,
  LOAD_TIKTOK,
  LOAD_DONE
};

volatile LoadingStatus loadingStatus = LOAD_WIFI;

const char *loadingStatusText(LoadingStatus status) {
  switch (status) {
    case LOAD_WIFI:      return "Connecting WiFi";
    case LOAD_INSTAGRAM: return "Loading Instagram";
    case LOAD_THREADS:   return "Loading Threads";
    case LOAD_PINTEREST: return "Loading Pinterest";
    case LOAD_YOUTUBE:   return "Loading YouTube";
    case LOAD_TIKTOK:    return "Loading TikTok";
    default:             return "Starting";
  }
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

bool nonEmpty(const char *s) {
  return s && s[0] != '\0';
}

void centeredText(const char *text, int centerY, const GFXfont *font, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;

  tft.setFont(font);
  tft.setTextSize(1);
  tft.setTextColor(color);

  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t cursorX = (320 - (int16_t)w) / 2 - x1;
  int16_t cursorY = centerY - y1 - (int16_t)h / 2;

  tft.setCursor(cursorX, cursorY);
  tft.print(text);
}

void drawLogo(uint8_t id) {
  const int cx = 160;
  const int cy = 88;

  int x = 0;
  int y = 0;

  switch (id) {
    case PLATFORM_YOUTUBE:
      x = cx - YOUTUBE_LOGO_WIDTH / 2;
      y = cy - YOUTUBE_LOGO_HEIGHT / 2;
      tft.drawRGBBitmap(x, y, youtube_logo,
                        YOUTUBE_LOGO_WIDTH, YOUTUBE_LOGO_HEIGHT);
      break;

    case PLATFORM_INSTAGRAM:
      x = cx - INSTAGRAM_LOGO_WIDTH / 2;
      y = cy - INSTAGRAM_LOGO_HEIGHT / 2;
      tft.drawRGBBitmap(x, y, instagram_logo,
                        INSTAGRAM_LOGO_WIDTH, INSTAGRAM_LOGO_HEIGHT);
      break;

    case PLATFORM_TIKTOK:
      x = cx - TIKTOK_LOGO_WIDTH / 2;
      y = cy - TIKTOK_LOGO_HEIGHT / 2;
      tft.drawRGBBitmap(x, y, tiktok_logo,
                        TIKTOK_LOGO_WIDTH, TIKTOK_LOGO_HEIGHT);
      break;

    case PLATFORM_THREADS:
      x = cx - THREADS_LOGO_WIDTH / 2;
      y = cy - THREADS_LOGO_HEIGHT / 2;
      tft.drawRGBBitmap(x, y, threads_logo,
                        THREADS_LOGO_WIDTH, THREADS_LOGO_HEIGHT);
      break;

    case PLATFORM_PINTEREST:
      x = cx - PINTEREST_LOGO_WIDTH / 2;
      y = cy - PINTEREST_LOGO_HEIGHT / 2;
      tft.drawRGBBitmap(x, y, pinterest_logo,
                        PINTEREST_LOGO_WIDTH, PINTEREST_LOGO_HEIGHT);
      break;
  }
}

void buildActivePlatformList() {
  uint8_t oldPlatform = 255;

  if (activePlatformCount > 0 && currentActiveIndex < activePlatformCount) {
    oldPlatform = activePlatforms[currentActiveIndex];
  }

  activePlatformCount = 0;

  for (uint8_t i = 0; i < PLATFORM_COUNT; i++) {
    // A page is only shown after its configuration has been
    // successfully validated at least once.
    if (platformState[i].configured && platformState[i].validated) {
      activePlatforms[activePlatformCount++] = i;
    }
  }

  currentActiveIndex = 0;

  // Keep the currently visible platform selected if possible.
  if (oldPlatform != 255) {
    for (uint8_t i = 0; i < activePlatformCount; i++) {
      if (activePlatforms[i] == oldPlatform) {
        currentActiveIndex = i;
        break;
      }
    }
  }
}

void drawNavigationDots() {
  if (activePlatformCount <= 1) return;

  const int spacing = 14;
  const int startX = 160 - ((activePlatformCount - 1) * spacing) / 2;

  for (uint8_t i = 0; i < activePlatformCount; i++) {
    if (i == currentActiveIndex)
      tft.fillCircle(startX + i * spacing, 228, 3, ILI9341_WHITE);
    else
      tft.drawCircle(startX + i * spacing, 228, 3, ILI9341_DARKGREY);
  }
}


void drawWiFiStatusIcon() {
  const int x = 301;
  const int y = 4;
  const uint16_t color =
    (WiFi.status() == WL_CONNECTED) ? ILI9341_WHITE : ILI9341_DARKGREY;

  // Clear only the small icon area.
  tft.fillRect(298, 0, 22, 19, ILI9341_BLACK);

  // Three small "radio waves" and a dot.
  tft.drawLine(x + 0,  y + 4,  x + 7,  y + 0, color);
  tft.drawLine(x + 7,  y + 0,  x + 14, y + 4, color);
  tft.drawLine(x + 3,  y + 8,  x + 7,  y + 6, color);
  tft.drawLine(x + 7,  y + 6,  x + 11, y + 8, color);
  tft.fillCircle(x + 7, y + 12, 2, color);
}

void drawLoadingScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);

  centeredText("Social Tracker", 54, &FreeSansBold18pt7b, ILI9341_WHITE);
  centeredText("Starting...", 108, &FreeSans9pt7b, ILI9341_LIGHTGREY);

  // Progress-bar frame. The moving block is animated by updateLoadingAnimation().
  tft.drawRoundRect(49, 143, 222, 18, 5, ILI9341_DARKGREY);

  drawWiFiStatusIcon();
}

void updateLoadingAnimation() {
  static unsigned long lastFrame = 0;
  static int barPos = 0;
  static int direction = 1;
  static LoadingStatus lastStatus = LOAD_DONE;

  if (millis() - lastFrame < 45) return;
  lastFrame = millis();

  LoadingStatus status = loadingStatus;

  if (status != lastStatus) {
    tft.fillRect(20, 86, 280, 36, ILI9341_BLACK);
    centeredText(loadingStatusText(status), 104,
                 &FreeSans9pt7b, ILI9341_LIGHTGREY);
    lastStatus = status;
  }

  // Indeterminate progress animation: a bright block moves continuously
  // while network requests run in the background.
  tft.fillRect(53, 147, 214, 10, ILI9341_BLACK);
  tft.fillRoundRect(53 + barPos, 147, 48, 10, 3, ILI9341_WHITE);

  barPos += direction * 5;
  if (barPos >= 166) {
    barPos = 166;
    direction = -1;
  } else if (barPos <= 0) {
    barPos = 0;
    direction = 1;
  }
}

void playFollowerIncreaseSignal() {
  tone(BUZZER_PIN, 1800, 90);
  delay(115);
  tone(BUZZER_PIN, 2400, 120);
}

bool selectPlatform(uint8_t platformId) {
  for (uint8_t i = 0; i < activePlatformCount; i++) {
    if (activePlatforms[i] == platformId) {
      currentActiveIndex = i;
      redrawCurrentPage();
      lastPageChange = millis();
      return true;
    }
  }
  return false;
}

void drawNoPlatforms() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);

  centeredText("Social Tracker", 54, &FreeSansBold18pt7b, ILI9341_WHITE);
  centeredText("No active platform", 116, &FreeSansBold12pt7b, ILI9341_LIGHTGREY);
  centeredText("Check credentials.h", 153, &FreeSans9pt7b, ILI9341_LIGHTGREY);
  drawWiFiStatusIcon();
}

void drawPlatform(uint8_t id) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);

  centeredText(stats[id].name, 20, &FreeSansBold12pt7b, ILI9341_WHITE);
  drawLogo(id);

  char value[24];

  if (platformState[id].hasValue) {
    snprintf(value, sizeof(value), "%lu", (unsigned long)stats[id].value);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  centeredText(value, 158, &FreeSansBold24pt7b, ILI9341_WHITE);
  centeredText(stats[id].metric, 199, &FreeSans9pt7b, ILI9341_LIGHTGREY);

  if (activePlatformCount > 1) {
    tft.fillTriangle(8, 120, 16, 114, 16, 126, ILI9341_DARKGREY);
    tft.fillTriangle(312, 120, 304, 114, 304, 126, ILI9341_DARKGREY);
  }

  drawNavigationDots();
  drawWiFiStatusIcon();
}

void redrawCurrentPage() {
  if (activePlatformCount == 0) {
    drawNoPlatforms();
  } else {
    drawPlatform(activePlatforms[currentActiveIndex]);
  }
}

void nextPlatform() {
  if (activePlatformCount == 0) return;

  currentActiveIndex = (currentActiveIndex + 1) % activePlatformCount;
  redrawCurrentPage();
  lastPageChange = millis();
}

void previousPlatform() {
  if (activePlatformCount == 0) return;

  currentActiveIndex =
    (currentActiveIndex == 0) ? activePlatformCount - 1 : currentActiveIndex - 1;

  redrawCurrentPage();
  lastPageChange = millis();
}

// ------------------------------------------------------------
// Touch
// ------------------------------------------------------------

bool getTouchXY(int &x, int &y) {
  if (!touch.touched()) return false;

  TS_Point p = touch.getPoint();
  if (p.z <= MINPRESSURE) return false;

  int px = map(p.x, TS_MINX, TS_MAXX, 0, 319);
  int py = map(p.y, TS_MINY, TS_MAXY, 239, 0);

  x = 319 - constrain(px, 0, 319);
  y = constrain(py, 0, 239);
  return true;
}

void handleTouch() {
  static bool wasPressed = false;

  int x, y;
  bool pressed = getTouchXY(x, y);

  if (pressed && !wasPressed && activePlatformCount > 1) {
    if (x < 160) previousPlatform();
    else         nextPlatform();
  }

  wasPressed = pressed;
}

// ------------------------------------------------------------
// Wi-Fi
// ------------------------------------------------------------

bool connectWiFi(unsigned long timeoutMs = 15000) {
  if (WiFi.status() == WL_CONNECTED) return true;

  if (!nonEmpty(WIFI_SSID)) {
    Serial.println("WiFi: no SSID configured");
    return false;
  }

  Serial.print("WiFi: connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);

    if (millis() - start >= timeoutMs) {
      Serial.println("WiFi: connection timeout");
      return false;
    }
  }

  Serial.print("WiFi: connected, IP=");
  Serial.println(WiFi.localIP());
  return true;
}

// ------------------------------------------------------------
// YouTube - public channel page, no API key
// ------------------------------------------------------------

// Prevent the EU consent interstitial. This is the same basic technique
// used by established YouTube download/parser projects for unauthenticated
// public page requests.
static const char YOUTUBE_CONSENT_COOKIE[] =
  "SOCS=CAI; CONSENT=YES+";


bool youtubeConfigured() {
  return nonEmpty(YOUTUBE_CHANNEL);
}

String youtubeChannelUrl() {
  String channel = YOUTUBE_CHANNEL;
  String url;

  if (channel.startsWith("http://") || channel.startsWith("https://")) {
    url = channel;
  } else if (channel.startsWith("UC")) {
    url = "https://www.youtube.com/channel/";
    url += channel;
  } else {
    if (!channel.startsWith("@")) channel = "@" + channel;
    url = "https://www.youtube.com/";
    url += channel;
  }

  // Prefer the public About page because it is focused on channel metadata.
  int q = url.indexOf('?');
  if (url.indexOf("/about") < 0) {
    if (q >= 0) {
      url = url.substring(0, q) + "/about" + url.substring(q);
    } else {
      url += "/about";
    }
  }

  // Ask YouTube for an English/US representation so K/M/B parsing
  // remains independent of the user's browser language.
  url += (url.indexOf('?') >= 0) ? "&hl=en&gl=US" : "?hl=en&gl=US";

  return url;
}

// Read a JSON-style quoted string from the HTTP stream.
// This is sufficient for the short subscriber text we need.
bool readQuotedValue(WiFiClient *stream, String &value, unsigned long timeoutMs = 3000) {
  value = "";
  bool escaped = false;
  unsigned long lastData = millis();

  while (millis() - lastData < timeoutMs) {
    while (stream->available()) {
      char c = (char)stream->read();
      lastData = millis();

      if (escaped) {
        // We only need normal display text. Keep common escaped chars.
        if (c == '"' || c == '\\' || c == '/') value += c;
        else if (c == 'n' || c == 'r' || c == 't') value += ' ';
        escaped = false;
        continue;
      }

      if (c == '\\') {
        escaped = true;
        continue;
      }

      if (c == '"') return value.length() > 0;

      value += c;

      if (value.length() > 96) return false;
    }

    if (!stream->connected()) break;
    delay(1);
  }

  return false;
}

// Stream-scan the HTML instead of storing the whole YouTube page in RAM.
// We first locate "subscriberCountText" and then accept either a
// "simpleText" or a "text" field shortly after it.
bool looksLikeSubscriberNumber(const String &s) {
  if (s.length() == 0 || s.length() > 24) return false;

  bool haveDigit = false;

  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);

    if (c >= '0' && c <= '9') {
      haveDigit = true;
      continue;
    }

    if (c == '.' || c == ',' ||
        c == 'K' || c == 'k' ||
        c == 'M' || c == 'm' ||
        c == 'B' || c == 'b') {
      continue;
    }

    return false;
  }

  return haveDigit;
}

// Search the received HTML for the visible subscriber text itself instead of
// relying on a particular YouTube JSON field name.
//
// This deliberately follows the same basic idea as the old COVID tracker:
// read the page continuously, keep only a small overlapping text window,
// search that window for a stable piece of visible text, and extract the
// value next to it.
bool isDigitChar(char c) {
  return c >= '0' && c <= '9';
}

bool findYouTubeSubscriberText(WiFiClient *stream, String &subscriberText) {
  const size_t MAX_SCAN_BYTES = 1024UL * 1024UL;

  const char followMarker[] =
    "\"interactionType\":{\"@type\":\"FollowAction\"}";
  const char countMarker[] =
    "\"userInteractionCount\":\"";

  String window;
  window.reserve(512);

  bool followBlockFound = false;
  size_t bytesAfterFollow = 0;
  size_t bytesRead = 0;
  size_t nextProgress = 64UL * 1024UL;
  unsigned long lastData = millis();

  Serial.println("YouTube: scanning structured metadata...");

  while ((stream->connected() || stream->available()) &&
         bytesRead < MAX_SCAN_BYTES) {

    while (stream->available() && bytesRead < MAX_SCAN_BYTES) {
      char c = (char)stream->read();
      bytesRead++;
      lastData = millis();

      window += c;
      if (window.length() > 512) {
        window.remove(0, window.length() - 512);
      }

      if (bytesRead >= nextProgress) {
        Serial.print("YouTube: scanned ");
        Serial.print(bytesRead / 1024);
        Serial.println(" kB");
        nextProgress += 64UL * 1024UL;
      }

      // Large HTML responses can keep stream->available() true for a long
      // time. Yield regularly so the scheduler/watchdog and UI get CPU time.
      if ((bytesRead & 0x0FFF) == 0) {
        vTaskDelay(1);
      }

      if (!followBlockFound) {
        if (window.endsWith(followMarker)) {
          followBlockFound = true;
          bytesAfterFollow = 0;
          window = "";

          Serial.print("YouTube: FollowAction found at byte ");
          Serial.println(bytesRead);
        }

        continue;
      }

      bytesAfterFollow++;

      if (window.endsWith(countMarker)) {
        String countText;

        if (!readQuotedValue(stream, countText)) {
          Serial.println("YouTube: userInteractionCount could not be read");
          return false;
        }

        countText.trim();

        if (countText.length() == 0) {
          Serial.println("YouTube: empty userInteractionCount");
          return false;
        }

        subscriberText = countText + " subscribers";

        Serial.print("YouTube: userInteractionCount = ");
        Serial.println(countText);

        return true;
      }

      // The count should be close to FollowAction. If not, resume searching.
      if (bytesAfterFollow > 1200) {
        followBlockFound = false;
        bytesAfterFollow = 0;
        window = "";
      }
    }

    if (millis() - lastData > 5000) {
      Serial.println("YouTube: stream timeout while scanning");
      break;
    }

    delay(1);
  }

  Serial.print("YouTube: scan finished after ");
  Serial.print(bytesRead / 1024);
  Serial.println(" kB");

  if (bytesRead >= MAX_SCAN_BYTES) {
    Serial.println("YouTube: scan limit reached");
  }

  Serial.println("YouTube: FollowAction/userInteractionCount not found");
  return false;
}

bool parsePublicCount(const String &input, uint32_t &value) {
  String s = input;
  s.trim();

  // Typical values: "842 subscribers", "1.23K subscribers", "12.3M subscribers"
  int space = s.indexOf(' ');
  if (space > 0) s = s.substring(0, space);

  s.replace(",", "");
  s.trim();

  if (s.length() == 0) return false;

  char suffix = s.charAt(s.length() - 1);
  double multiplier = 1.0;

  if (suffix == 'K' || suffix == 'k') {
    multiplier = 1000.0;
    s.remove(s.length() - 1);
  } else if (suffix == 'M' || suffix == 'm') {
    multiplier = 1000000.0;
    s.remove(s.length() - 1);
  } else if (suffix == 'B' || suffix == 'b') {
    multiplier = 1000000000.0;
    s.remove(s.length() - 1);
  }

  char *endPtr = nullptr;
  double n = strtod(s.c_str(), &endPtr);

  if (endPtr == s.c_str() || n < 0.0) return false;

  double expanded = n * multiplier;
  if (expanded > 4294967295.0) return false;

  value = (uint32_t)(expanded + 0.5);
  return true;
}

bool fetchYouTubePage(const String &url, String &subscriberText) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(10000);

  Serial.print("YouTube: GET ");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("YouTube: https.begin failed");
    return false;
  }

  const char *headerKeys[] = {"Location"};
  https.collectHeaders(headerKeys, 1);

  https.setUserAgent(
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 Chrome/130 Safari/537.36"
  );
  https.addHeader("Accept-Language", "en-US,en;q=0.9");
  https.addHeader("Accept-Encoding", "identity");
  https.addHeader("Cookie", YOUTUBE_CONSENT_COOKIE);

  // Do NOT use HTTPClient automatic redirect handling here.
  // Some ESP32 core versions can hang during HTTPS redirects.
  https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  Serial.println("YouTube: sending request...");
  int httpCode = https.GET();

  Serial.print("YouTube: HTTP ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
      httpCode == HTTP_CODE_FOUND ||
      httpCode == HTTP_CODE_SEE_OTHER ||
      httpCode == HTTP_CODE_TEMPORARY_REDIRECT ||
      httpCode == HTTP_CODE_PERMANENT_REDIRECT) {

    String location = https.header("Location");
    Serial.print("YouTube: redirect -> ");
    Serial.println(location);
    https.end();

    if (location.length() == 0) return false;

    // YouTube may return a relative redirect.
    if (location.startsWith("/")) {
      location = "https://www.youtube.com" + location;
    }

    // Follow exactly one redirect with a fresh TLS connection.
    WiFiClientSecure client2;
    client2.setInsecure();
    client2.setTimeout(10);

    HTTPClient https2;
    https2.setConnectTimeout(8000);
    https2.setTimeout(10000);

    Serial.print("YouTube: GET ");
    Serial.println(location);

    if (!https2.begin(client2, location)) {
      Serial.println("YouTube: redirected https.begin failed");
      return false;
    }

    https2.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    https2.setUserAgent(
      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
      "AppleWebKit/537.36 Chrome/130 Safari/537.36"
    );
    https2.addHeader("Accept-Language", "en-US,en;q=0.9");
    https2.addHeader("Accept-Encoding", "identity");
    https2.addHeader("Cookie", YOUTUBE_CONSENT_COOKIE);

    Serial.println("YouTube: sending redirected request...");
    int code2 = https2.GET();

    Serial.print("YouTube: redirected HTTP ");
    Serial.println(code2);

    if (code2 != HTTP_CODE_OK) {
      https2.end();
      return false;
    }

    WiFiClient *stream2 = https2.getStreamPtr();
    bool found2 = findYouTubeSubscriberText(stream2, subscriberText);
    https2.end();
    return found2;
  }

  if (httpCode != HTTP_CODE_OK) {
    https.end();
    return false;
  }

  WiFiClient *stream = https.getStreamPtr();
  bool found = findYouTubeSubscriberText(stream, subscriberText);
  https.end();

  return found;
}

bool updateYouTubeStats() {
  platformState[PLATFORM_YOUTUBE].configured = youtubeConfigured();

  if (!platformState[PLATFORM_YOUTUBE].configured) {
    platformState[PLATFORM_YOUTUBE].validated = false;
    platformState[PLATFORM_YOUTUBE].hasValue = false;
    Serial.println("YouTube: channel not configured");
    return false;
  }

  if (!connectWiFi()) {
    Serial.println("YouTube: WiFi unavailable");
    return false;
  }

  String subscriberText;
  String url = youtubeChannelUrl();

  if (!fetchYouTubePage(url, subscriberText)) {
    Serial.println("YouTube: subscriber count not found");
    return false;
  }

  Serial.print("YouTube: public subscriber text = ");
  Serial.println(subscriberText);

  uint32_t subscribers = 0;
  if (!parsePublicCount(subscriberText, subscribers)) {
    Serial.println("YouTube: subscriber count parse failed");
    return false;
  }

  stats[PLATFORM_YOUTUBE].value = subscribers;
  platformState[PLATFORM_YOUTUBE].validated = true;
  platformState[PLATFORM_YOUTUBE].hasValue = true;
  platformState[PLATFORM_YOUTUBE].lastSuccess = millis();

  Serial.print("YouTube: subscribers=");
  Serial.println(subscribers);

  return true;
}


// ------------------------------------------------------------
// Instagram - HonestHook API
// ------------------------------------------------------------

bool honestHookConfigured() {
  return nonEmpty(HONESTHOOK_API_KEY);
}

bool instagramConfigured() {
  return nonEmpty(INSTAGRAM_USERNAME) && honestHookConfigured();
}

String instagramProfileUrl() {
  String user = INSTAGRAM_USERNAME;
  if (user.startsWith("@")) user.remove(0, 1);

  String url = "https://honesthook.com/api/v1/instagram/profile?handle=";
  url += user;
  return url;
}

bool parseJsonUnsignedField(const String &json, const char *field, uint32_t &value) {
  String marker = "\"";
  marker += field;
  marker += "\"";

  int p = json.indexOf(marker);
  if (p < 0) return false;

  p = json.indexOf(':', p + marker.length());
  if (p < 0) return false;

  p++;
  while (p < (int)json.length() &&
         (json.charAt(p) == ' ' || json.charAt(p) == '\t' ||
          json.charAt(p) == '\r' || json.charAt(p) == '\n')) {
    p++;
  }

  int begin = p;
  while (p < (int)json.length() && json.charAt(p) >= '0' && json.charAt(p) <= '9') {
    p++;
  }

  if (p == begin) return false;
  value = strtoul(json.substring(begin, p).c_str(), nullptr, 10);
  return true;
}

bool updateInstagramStats() {
  platformState[PLATFORM_INSTAGRAM].configured = instagramConfigured();

  if (!nonEmpty(INSTAGRAM_USERNAME)) {
    platformState[PLATFORM_INSTAGRAM].validated = false;
    platformState[PLATFORM_INSTAGRAM].hasValue = false;
    Serial.println("Instagram: username not configured");
    return false;
  }

  if (!honestHookConfigured()) {
    platformState[PLATFORM_INSTAGRAM].validated = false;
    platformState[PLATFORM_INSTAGRAM].hasValue = false;
    Serial.println("Instagram: HonestHook API key not configured");
    return false;
  }

  if (!connectWiFi()) {
    Serial.println("Instagram: WiFi unavailable");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(15000);

  String url = instagramProfileUrl();
  Serial.print("Instagram/HonestHook: GET ");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("Instagram/HonestHook: https.begin failed");
    return false;
  }

  String auth = "Bearer ";
  auth += HONESTHOOK_API_KEY;
  https.addHeader("Authorization", auth);
  https.addHeader("Cache-Control", "stale-if-error=86400");
  https.addHeader("Accept", "application/json");
  https.setUserAgent("AZ-Touch-Social-Tracker/0.8");

  Serial.println("Instagram/HonestHook: sending request...");
  int httpCode = https.GET();
  Serial.print("Instagram/HonestHook: HTTP ");
  Serial.println(httpCode);

  String body = https.getString();
  https.end();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Instagram/HonestHook: response = ");
    Serial.println(body);
    return false;
  }

  Serial.print("Instagram/HonestHook: response bytes = ");
  Serial.println(body.length());

  uint32_t followers = 0;
  if (!parseJsonUnsignedField(body, "followers", followers)) {
    Serial.println("Instagram/HonestHook: followers field not found");
    Serial.println(body);
    return false;
  }

  stats[PLATFORM_INSTAGRAM].value = followers;
  platformState[PLATFORM_INSTAGRAM].validated = true;
  platformState[PLATFORM_INSTAGRAM].hasValue = true;
  platformState[PLATFORM_INSTAGRAM].lastSuccess = millis();

  Serial.print("Instagram/HonestHook: followers = ");
  Serial.println(followers);

  uint32_t creditsRemaining = 0;
  if (parseJsonUnsignedField(body, "credits_remaining", creditsRemaining)) {
    Serial.print("Instagram/HonestHook: credits remaining = ");
    Serial.println(creditsRemaining);
  }

  if (body.indexOf("\"stale\":true") >= 0 || body.indexOf("\"stale\": true") >= 0) {
    Serial.println("Instagram/HonestHook: stale cached value used");
  }

  return true;
}


// ------------------------------------------------------------
// Threads - HonestHook API
// ------------------------------------------------------------

bool threadsConfigured() {
  return nonEmpty(THREADS_USERNAME) && honestHookConfigured();
}

String threadsProfileUrl() {
  String user = THREADS_USERNAME;

  if (user.startsWith("@")) {
    user.remove(0, 1);
  }

  String url = "https://honesthook.com/api/v1/threads/profile?handle=";
  url += user;

  return url;
}

bool updateThreadsStats() {
  platformState[PLATFORM_THREADS].configured = threadsConfigured();

  if (!nonEmpty(THREADS_USERNAME)) {
    platformState[PLATFORM_THREADS].validated = false;
    platformState[PLATFORM_THREADS].hasValue = false;
    Serial.println("Threads: username not configured");
    return false;
  }

  if (!honestHookConfigured()) {
    platformState[PLATFORM_THREADS].validated = false;
    platformState[PLATFORM_THREADS].hasValue = false;
    Serial.println("Threads: HonestHook API key not configured");
    return false;
  }

  if (!connectWiFi()) {
    Serial.println("Threads: WiFi unavailable");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(15000);

  String url = threadsProfileUrl();

  Serial.print("Threads/HonestHook: GET ");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("Threads/HonestHook: https.begin failed");
    return false;
  }

  String auth = "Bearer ";
  auth += HONESTHOOK_API_KEY;
  https.addHeader("Authorization", auth);
  https.addHeader("Cache-Control", "stale-if-error=86400");
  https.addHeader("Accept", "application/json");
  https.setUserAgent("AZ-Touch-Social-Tracker/0.9");

  Serial.println("Threads/HonestHook: sending request...");
  int httpCode = https.GET();

  Serial.print("Threads/HonestHook: HTTP ");
  Serial.println(httpCode);

  String body = https.getString();
  https.end();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Threads/HonestHook: response = ");
    Serial.println(body);
    return false;
  }

  Serial.print("Threads/HonestHook: response bytes = ");
  Serial.println(body.length());

  if (body.indexOf("\"success\":true") < 0 &&
      body.indexOf("\"success\": true") < 0) {
    Serial.println("Threads/HonestHook: success=true not found");
    Serial.println(body);
    return false;
  }

  uint32_t followers = 0;
  if (!parseJsonUnsignedField(body, "followers", followers)) {
    Serial.println("Threads/HonestHook: followers field not found");
    Serial.println(body);
    return false;
  }

  stats[PLATFORM_THREADS].value = followers;
  platformState[PLATFORM_THREADS].validated = true;
  platformState[PLATFORM_THREADS].hasValue = true;
  platformState[PLATFORM_THREADS].lastSuccess = millis();

  Serial.print("Threads/HonestHook: followers = ");
  Serial.println(followers);

  uint32_t creditsRemaining = 0;
  if (parseJsonUnsignedField(body, "credits_remaining", creditsRemaining)) {
    Serial.print("Threads/HonestHook: credits remaining = ");
    Serial.println(creditsRemaining);
  }

  if (body.indexOf("\"stale\":true") >= 0 ||
      body.indexOf("\"stale\": true") >= 0) {
    Serial.println("Threads/HonestHook: stale cached value used");
  }

  return true;
}


// ------------------------------------------------------------
// TikTok - direct public profile page
// ------------------------------------------------------------

bool tiktokConfigured() {
  return nonEmpty(TIKTOK_USERNAME);
}

String tiktokProfileUrl() {
  String user = TIKTOK_USERNAME;
  if (user.startsWith("@")) {
    user.remove(0, 1);
  }

  String url = "https://www.tiktok.com/@";
  url += user;
  return url;
}

bool findTikTokFollowerCount(WiFiClient *stream, uint32_t &followers) {
  const size_t MAX_SCAN_BYTES = 1024UL * 1024UL;
  const char marker[] = "\"followerCount\":";

  String window;
  window.reserve(256);

  size_t bytesRead = 0;
  size_t nextProgress = 64UL * 1024UL;
  unsigned long lastData = millis();

  Serial.println("TikTok: scanning public profile HTML...");

  while ((stream->connected() || stream->available()) &&
         bytesRead < MAX_SCAN_BYTES) {

    while (stream->available() && bytesRead < MAX_SCAN_BYTES) {
      char c = (char)stream->read();
      bytesRead++;
      lastData = millis();

      window += c;
      if (window.length() > 256) {
        window.remove(0, window.length() - 256);
      }

      if (bytesRead >= nextProgress) {
        Serial.print("TikTok: scanned ");
        Serial.print(bytesRead / 1024);
        Serial.println(" kB");
        nextProgress += 64UL * 1024UL;
      }

      if ((bytesRead & 0x0FFF) == 0) {
        vTaskDelay(1);
      }

      if (window.endsWith(marker)) {
        Serial.print("TikTok: followerCount marker found at byte ");
        Serial.println(bytesRead);

        String digits;
        unsigned long valueTimeout = millis();

        while (millis() - valueTimeout < 3000) {
          while (stream->available()) {
            char d = (char)stream->read();
            bytesRead++;
            valueTimeout = millis();

            if (d >= '0' && d <= '9') {
              digits += d;
              if (digits.length() > 12) return false;
            } else if (digits.length() > 0) {
              followers = strtoul(digits.c_str(), nullptr, 10);
              Serial.print("TikTok: followerCount = ");
              Serial.println(followers);
              return true;
            } else if (d == ' ' || d == '\t' || d == '\r' || d == '\n') {
              continue;
            } else {
              // Unexpected character before number
              return false;
            }
          }

          if (!stream->connected()) break;
          delay(1);
        }

        return false;
      }
    }

    if (millis() - lastData > 5000) {
      Serial.println("TikTok: stream timeout while scanning");
      break;
    }

    delay(1);
  }

  Serial.print("TikTok: scan finished after ");
  Serial.print(bytesRead / 1024);
  Serial.println(" kB");

  if (bytesRead >= MAX_SCAN_BYTES) {
    Serial.println("TikTok: scan limit reached");
  }

  Serial.println("TikTok: followerCount not found");
  return false;
}

bool updateTikTokStats() {
  platformState[PLATFORM_TIKTOK].configured = tiktokConfigured();

  if (!platformState[PLATFORM_TIKTOK].configured) {
    platformState[PLATFORM_TIKTOK].validated = false;
    platformState[PLATFORM_TIKTOK].hasValue = false;
    Serial.println("TikTok: username not configured");
    return false;
  }

  if (!connectWiFi()) {
    Serial.println("TikTok: WiFi unavailable");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(15000);
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  String url = tiktokProfileUrl();

  Serial.print("TikTok: GET ");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("TikTok: https.begin failed");
    return false;
  }

  https.setUserAgent(
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 Chrome/130 Safari/537.36"
  );
  https.addHeader("Accept-Language", "en-US,en;q=0.9");
  https.addHeader("Accept-Encoding", "identity");

  Serial.println("TikTok: sending request...");
  int httpCode = https.GET();

  Serial.print("TikTok: HTTP ");
  Serial.println(httpCode);

  if (httpCode != HTTP_CODE_OK) {
    String body = https.getString();
    Serial.print("TikTok: response = ");
    Serial.println(body.substring(0, min((int)body.length(), 300)));
    https.end();
    return false;
  }

  WiFiClient *stream = https.getStreamPtr();
  uint32_t followers = 0;
  bool found = findTikTokFollowerCount(stream, followers);
  https.end();

  if (!found) return false;

  stats[PLATFORM_TIKTOK].value = followers;
  platformState[PLATFORM_TIKTOK].validated = true;
  platformState[PLATFORM_TIKTOK].hasValue = true;
  platformState[PLATFORM_TIKTOK].lastSuccess = millis();

  return true;
}


// ------------------------------------------------------------
// Pinterest - HonestHook API
// ------------------------------------------------------------

bool pinterestConfigured() {
  return nonEmpty(PINTEREST_USERNAME) && honestHookConfigured();
}

String pinterestProfileUrl() {
  String user = PINTEREST_USERNAME;

  if (user.startsWith("@")) {
    user.remove(0, 1);
  }

  String url = "https://honesthook.com/api/v1/pinterest/profile?handle=";
  url += user;

  return url;
}

bool updatePinterestStats() {
  platformState[PLATFORM_PINTEREST].configured = pinterestConfigured();

  if (!nonEmpty(PINTEREST_USERNAME)) {
    platformState[PLATFORM_PINTEREST].validated = false;
    platformState[PLATFORM_PINTEREST].hasValue = false;
    Serial.println("Pinterest: username not configured");
    return false;
  }

  if (!honestHookConfigured()) {
    platformState[PLATFORM_PINTEREST].validated = false;
    platformState[PLATFORM_PINTEREST].hasValue = false;
    Serial.println("Pinterest: HonestHook API key not configured");
    return false;
  }

  if (!connectWiFi()) {
    Serial.println("Pinterest: WiFi unavailable");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient https;
  https.setConnectTimeout(8000);
  https.setTimeout(15000);

  String url = pinterestProfileUrl();

  Serial.print("Pinterest/HonestHook: GET ");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("Pinterest/HonestHook: https.begin failed");
    return false;
  }

  String auth = "Bearer ";
  auth += HONESTHOOK_API_KEY;
  https.addHeader("Authorization", auth);
  https.addHeader("Cache-Control", "stale-if-error=86400");
  https.addHeader("Accept", "application/json");
  https.setUserAgent("AZ-Touch-Social-Tracker/1.2");

  Serial.println("Pinterest/HonestHook: sending request...");
  int httpCode = https.GET();

  Serial.print("Pinterest/HonestHook: HTTP ");
  Serial.println(httpCode);

  String body = https.getString();
  https.end();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Pinterest/HonestHook: response = ");
    Serial.println(body);
    return false;
  }

  Serial.print("Pinterest/HonestHook: response bytes = ");
  Serial.println(body.length());

  if (body.indexOf("\"success\":true") < 0 &&
      body.indexOf("\"success\": true") < 0) {
    Serial.println("Pinterest/HonestHook: success=true not found");
    Serial.println(body);
    return false;
  }

  uint32_t followers = 0;

  // HonestHook normally exposes the normalized count as "followers".
  // Keep a couple of fallbacks for service-side naming changes.
  if (!parseJsonUnsignedField(body, "followers", followers) &&
      !parseJsonUnsignedField(body, "follower_count", followers) &&
      !parseJsonUnsignedField(body, "followerCount", followers)) {
    Serial.println("Pinterest/HonestHook: follower field not found");
    Serial.println(body);
    return false;
  }

  stats[PLATFORM_PINTEREST].value = followers;
  platformState[PLATFORM_PINTEREST].validated = true;
  platformState[PLATFORM_PINTEREST].hasValue = true;
  platformState[PLATFORM_PINTEREST].lastSuccess = millis();

  Serial.print("Pinterest/HonestHook: followers = ");
  Serial.println(followers);

  uint32_t creditsRemaining = 0;
  if (parseJsonUnsignedField(body, "credits_remaining", creditsRemaining)) {
    Serial.print("Pinterest/HonestHook: credits remaining = ");
    Serial.println(creditsRemaining);
  }

  if (body.indexOf("\"stale\":true") >= 0 ||
      body.indexOf("\"stale\": true") >= 0) {
    Serial.println("Pinterest/HonestHook: stale cached value used");
  }

  return true;
}

// ------------------------------------------------------------
// API update / configuration
// ------------------------------------------------------------

void initializePlatformConfiguration() {
  for (uint8_t i = 0; i < PLATFORM_COUNT; i++) {
    platformState[i].configured = false;
    platformState[i].validated = false;
    platformState[i].hasValue = false;
    platformState[i].lastSuccess = 0;
  }

  platformState[PLATFORM_YOUTUBE].configured = youtubeConfigured();
  platformState[PLATFORM_INSTAGRAM].configured = instagramConfigured();
  platformState[PLATFORM_THREADS].configured = threadsConfigured();
  platformState[PLATFORM_TIKTOK].configured = tiktokConfigured();
  platformState[PLATFORM_PINTEREST].configured = pinterestConfigured();
}

bool runPlatformUpdate(uint8_t platformId, bool (*updateFunction)(),
                       LoadingStatus status) {
  loadingStatus = status;

  bool hadValue =
    platformState[platformId].validated && platformState[platformId].hasValue;
  uint32_t oldValue = stats[platformId].value;

  bool ok = updateFunction();

  if (ok) {
    uint32_t newValue = stats[platformId].value;

    // Never signal the first value after boot. Only a later increase counts.
    if (hadValue && newValue > oldValue) {
      pendingIncreasePlatform = (int8_t)platformId;

      Serial.print("Follower increase: ");
      Serial.print(stats[platformId].name);
      Serial.print(" ");
      Serial.print(oldValue);
      Serial.print(" -> ");
      Serial.println(newValue);
    }
  }

  platformDataDirty = true;
  return ok;
}

void updateHonestHookPlatforms() {
  runPlatformUpdate(PLATFORM_INSTAGRAM, updateInstagramStats, LOAD_INSTAGRAM);
  vTaskDelay(pdMS_TO_TICKS(100));
  runPlatformUpdate(PLATFORM_THREADS,   updateThreadsStats,   LOAD_THREADS);
  vTaskDelay(pdMS_TO_TICKS(100));
  runPlatformUpdate(PLATFORM_PINTEREST, updatePinterestStats, LOAD_PINTEREST);
}

void updateDirectPlatforms() {
  // Let TLS/network resources from HonestHook settle before opening the
  // considerably larger YouTube response.
  vTaskDelay(pdMS_TO_TICKS(250));
  runPlatformUpdate(PLATFORM_YOUTUBE, updateYouTubeStats, LOAD_YOUTUBE);
  vTaskDelay(pdMS_TO_TICKS(100));
  runPlatformUpdate(PLATFORM_TIKTOK,  updateTikTokStats,  LOAD_TIKTOK);
}

void apiTask(void *parameter) {
  loadingStatus = LOAD_WIFI;
  connectWiFi();

  // Initial load: HonestHook services first, then direct fetches.
  updateHonestHookPlatforms();
  updateDirectPlatforms();

  loadingStatus = LOAD_DONE;
  initialLoadDone = true;
  platformDataDirty = true;

  unsigned long lastHonestHookUpdate = millis();
  unsigned long lastDirectUpdate = millis();

  for (;;) {
    unsigned long now = millis();

    if (now - lastHonestHookUpdate >= HONESTHOOK_UPDATE_TIME) {
      updateHonestHookPlatforms();
      lastHonestHookUpdate = millis();
    }

    if (now - lastDirectUpdate >= DIRECT_UPDATE_TIME) {
      updateDirectPlatforms();
      lastDirectUpdate = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void processBackgroundUpdates() {
  if (platformDataDirty) {
    platformDataDirty = false;

    uint8_t oldPlatform = 255;
    if (activePlatformCount > 0 && currentActiveIndex < activePlatformCount) {
      oldPlatform = activePlatforms[currentActiveIndex];
    }

    buildActivePlatformList();

    // Keep the currently displayed service where possible.
    if (oldPlatform != 255) {
      for (uint8_t i = 0; i < activePlatformCount; i++) {
        if (activePlatforms[i] == oldPlatform) {
          currentActiveIndex = i;
          break;
        }
      }
    }

    if (initialLoadDone) {
      redrawCurrentPage();
    }
  }

  int8_t increased = pendingIncreasePlatform;
  if (increased >= 0) {
    pendingIncreasePlatform = -1;

    // The platform is already validated, therefore it should be present
    // in activePlatforms after processBackgroundUpdates rebuilt the list.
    if (selectPlatform((uint8_t)increased)) {
      playFollowerIncreaseSignal();
    }
  }
}

// ------------------------------------------------------------
// setup / loop
// ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(TFT_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  // Explicit SPI initialization, matching the proven Fuel Tracker setup.
  SPI.begin();

  tft.begin();
  tft.setRotation(TFT_ROTATION);
  tft.setTextWrap(false);

  touch.begin();

  // TFT and XPT2046 share the SPI bus. Keep the touch controller
  // deselected whenever the TFT RAM is read back.
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);

  digitalWrite(TFT_LED, LOW);

  initializePlatformConfiguration();

  Serial.println();
  Serial.println("AZ-Touch Social Tracker 2026 - v1.4");
  Serial.println("Press 's' for screenshot dump.");

  drawLoadingScreen();

  // Network/API work runs separately so the loading animation, touch,
  // screenshot handler and WiFi indicator remain responsive.
  xTaskCreatePinnedToCore(
    apiTask,
    "socialApi",
    12288,
    nullptr,
    1,
    &apiTaskHandle,
    1
  );

  lastPageChange = millis();
}

void loop() {
  static bool loadingScreenActive = true;
  static bool lastWiFiConnected = false;
  static unsigned long lastWiFiCheck = 0;

  if (!initialLoadDone) {
    updateLoadingAnimation();

    if (millis() - lastWiFiCheck >= 500) {
      lastWiFiCheck = millis();
      bool connected = (WiFi.status() == WL_CONNECTED);
      if (connected != lastWiFiConnected) {
        lastWiFiConnected = connected;
        drawWiFiStatusIcon();
      }
    }

    delay(5);
    return;
  }

  if (loadingScreenActive) {
    loadingScreenActive = false;
    processBackgroundUpdates();
    buildActivePlatformList();
    redrawCurrentPage();
    lastPageChange = millis();
  }

  handleTouch();
  processBackgroundUpdates();

  if (activePlatformCount > 1 &&
      millis() - lastPageChange >= PAGE_TIME) {
    nextPlatform();
  }

  if (millis() - lastWiFiCheck >= 1000) {
    lastWiFiCheck = millis();
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected != lastWiFiConnected) {
      lastWiFiConnected = connected;
      drawWiFiStatusIcon();
    }
  }

  delay(10);
}
