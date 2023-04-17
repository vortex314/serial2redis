#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdarg>

const char *subscribeCmd[] = {"SUB", "_"};
const char *publishCmd[] = {"PUB", "_", "_"};

#define UART Serial

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
  UART.begin(115200);
}

uint32_t crc32(uint32_t crc, uint32_t data)
{
  int i;
  crc = crc ^ data;
  for(i=0; i<32; i++)
    if (crc & 0x80000000)
      crc = (crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
    else
      crc = (crc << 1);
  return(crc);
}

String crc(String s){
  uint32_t crc = 0xFFFFFFFF;
  for( uint32_t i=0;i<s.length();i++) crc=crc32(crc,s[i]);
  String out = String(crc,16);
  while(out.length()<8) out = "0"+out;
  return out;
}


void send_json(DynamicJsonDocument &doc)
{
  String s;
  serializeJson(doc, s);
  UART.print(s);
  UART.print(crc(s));
  UART.println();
}

void publish(const char *key, uint32_t value)
{
  doc.clear();
  copyArray(publishCmd, doc);
  doc[1] = key;
  doc[2] = String(value, 10);
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
    subscribe("dst/maple/*");
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    nextSub = millis() + 1000;
  }
  if (millis() > nextPub)
  {
    publish("dst/maple/sys/loopback", millis());
    nextPub = millis() + 100;
  }
}