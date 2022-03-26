#include <Arduino.h>
#include <ArduinoJson.h>

const char *helloCmd[] = {"HELLO", "3"};
const char *subscribeCmd[] = {"PSUBSCRIBE", "____"};
const char *publishCmd[] = {"PUBLISH", "____", "___"};

DynamicJsonDocument doc(1024);

#include <Arduino.h>
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
}

void publish(const char *key, uint32_t value)
{
  doc.clear();
  JsonArray array = doc.to<JsonArray>();
  copyArray(publishCmd, array);
  doc[1] = key;
  doc[2] = String(value, 10);
  serializeJson(doc, Serial);
  Serial.println();
}

void hello()
{
  doc.clear();
  copyArray(helloCmd, doc);
  serializeJson(doc, Serial);
  Serial.println();
}

void subscribe(const char *pattern)
{
  doc.clear();
  copyArray(subscribeCmd, doc);
  doc[1] = pattern;
  serializeJson(doc, Serial);
  Serial.println();
}

void loop()
{
  hello();
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(100);                      
  subscribe("dst/maple/*");
  digitalWrite(LED_BUILTIN, LOW); 
  delay(100);                     
  publish("dst/maple/sys/loopback", millis());
}