# ESP32-social-tracker-with-2.4inch-TFT
ESP32 social media tracker for Facebook, Instagram and Youtube for ArduiTouch ESP (with 2.4 inch touchscreen ILI9341)

![My image](https://i.pinimg.com/564x/5d/10/40/5d1040267a2923ab269fd2b01193e2ec.jpg)

## Introduction

This example for the AZ-Touch MOD(https://www.hwhardsoft.de/english/projects/arduitouch-esp/) or AZ-Touch Feather (e.g., with the Huzzah32 board) shows how to build a small social media follower counter using the AZ-Touch kits. Currently supported platforms include YouTube, TikTok, Threads, Instagram, and Pinterest. To make setup as easy as possible for the user, we have largely avoided using APIs. Instead, we extract the data for YouTube and TikTok by scraping the corresponding websites.
Instagram and Threads protect their pages against scraping, so we use Honesthook to access the data. 

## Hardware settings


## WiFi settings

Enter your WiFi SSID & password in the fields in the WiFi section: 
#define WIFI_SSID "xxxxxx"    // Enter your SSID here 
#define WIFI_PASS "xxxxx"    // Enter your WiFi password here 


## Facebook settings

Enter the Facebook Page ID, Access Token and Fingerprint in the Facebook section:
#define PAGE_ID "YOUR_PAGE_ID" 
#define ACCESS_TOKEN "YOUR_ACCESS_TOKEN" 
// graph.facebook.com SHA1 fingerprint 
const char* facebookGraphFingerPrint = "YOUR_FINGER_PRINT"; 

How to get the Page ID, Access Token and Fingerprint:
• Follow the steps on https://developers.facebook.com/docs/pages/getting-started to create an APP 
• After the app is created, go to the graph explorer. 
• Up the top right change the application to the new one that you created 
• Click "Get Token" and then click "Get User Access Token" 
• Check the "User_Friends" option, click get access token and authenticated the applicaiton with your account. 
• The key that appears the in the bar can be used with the library. 
• Click on (https://developers.facebook.com/apps), 
• click on the app you created. Your consumer ID and consumer secret are available on this page. You will need to this to extend your API key, which you can do using the library


## Instagram settings

You have only to enter your Instagram name in the field below: 
String Instagram_userName = "YOUR_USERNAME";    


## Google settings

Enter the Youtube API Key and Channel ID in the Youtube section: 
#define API_KEY "YOUR_API_KEY"  // your google apps API Token 
#define CHANNEL_ID "YOUR_CHANNEL_ID" // makes up the url of channel 

It's necessary needed to create a Google Apps API key:
• Create an application (https://console.developers.google.com 
• On the API Manager section, go to "Credentials" and create a new API key 
• Enable your application to communicate the YouTube Api (https://console.developers.google.com/apis/api/youtube) 
• Make sure the following URL works for you in your browser (Change the key at the end!): https://www.googleapis.com/youtube/v3/channels? part=statistics&id=UCu7_D0o48KbfhpEohoP7YSQ&key=PutYourNewlyGeneratedKeyHere
 

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
