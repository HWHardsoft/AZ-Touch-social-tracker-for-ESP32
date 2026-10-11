#pragma once

/*
 * Local configuration for the Social Tracker.
 *
 * Do not publish this file with your real Wi-Fi password.
 */

// Wi-Fi
static const char WIFI_SSID[]     = "";
static const char WIFI_PASSWORD[] = "";

/*
 * Public YouTube channel identifier.
 *
 * Examples:
 *   "@HWHardsoft"
 *   "https://www.youtube.com/@HWHardsoft"
 *   "UCxxxxxxxxxxxxxxxxxxxxxx"
 *
 * No Google Cloud project and no YouTube API key are required.
 */
static const char YOUTUBE_CHANNEL[] = "";



// HonestHook API key, e.g. "hk_live_..."
static const char HONESTHOOK_API_KEY[] = "";

// Public Instagram profile name, without @
// Example: "hwhardsoft"
static const char INSTAGRAM_USERNAME[] = "";


// Public Threads profile name, without @
static const char THREADS_USERNAME[] = "";


// Public TikTok profile name, without @
static const char TIKTOK_USERNAME[] = "";


// Public Pinterest profile name, without @
static const char PINTEREST_USERNAME[] = "";


// Update intervals
// HonestHook services: Instagram, Threads, Pinterest
static const unsigned long HONESTHOOK_UPDATE_MINUTES = 360;  // 6 hours

// Direct services: YouTube, TikTok
static const unsigned long DIRECT_UPDATE_MINUTES = 30;       // 30 minutes
