#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Wire.h>
#include "RTClib.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#define RELAY1 5
#define RELAY2 18
#define RELAY3 19

RTC_DS3231 rtc;
WebServer server(80);
StaticJsonDocument<3048> timerData;

const char* ssid = "ESP32_Hotspot";
const char* password = "12345678";

void setup() {
    Serial.begin(115200);
    WiFi.softAP(ssid, password);
    Serial.println("Hotspot Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    Wire.begin();
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(RELAY3, OUTPUT);
    
    loadTimerData();
    
    server.on("/", HTTP_GET, serveHTML);
    server.on("/data", HTTP_GET, sendJSON);
    server.on("/data", HTTP_POST, receiveJSON);

    server.on("/getTimerData", HTTP_GET, handleGetTimerData);
    server.on("/saveTimerData", HTTP_POST, handleSaveTimerData);

    // اضافه کردن CORS headers
    server.enableCORS(true);
    server.begin();
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
}

void loop() {
    server.handleClient();
    checkTimers();
}
void handleGetTimerData() {
    File file = SPIFFS.open("/timers.json", "r");
    if (!file) {
        server.send(404, "application/json", "{\"error\":\"File not found\"}");
        return;
    }
    server.streamFile(file, "application/json");
    file.close();
}

void handleSaveTimerData() {
    if (server.hasArg("plain")) {
        File file = SPIFFS.open("/timers.json", "w");
        if (!file) {
            server.send(500, "text/plain", "Failed to open file for writing");
            return;
        }
        file.print(server.arg("plain"));
        file.close();
        server.send(200, "text/plain", "Data saved successfully");
    } else {
        server.send(400, "text/plain", "No data received");
    }
}
void serveHTML() {
    File file = SPIFFS.open("/index.html", "r");
    if (!file) {
        server.send(500, "text/plain", "File not found");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

void sendJSON() {
    String jsonString;
    serializeJson(timerData, jsonString);
    server.send(200, "application/json", jsonString);
}

void receiveJSON() {
    if (server.hasArg("plain")) {
        StaticJsonDocument<3048> newTimerData;
        deserializeJson(newTimerData, server.arg("plain"));
        
        for (int i = 0; i < 7; i++) {
            for (int j = 1; j <= 3; j++) {
                timerData[i][String(j) + "on"] = newTimerData[i][String(j) + "on"].as<String>();
                timerData[i][String(j) + "off"] = newTimerData[i][String(j) + "off"].as<String>();
                timerData[i][String(j) + "on2"] = newTimerData[i][String(j) + "on2"].as<String>();
                timerData[i][String(j) + "off2"] = newTimerData[i][String(j) + "off2"].as<String>();
                timerData[i][String(j) + "is_timer2"] = newTimerData[i][String(j) + "is_timer2"].as<String>();
                timerData[i][String(j) + "isholiday"] = newTimerData[i][String(j) + "isholiday"].as<String>();
            }
        }
        
        saveTimerData();
        server.send(200, "application/json", "{\"status\": \"ok\"}");
    }
}

void saveTimerData() {
    File file = SPIFFS.open("/timers.json", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    serializeJson(timerData, file);
    file.close();
}

void loadTimerData() {
    File file = SPIFFS.open("/timers.json", "r");
    if (!file) {
        Serial.println("No saved timer data");
        return;
    }
    deserializeJson(timerData, file);
    file.close();
}

void checkTimers() {
    DateTime now = rtc.now();
    char currentTime[6];
    sprintf(currentTime, "%02d:%02d", now.hour(), now.minute());
    
    int dayOfWeek = now.dayOfTheWeek();
    for (int i = 1; i <= 3; i++) {
        String onTime = timerData[dayOfWeek][String(i) + "on"].as<String>();
        String offTime = timerData[dayOfWeek][String(i) + "off"].as<String>();
        
        if (onTime == currentTime) {
            digitalWrite(i == 1 ? RELAY1 : i == 2 ? RELAY2 : RELAY3, HIGH);
        }
        if (offTime == currentTime) {
            digitalWrite(i == 1 ? RELAY1 : i == 2 ? RELAY2 : RELAY3, LOW);
        }
    }
}
