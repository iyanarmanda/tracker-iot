// #include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <GeoLinker.h>
#include "secrets.h"

#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600

const uint16_t updateInterval = 2;
const bool enableOfflineStorage = true;
const uint8_t offlineBufferLimit = 20;
const bool enableAutoReconnect = true;

const int8_t timeOffsetHours = 5;
const int8_t timeOffsetMinutes = 30;

bool isTrackingActive = false;

const char* MQTT_SUB_TOPIC = "tracker/control";
const char* MQTT_PUB_TOPIC = "tracker/telemetry";

HardwareSerial gpsSerial(1);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
GeoLinker geo;

String getUniqueDeviceId() {
  uint64_t chipid = ESP.getEfuseMac();
  char id[20];
  snprintf(id, sizeof(id), "ESP32-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  return String(id);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();

  Serial.print("[MQTT] Get message from [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (message.equalsIgnoreCase("ON")) {
    isTrackingActive = true;
    Serial.println("Tracking GPS ACTIVATED");
    mqttClient.publish(MQTT_PUB_TOPIC, "STATUS: GPS_TRACKING_ON");
  } 
  else if (message.equalsIgnoreCase("OFF")) {
    isTrackingActive = false;
    Serial.println("Tracking GPS DEACTIVATED");
    mqttClient.publish(MQTT_PUB_TOPIC, "STATUS: GPS_TRACKING_OFF");
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to broker: ");
    Serial.println(MQTT_BROKER);
    
    String clientId = getUniqueDeviceId();
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("Connected!");
      mqttClient.subscribe(MQTT_SUB_TOPIC);
      Serial.print("Subscribed at topic: ");
      Serial.println(MQTT_SUB_TOPIC);
    } else {
      Serial.print("Connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" | try again in 5 second...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Starting GeoLinker GPS Tracker...");

  String deviceId = getUniqueDeviceId();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  
  geo.begin(gpsSerial);
  geo.setApiKey(API_KEY);
  geo.setDeviceID(deviceId);
  geo.setUpdateInterval_seconds(updateInterval);
  geo.setDebugLevel(DEBUG_BASIC);
  geo.enableOfflineStorage(enableOfflineStorage);
  geo.enableAutoReconnect(enableAutoReconnect);
  geo.setOfflineBufferLimit(offlineBufferLimit);
  geo.setTimeOffset(timeOffsetHours, timeOffsetMinutes);
  geo.setNetworkMode(GEOLINKER_WIFI);
  geo.setWiFiCredentials(WIFI_SSID, WIFI_PASS);

  if (!geo.connectToWiFi()) {
    Serial.println("ERROR: WiFi connection failed!");
  } else {
    Serial.println("WiFi connected successfully!");
  }

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  Serial.println("Setup initiated successfully...");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  }

  if (isTrackingActive) {
    uint8_t status = geo.loop();

    if (status > 0) {
      Serial.print("GeoLinker Operation: ");

      switch(status) {
        case STATUS_SENT:
          Serial.println("Data transmitted successfully to cloud!");
          mqttClient.publish(MQTT_PUB_TOPIC, "DATA_SENT_OK");
          break;
        case STATUS_GPS_ERROR:
          Serial.println("GPS module connection error");
          mqttClient.publish(MQTT_PUB_TOPIC, "ERROR_GPS_HARDWARE");
          break;
        case STATUS_NETWORK_ERROR:
          Serial.println("Network connectivity issue");
          mqttClient.publish(MQTT_PUB_TOPIC, "ERROR_NETWORK");
          break;
        case STATUS_BAD_REQUEST_ERROR:
          Serial.println("Server rejected request");
          break;
        case STATUS_PARSE_ERROR:
          Serial.println("GPS data parsing error");
          break;
        case STATUS_INTERNAL_SERVER_ERROR:
          Serial.println("GeoLinker server internal error");
          break;
        default:
          break;
      }
    }
  }

  delay(100);
}
