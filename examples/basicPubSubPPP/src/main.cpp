#include <Arduino.h>
#include <ArduinoJson.h>
#include <Common.h>
#include <cstdarg>

#include "../../../limero/limero.cpp"
#include "../../src/PPP.cpp"

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
PPP ppp;
class SerialSession {
 public:
   ValueFlow<Bytes> _rxd; 
   SinkFunction<Bytes> _txd;
   SerialSession

  Source<Bytes> rxd() { return _rxd;};
  Sink<Bytes> txd() { return _txd;};
} serialSession;

Flow<Bytes, Json> *bytesToJson;
Flow<Json, Bytes> jsonToBytes;
ValueFlow<Json> redisRequests;
ValueFlow<Json> redisReplies;

void serialEvent1() {
  Bytes data;
  while (UART.available() > 0) {
    data += UART.read();
  };
  serial._rxd.on(data);
}

class RedisClient {

  Sink<Json> in();
  Source<Json> out();

};

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  UART.begin(921600);
  Flow<Bytes, Json> bytesToJson =
      new LambdaFlow<Bytes, Json>([&](Json &docIn, const Bytes &frame) {
        std::string s = std::string(frame.begin(), frame.end());
        if (s.c_str() == nullptr) return false;
        docIn.clear();
        auto rc = deserializeJson(docIn, s);
        if (rc != DeserializationError::Ok) {
          INFO("RXD : %s%s%s", ColorOrange, s.c_str(), ColorDefault);
        } else {
          //          INFO("RXD : '%s'", s.c_str());
        }
        return rc == DeserializationError::Ok;
      });

  Flow<Json, Bytes> jsonToBytes =
      new LambdaFlow<Json, Bytes>([&](Bytes &msg, const Json &docIn) {
        std::string str;
        size_t sz = serializeJson(docIn, str);
        //        INFO("TXD : '%s'", str.c_str());
        msg = Bytes(str.begin(), str.end());
        return str.size() > 0;
      });
    serialSession.rxd() >> ppp.deframe() >> bytesToJson >> redisReplies;
    redisRequests >> jsonToBytes >> ppp.frame() >> serialSession.txd(); 
}

void publish(const char *key, uint32_t value) {
  doc.clear();
  copyArray(publishCmd, doc);
  doc[1] = key;
  doc[2] = String(value, 10);
  redisRequests.on(doc);
}

void hello() {
  doc.clear();
  copyArray(helloCmd, doc);
  redisRequests.on(doc);
}

void subscribe(const char *pattern) {
  doc.clear();
  copyArray(subscribeCmd, doc);
  doc[1] = pattern;
  redisRequests.on(doc);
}

uint64_t nextSub = 0;
uint64_t nextPub = 0;

void loop() {
  if (millis() > nextSub) {
    hello();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(10);
    subscribe("dst/maple/*");
    digitalWrite(LED_BUILTIN, LOW);
    delay(10);
    nextSub = millis() + 1000;
  }
  if (millis() > nextPub) {
    publish("dst/maple/sys/loopback", millis());
    nextPub = millis() + 100;
  }
}