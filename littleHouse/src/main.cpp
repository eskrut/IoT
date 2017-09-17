
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"

MDNSResponder mdns;
ESP8266WebServer server(80);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

const int L1 = 2;
const int L2 = 4;
const int L3 = 5;
const int L4 = 12;
const int L5 = 14;
const int L6 = 16;

#include <map>
#include <string>

std::map<std::string, int> nameIDMap;
std::map<int, int> states;

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void turmLight(const std::string &name, int state) {
  if(name == "all") {
    for(const auto &rid : nameIDMap)
      turmLight(rid.first, state);
  }
  else {
    auto id = nameIDMap[name];
    states[id] = state;
    digitalWrite(id, state);
  }
}

void setup() {

  nameIDMap["room1"] = L1;
  nameIDMap["room2"] = L2;
  nameIDMap["room3"] = L3;
  nameIDMap["room4"] = L4;
  nameIDMap["room5"] = L5;
  nameIDMap["room6"] = L6;

  for(const auto &rid : nameIDMap) {
    pinMode(rid.second, OUTPUT);
    digitalWrite(rid.second, 0);
    states[rid.second] = 0;
  }

  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  if(!wifiManager.autoConnect()) {
    ESP.reset();
    delay(1000);
  }

  ArduinoOTA.setHostname("littleHouse");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Start server
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  for(const auto &rid : nameIDMap) {
    server.on(("/" + rid.first + "/1").c_str(), [&rid](){
      server.send(200, "text/plain", ("turning on " + rid.first).c_str());
      turmLight(rid.first, 1);
    });
    server.on(("/" + rid.first + "/0").c_str(), [&rid](){
      server.send(200, "text/plain", ("turning off " + rid.first).c_str());
      turmLight(rid.first, 0);
    });
    server.on(("/" + rid.first + "/state").c_str(), [&rid](){
      char sstate[3];
      sprintf(sstate, "%d\n", states[rid.second]);
      server.send(200, "text/plain", sstate);
    });
  }

  server.on("/on", [](){
    server.send(200, "text/plain", "turning all lights on");
    turmLight("all", 1);
  });

  server.on("/off", [](){
    server.send(200, "text/plain", "turning all lights off");
    turmLight("all", 0);
  });

  server.onNotFound(handleNotFound);

  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
