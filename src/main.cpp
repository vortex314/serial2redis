// Timeseries exampkle => DON'T FORGET you need to have redis running with
// timeseries module short way : 
// $ docker pull redislabs/redistimeseries
// See also 
// https://redisgrafana.github.io/development/images/

#include <Arduino.h>
#include <ArduinoJson.h>

const char* addCmd[] = {"TS.ADD", "upTime", "*", "____"};
const char* alterCmd[] = {"TS.ALTER", "upTime", "____", "LABELS",   "node",
                          "esp32",    "object", "sys",  "property", "myTime"};
const char* clientCmd[] = {"CLIENT", "SETNAME", "esp32"};
int count = 0;
DynamicJsonDocument doc(256);

void setup() { Serial.begin(115200); }

void loop() {
  doc.clear();
  doc.copyArray(doc, addCmd);
  doc[3] = std::to_string(millis());
  serializeJson(doc, Serial);
  if (count++ % 100 == 0) {
    doc.clear();
    doc.copyArray(doc, alterCmd);
    serializeJson(doc, Serial);
    doc.clear();
    doc.copyArray(doc, clientCmd);
    serializeJson(doc, Serial);
  }
  delay(100);
}