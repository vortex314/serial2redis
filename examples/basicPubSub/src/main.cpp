#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdarg>

const char *subscribeCmd[] = {"sub", "_"};
const char *publishCmd[] = {"pub", "_", "_"};

#define UART Serial

#define LINE()          \
  UART.print(__LINE__); \
  UART.print(" : ")

DynamicJsonDocument doc(256);

std::string buffer = "";
DynamicJsonDocument docIn(256);

void publish(const char *key, uint32_t value);

void addLine(std::string &s)
{
  docIn.clear();
  std::string line = s.substr(0, line.length() - 4);
  if (deserializeJson(docIn, line) == DeserializationError::Ok)
  {
    if (docIn.is<JsonArray>() && docIn[0] == "pub")
    {
      uint64_t start = std::stoi(docIn[3].as<const char *>());
      UART.printf("JSON:%s \n", docIn[3].as<const char *>());
      publish("src/arduino/sys/latency", millis() - start);
    }
    else
    {
      UART.printf("RAW:%s \n", line.c_str());
    }
  }
  else
  {
    UART.printf("RAW:%s \n", line.c_str());
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
  UART.begin(115200);
}

uint32_t crc32(uint32_t crc, uint32_t data)
{
  int i;
  crc = crc ^ data;
  for (i = 0; i < 32; i++)
    if (crc & 0x80000000)
      crc = (crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
    else
      crc = (crc << 1);
  return (crc);
}

String crc16(String data)
{
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < data.length(); i++)
  {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
    {
      if (crc & 1)
      {
        crc = (crc >> 1) ^ 0xA001;
      }
      else
      {
        crc = crc >> 1;
      }
    }
  }
  char buf[5];
  sprintf(buf, "%04X", crc);
  return String(buf);
}

void send_json(DynamicJsonDocument &doc)
{
  String s;
  serializeJson(doc, s);
  UART.print(s);
  UART.print(crc16(s));
  UART.println();
}

void publish(const char *key, uint32_t value)
{
  doc.clear();
  copyArray(publishCmd, doc);
  doc[1] = key;
  doc[2] = value;
  send_json(doc);
}

void subscribe(const char *pattern)
{
  doc.clear();
  copyArray(subscribeCmd, doc);
  doc[1] = pattern;
  send_json(doc);
}

uint64_t nextSub = 0;
uint64_t nextPub = 0;

void loop()
{
  if (millis() > nextSub)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10);
    subscribe("dst/arduino/*");
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    nextSub = millis() + 1000;
  }
  if (millis() > nextPub)
  {
    publish("dst/arduino/sys/loopback", millis());
    nextPub = millis() + 1000;
  }
}