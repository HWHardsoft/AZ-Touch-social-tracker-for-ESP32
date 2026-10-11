# ESP32 based Follower Counter for AZ-Touch


![My image](https://github.com/HWHardsoft/ESP32-social-tracker-with-2.4inch-TFT/blob/master/graphics/AZ-Touch-Tiktok.jpg)

## Introduction

This example for the [AZ-Touch MOD](https://www.hwhardsoft.de/english/projects/arduitouch-esp/) or [AZ-Touch Feather](https://www.hwhardsoft.de/english/projects/az-touch-feather/) (e.g., with the Huzzah32 board) shows how to build a small social media follower counter using the AZ-Touch kits. Currently supported platforms include YouTube, TikTok, Threads, Instagram, and Pinterest. To make setup as easy as possible for the user, we have largely avoided using APIs. Instead, we extract the data for YouTube and TikTok by scraping the corresponding websites.
Instagram and Threads protect their pages against scraping, so we use Honesthook to access the data. 

## Hardware settings

First, you must specify in the hardwareconfig.h file which AZ-Touch kit or platform is being used:
```cpp
// Select ONE controller
#define AZTOUCH_ESP32_DEVKIT
//#define AZTOUCH_FEATHER_HUZZAH32
```


## WiFi settings

Enter your Wi-Fi SSID and password in the fields in the “Wi-Fi” section of credentials.h: 
```cpp
// Wi-Fi
static const char WIFI_SSID[]     = "";
static const char WIFI_PASSWORD[] = ""; 
```

## Youtube and Tiktok settings

The data for YouTube and TikTok is obtained by scraping the respective websites. Therefore, you only need to enter the username of the relevant account in credentials.h . Passwords or channel IDs are not required. 
```cpp
static const char YOUTUBE_CHANNEL[] = "";

// Public TikTok profile name, without @
static const char TIKTOK_USERNAME[] = ""; 
```

## Instagram, Threads and Pinterest

To find out the follower counts for Instagram, Threads, and Pinterest, we use the Honeyhook service. This service also allows us to retrieve a wide range of other social media data from various providers without having to use each provider’s API. Honeyhook offers free access for up to 1,000 retrievals per month. (If you retrieve data for the three services only once every 6 hours, that comes to 558 retrievals per month.)

First, enter your personal Honeyhook API key in the `credentials.h` file:
```cpp
// HonestHook API key, e.g. "hk_live_..."
static const char HONESTHOOK_API_KEY[] = "";
```

In addition, the usernames for the three social media channels must also be provided:
```cpp
// Public Instagram profile name, without @
// Example: "hwhardsoft"
static const char INSTAGRAM_USERNAME[] = "";

// Public Threads profile name, without @
static const char THREADS_USERNAME[] = "";

// Public Pinterest profile name, without @
static const char PINTEREST_USERNAME[] = "";
``` 

# License

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA 
