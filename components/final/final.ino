#include <WiFi.h>
#include <FS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <ELMduino.h>

#define ELM_PORT SerialBT

// SSID & Password
const char *ssid = "ESP32";         // Enter your SSID here
const char *password = "123456789"; // Enter your Password here

// IP Address details
// IPAddress local_ip(192, 168, 1, 1);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 255, 0);

// Web server
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Bluetooth
BluetoothSerial SerialBT;

// ELM327
ELM327 Engine;
StaticJsonDocument<JSON_OBJECT_SIZE(10)> engineData;

// Utility
char engineDataString[300];

void setup()
{
  Serial.begin(115200);

  /* Create soft access point */

  WiFi.softAP(ssid, password);
  // WiFi.softAPConfig(local_ip, gateway, subnet);

  Serial.print("Wifi access point created\nIP address: ");
  Serial.println(WiFi.softAPIP());


  /* Initialize SPIFFS */

  if(!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    while (1);
  }

  // DEBUG: ls
  // for(File file = SPIFFS.open("/"); file; file = file.openNextFile())
  //  Serial.println(file.name());

  Serial.println("File system mounted");


  /* Web server */

  server.serveStatic("/", SPIFFS, "/");
  // server.serveStatic("/", SPIFFS, "/js/");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html");
  });

  server.begin();
  server.addHandler(&events);

  Serial.println("Web server started");


  /* ELM327 */

  ELM_PORT.begin("ESP32", true);

  if (!ELM_PORT.connect("OBDII")) {
    DEBUG_PORT.println("Couldn't connect to OBD scanner");
    while (1);
  }

  Engine.begin(ELM_PORT);

  Serial.println("Connected to ELM327");

}

void loop()
{
  readOBDData();
  serializeJson(engineData, engineDataString);

  events.send((const char *) engineDataString, "dataupdate", millis());
  delay(100);
}

void readOBDData() {
  // TODO: Check for errors from the OBD-II
  // Error 7 -> OBD unreachable (car's off)
  // Error 5 -> OBD is no more reachable (car's been turned off)
  // If any of those two happen, write a special value in the engineData and return

  engineData["VEHICLE_SPEED"] = getValueFromOBD(VEHICLE_SPEED);
  engineData["ENGINE_RPM"] = getValueFromOBD(ENGINE_RPM) / 4.0;
  engineData["FUEL_TANK_LEVEL_INPUT"] = getValueFromOBD(FUEL_TANK_LEVEL_INPUT) / 2.55;

  engineData["AMBIENT_AIR_TEMP"] = getValueFromOBD(AMBIENT_AIR_TEMP) - 40;
  engineData["ENGINE_OIL_TEMP"] = getValueFromOBD(ENGINE_OIL_TEMP) - 40;
  engineData["ENGINE_COOLANT_TEMP"] = getValueFromOBD(ENGINE_COOLANT_TEMP);
  engineData["INTAKE_AIR_TEMP"] = getValueFromOBD(INTAKE_AIR_TEMP);

  engineData["ENGINE_LOAD"] = getValueFromOBD(ENGINE_LOAD) / 2.55;
  engineData["RELATIVE_THROTTLE_POSITION"] = getValueFromOBD(RELATIVE_THROTTLE_POSITION) / 2.55;
  engineData["ACTUAL_ENGINE_TORQUE"] = getValueFromOBD(ACTUAL_ENGINE_TORQUE) - 125;

   // DEBUG
  serializeJsonPretty(engineData, Serial);
  Serial.println();
}

float getValueFromOBD(uint8_t pid) {
  if (Engine.queryPID(SERVICE_01, pid))
    return Engine.findResponse();
  return ELM_GENERAL_ERROR;
}
