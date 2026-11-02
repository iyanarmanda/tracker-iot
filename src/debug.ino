// #include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <GeoLinker.h>
#include "secrets.h"

#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600

HardwareSerial gpsSerial(1);
GeoLinker geo;

unsigned long lastStatusPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nGEOLINKER GPS DEBUG");

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("Hardware Serial 1 initialized at PIN RX:16, TX:17");

  geo.begin(gpsSerial);
  geo.setApiKey(API_KEY);
  geo.setUpdateInterval_seconds(2);
  
  geo.setDebugLevel(DEBUG_VERBOSE); 
  geo.enableOfflineStorage(true);
  geo.setNetworkMode(GEOLINKER_WIFI);
  geo.setWiFiCredentials(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  if (geo.connectToWiFi()) {
    Serial.println("WiFi Connected Successfully!");
  } else {
    Serial.println("WiFi Connection Failed. Operating in offline/buffer mode.");
  }

  Serial.println("\nStarting Live Telemetry & Raw NMEA Stream");
}

void loop() {
  uint8_t status = geo.loop();

  if (status > 0) {
    Serial.print("\nGeoLinker Event Return Code: ");
    Serial.println(status);
  }

  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    Serial.write(c);
  }

  delay(2000);
}
