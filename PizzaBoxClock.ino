/*  £5 Pizza Box Clock Version 0.1
    Copyright (C) 2018 Gavin Smalley

    Based on RGBDigit by Ralph Crützen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/


// --- --- DEFINES --- DEFINES --- DEFINES --- --- //

#define DIGITS_PIN D2
#define COLON_PIN D3
#define BUTTON_PIN D7

#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWD "YOUR_PASSWORD"

#define DIGITS 4
#define SEGMENTS_PER_DIGIT 7

#define NTP_SERVER "pool.ntp.org"
#define NTP_POLL_INTERVAL 63

#define ONBOARDLED 2

#define MAX_DISPLAY_MODES 2

// --- --- INCLUDES --- INCLUDES --- INCLUDES --- --- //

#include <Wire.h>
#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>  // https://github.com/adafruit/Adafruit_NeoPixel
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include <NtpClientLib.h>       //https://github.com/gmag11/NtpClient
#include <RGBDigit.h>           //https://github.com/ralphcrutzen/RGBDigit
#include <Ticker.h>
#include <OneButton.h>          //https://github.com/mathertel/OneButton

// --- --- DEFAULTS --- DEFAULTS --- DEFAULTS --- --- //

volatile int brightness = 128;

//initial digit colour

volatile byte red = 0x45;
volatile byte green = 0x2F;
volatile byte blue = 0x68;

//colour for colon when internet is connected

byte colonConnectedRed = 0x00;
byte colonConnectedGreen = 0xFF;
byte colonConnectedBlue = 0x00;

//colour for colon when internet is disctonnected

byte colonNoInternetRed = 0xFF;
byte colonNoInternetGreen = 0x00;
byte colonNoInternetBlue = 0x00;

int8_t timeZone = 0;
int8_t minutesTimeZone = 0;

// --- --- CONTROLS --- CONTROLS --- CONTROLS --- --- //

bool wifiFirstConnected = false;
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

volatile bool isInternetConnection = false;
volatile bool isColonOn = false;

byte colonRed;
byte colonGreen;
byte colonBlue;

int digitOne = 0;
int digitTwo = 0;
int digitThree = 0;
int digitFour = 0;

int displayMode = 0;
int lastDisplayMode = 1;

// --- --- INITIALISATIONS --- INITIALISATIONS --- INITIALISATIONS --- --- //

RGBDigit digitString(DIGITS, DIGITS_PIN, NEO_BRG + NEO_KHZ800, SEGMENTS_PER_DIGIT);  // uses another pin; 6 in this example

Adafruit_NeoPixel colon = Adafruit_NeoPixel(1, COLON_PIN, NEO_BRG + NEO_KHZ800);

Ticker colonBlinker;

OneButton button(BUTTON_PIN, 1);

// --- --- FUNCTIONS --- FUNCTIONS --- FUNCTIONS --- --- //

void colonSetColonColour(bool internetConnected) {
  if (internetConnected) {
    colonRed = colonConnectedRed;
    colonGreen = colonConnectedGreen;
    colonBlue = colonConnectedBlue;
  }
  else {
    colonRed = colonNoInternetRed;
    colonGreen = colonNoInternetGreen;
    colonBlue = colonNoInternetBlue;
  }
}

void colonBlink() {
  if (isColonOn) {
    //turn strip off
    colon.setPixelColor(0, colon.Color(0, 0, 0));
    colon.show();
    Serial.println("Colon Off");
    isColonOn = false;
  }
  else {
    //turn strip on
    colon.setPixelColor(0, colon.Color(colonRed, colonGreen, colonBlue));
    colon.show();
    Serial.println("Colon On");
    isColonOn = true;
  }
}

void singleClick() {
  displayMode++;
  if (displayMode == MAX_DISPLAY_MODES) {
    displayMode = 0;
  }
  Serial.print("Button pressed. New mode: ");
  Serial.println(displayMode);
}

void onSTAConnected (WiFiEventStationModeConnected ipInfo) {
  Serial.printf ("Connected to %s\r\n", ipInfo.ssid.c_str ());
  isInternetConnection = true;
}

// Start NTP only after IP network is connected
void onSTAGotIP (WiFiEventStationModeGotIP ipInfo) {
  Serial.printf ("Got IP: %s\r\n", ipInfo.ip.toString ().c_str ());
  Serial.printf ("Connected: %s\r\n", WiFi.status () == WL_CONNECTED ? "yes" : "no");
  //digitalWrite (ONBOARDLED, LOW); // Turn on LED
  wifiFirstConnected = true;
  isInternetConnection = true;
}

// Manage network disconnection
void onSTADisconnected (WiFiEventStationModeDisconnected event_info) {
  Serial.printf ("Disconnected from SSID: %s\n", event_info.ssid.c_str ());
  Serial.printf ("Reason: %d\n", event_info.reason);
  //digitalWrite (ONBOARDLED, HIGH); // Turn off LED
  //NTP.stop(); // NTP sync can be disabled to avoid sync errors
  isInternetConnection = false;
}

void processSyncEvent (NTPSyncEvent_t ntpEvent) {
  if (ntpEvent) {
    Serial.print ("Time Sync error: ");
    if (ntpEvent == noResponse)
      Serial.println ("NTP server not reachable");
    else if (ntpEvent == invalidAddress)
      Serial.println ("Invalid NTP server address");
  } else {
    Serial.print ("Got NTP time: ");
    Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
  }
}

// --- --- SETUP --- SETUP --- SETUP --- --- //

void setup() {
  Serial.begin(115200);

  //Configure Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  button.attachClick(singleClick);
  

  //Disable onboard LED (it's in the box and no-one will see it!)
  pinMode(ONBOARDLED, OUTPUT);
  digitalWrite (ONBOARDLED, HIGH);


  //Set colon color to the disconnected state and start it blinking
  colonSetColonColour(false);
  colon.begin();
  colon.setPixelColor(0, colon.Color(colonRed, colonGreen, colonBlue));
  colon.setBrightness(brightness);
  colon.show();
  isColonOn = true;
  colonBlinker.attach(0.5, colonBlink);

  //Initialise digits and show all 0s
  digitString.begin();
  digitString.clearAll();
  digitString.setColor(red, green, blue);
  digitString.setBrightness(brightness);

  for (int digit = 0; digit <= (DIGITS - 1); digit++) {
    digitString.setDigit(0, digit, red, green, blue); //Color is rgb(64,0,0).
  }

  //Connect to WiFi and initialise NTP (taken from example sketch for NTPClient library

  static WiFiEventHandler e1, e2, e3;

  WiFi.mode (WIFI_STA);
  WiFi.begin (WIFI_SSID, WIFI_PASSWD);

  pinMode (ONBOARDLED, OUTPUT); // Onboard LED
  digitalWrite (ONBOARDLED, HIGH); // Switch off LED

  NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });

  e1 = WiFi.onStationModeGotIP (onSTAGotIP);// As soon WiFi is connected, start NTP Client
  e2 = WiFi.onStationModeDisconnected (onSTADisconnected);
  e3 = WiFi.onStationModeConnected (onSTAConnected);

}

// --- --- LOOP --- LOOP --- LOOP --- --- //

void loop() {

  static int i = 0;
  static int last = 0;

  //Keep watching the button
  button.tick();

  //Maintain correct colour for the colon - it will take effect the next time it blinks
  colonSetColonColour(isInternetConnection);

  //NTP
  if (wifiFirstConnected) {
    wifiFirstConnected = false;
    NTP.begin (NTP_SERVER, timeZone, true, minutesTimeZone);
    NTP.setInterval (NTP_POLL_INTERVAL);
  }

  if (syncEventTriggered) {
    processSyncEvent (ntpEvent);
    syncEventTriggered = false;
  }

  //Display Time
  if ((millis () - last) > 5100 || displayMode != lastDisplayMode) {
    if (displayMode != lastDisplayMode) {
      lastDisplayMode = displayMode;
    }
    //Serial.println(millis() - last);
    last = millis ();
    Serial.print (i); Serial.print (" ");
    Serial.print (NTP.getTimeDateString ()); Serial.print (" ");
    Serial.print (NTP.isSummerTime () ? "Summer Time. " : "Winter Time. ");
    Serial.print ("WiFi is ");
    Serial.print (WiFi.isConnected () ? "connected" : "not connected"); Serial.print (". ");
    Serial.print ("Uptime: ");
    Serial.print (NTP.getUptimeString ()); Serial.print (" since ");
    Serial.println (NTP.getTimeDateString (NTP.getFirstSync ()).c_str ());

    if (displayMode == 1) {
      // show day:month
      String currentDateTime = NTP.getTimeDateString();
      digitOne = currentDateTime.substring(9, 10).toInt();
      digitTwo = currentDateTime.substring(10, 11).toInt();
      digitThree = currentDateTime.substring(12, 13).toInt();
      digitFour = currentDateTime.substring(13, 14).toInt();
    }
    else {
      // show time
      String currentTime = NTP.getTimeStr();
      digitOne = currentTime.substring(0, 1).toInt();
      digitTwo = currentTime.substring(1, 2).toInt();
      digitThree = currentTime.substring(3, 4).toInt();
      digitFour = currentTime.substring(4, 5).toInt();
    }

    Serial.print ("Display should be showing: ");
    Serial.print (digitOne);
    Serial.print (digitTwo);
    Serial.print (":");
    Serial.print (digitThree);
    Serial.println (digitFour);

    digitString.setDigit(digitOne, 0, red, green, blue); //Color is rgb(64,0,0).
    digitString.setDigit(digitTwo, 1, red, green, blue); //Color is rgb(64,0,0).
    digitString.setDigit(digitThree, 2, red, green, blue); //Color is rgb(64,0,0).
    digitString.setDigit(digitFour, 3, red, green, blue); //Color is rgb(64,0,0).

    i++;
  }
  yield;
}

