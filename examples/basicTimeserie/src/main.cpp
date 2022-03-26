#include <Arduino.h>
#include <ArduinoJson.h>

const char *addCmd[] = {"TS.ADD", "randomArduino", "*", "____","LABELS","property","random"};
DynamicJsonDocument doc(1024);

#include <Arduino.h>
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
}
void loop()
{
  doc.clear();
  JsonArray array = doc.to<JsonArray>();
  copyArray( addCmd,array);
  doc[3] = std::to_string(random(0,100));
  serializeJson(doc, Serial);
  Serial.println();
  digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // turn the LED off by making the voltage LOW
  delay(100);                      // wait for a second
}