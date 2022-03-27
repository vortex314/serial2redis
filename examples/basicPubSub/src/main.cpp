#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdarg>

const char *helloCmd[] = {"HELLO", "3"};
const char *subscribeCmd[] = {"PSUBSCRIBE", "_"};
const char *publishCmd[] = {"PUBLISH", "_", "_"};

#define UART Serial1

#define LINE()          \
  UART.print(__LINE__); \
  UART.print(" : ")

DynamicJsonDocument doc(256);

std::string buffer = "";
DynamicJsonDocument docIn(256);

void publish(const char *key, uint32_t value);

void addLine(std::string &line)
{
  docIn.clear();
  if (deserializeJson(docIn, line) == DeserializationError::Ok)
  {
    if (docIn.is<JsonArray>() && docIn[0] == "pmessage")
    {
      uint64_t start = std::stoi(docIn[3].as<const char *>());
      //      UART.printf("JSON:%s \n",docIn[3].as<const char*>());
      publish("src/maple/sys/latency", millis() - start);
    }
    else
    {
      //      UART.printf("RAW:%s \n",line.c_str());
    }
  }
}

void addByte(uint8_t b)
{
  if (b == '\r' || b == '\n')
  {
    if (buffer.length() > 0)
      addLine(buffer);
    buffer.clear();
  }
  else
    buffer.push_back(b);
}

void serialEvent1()
{
  while (UART.available() > 0)
    addByte(UART.read());
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  UART.begin(921600);
}

void publish(const char *key, uint32_t value)
{
  doc.clear();
  copyArray(publishCmd, doc);
  doc[1] = key;
  doc[2] = String(value, 10);
  serializeJson(doc, UART);
  UART.println();
}

void hello()
{
  doc.clear();
  copyArray(helloCmd, doc);
  serializeJson(doc, UART);
  UART.println();
}

void subscribe(const char *pattern)
{
  doc.clear();
  copyArray(subscribeCmd, doc);
  doc[1] = pattern;
  serializeJson(doc, UART);
  UART.println();
}

uint64_t nextSub=0;
uint64_t nextPub=0;

void loop()
{
  if (millis()> nextSub )
  {
    hello();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10);
    subscribe("dst/maple/*");
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    nextSub = millis()+1000;
  }
  if (millis() > nextPub ) {
    publish("dst/maple/sys/loopback", millis());
    nextPub = millis()+100;
  }
}