#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstdarg>

class SerialPubSub
{
  Stream &_stream;
  DynamicJsonDocument doc;
  std::string buffer = "";
  DynamicJsonDocument docIn;

public:
  SerialPubSub(Stream &stream) : _stream(stream),doc(1024),docIn(1024) {}

  void handle_line(std::string &s)
  {
    docIn.clear();
    std::string line = s.substr(0, s.length() - 4);
    std::string crc = s.substr(s.length() - 4, 4);
    if (String(crc.c_str()) != crc16(line.c_str()))
    {
      _stream.printf("CRC failed :%s => %s vs %s \n", line.c_str(), crc.c_str(), crc16(line.c_str()).c_str());
    }

    if (deserializeJson(docIn, line) == DeserializationError::Ok)
    {
      if (docIn.is<JsonArray>() && docIn[0] == "pub" && docIn[1] == "dst/arduino/sys/loopback")
      {
        uint64_t start = docIn[2].as<uint64_t>();
        publish("src/arduino/sys/latency", millis() - start);
      }
      else
      {
        // _stream.printf("RAW:%s \n", line.c_str());
      }
    }
    else
    {
      // _stream.printf("RAW:%s \n", line.c_str());
    }
  }

  void handle_byte(uint8_t b)
  {
    if (b == '\r' || b == '\n')
    {
      if (buffer.length() > 0)
        handle_line(buffer);
      buffer.clear();
    }
    else
      buffer.push_back(b);
    if (buffer.size() > 512)
      buffer.clear();
  }

  void rxd()
  {
    while (_stream.available() > 0)
      handle_byte(_stream.read());
  };

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
    _stream.print(s);
    _stream.print(crc16(s));
    _stream.println();
  }

  void publish(const char *key, uint32_t value)
  {
    doc.clear();
    doc[0] = "pub";
    doc[1] = key;
    doc[2] = value;
    send_json(doc);
  }

  void subscribe(const char *pattern)
  {
    doc.clear();
    doc[0]="sub";
    doc[1] = pattern;
    send_json(doc);
  }
};

SerialPubSub serialPubSub(Serial);

void serialEvent()
{
  serialPubSub.rxd();
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
}

uint64_t nextSub = 0;
uint64_t nextPub = 0;

void loop()
{
  if (millis() > nextSub)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10);
    serialPubSub.subscribe("dst/arduino/*");
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    nextSub = millis() + 1000;
  }
  if (millis() > nextPub)
  {
    serialPubSub.publish("dst/arduino/sys/loopback", millis());
    nextPub = millis() + 10;
  }
}