#include <ArduinoJson.h>
#include <Frame.h>
#include <Framing.h>
#include <Log.h>
#include <PPP.h>
#include <Redis.h>
#include <SessionSerial.h>
#include <StringUtility.h>
#include <async.h>
#include <broker_protocol.h>
#include <hiredis.h>
#include <limero.h>
#include <stdio.h>
#include <ConfigFile.h>

#include <thread>
#include <unordered_map>
#include <utility>

Log logger;



//================================================================

//==========================================================================
int main(int argc, char **argv) {
  INFO("Loading configuration.");
  Json config;
  config["serial"]["port"] = "/dev/ttyUSB0";
  config["serial"]["baudrate"] = 115200;
  config["serial"]["frame"] = "crlf";
  config["broker"]["host"] = "localhost";
  config["broker"]["port"] = 6379;
  config["proxy"]["timeout"] = 5000;
  configurator(config, argc, argv);
  Thread workerThread("worker");

  Redis redis(workerThread, config["redis"].as<JsonObject>());
  redis.connect();

  SessionSerial serial(workerThread, config["serial"].as<JsonObject>());
  serial.init();
  serial.connect();

  Framing crlf("\r\n", 10000);
  PPP ppp(workerThread, 1024);

  auto bytesToJson =
      new LambdaFlow<Bytes, Json>([&](Json &docIn, const Bytes &frame) {
        std::string s = std::string(frame.begin(), frame.end());
        if (s.c_str() == nullptr) return false;
        docIn.clear();
        auto rc = deserializeJson(docIn, s);
        if (rc != DeserializationError::Ok) {
          INFO("RXD[%d] : %s%s%s", s.length(), ColorOrange, s.c_str(),
               ColorDefault);
        } else {
          INFO("RXD[%d] : '%s'", s.length(), s.c_str());
        }
        return rc == DeserializationError::Ok;
      });

  auto jsonToBytes =
      new LambdaFlow<Json, Bytes>([&](Bytes &msg, const Json &docIn) {
        std::string str;
        size_t sz = serializeJson(docIn, str);
        INFO("TXD[%d] : '%s'", str.length(), str.c_str());
        msg = Bytes(str.begin(), str.end());
        return str.size() > 0;
      });

  std::string framing = config["serial"]["frame"] | "crlf";

  if (framing == "crlf") {
    serial.incoming() >> crlf.deframe() >> bytesToJson >> redis.request();
    redis.response() >> jsonToBytes >> crlf.frame() >> serial.outgoing();

  } else if (framing == "ppp") {
    serial.incoming() >> ppp.deframe() >> bytesToJson >> redis.request();
    redis.response() >> jsonToBytes >> ppp.frame() >> serial.outgoing();
    ppp.garbage() >> [&](const Bytes &bs) {
      INFO("RXD[%d] : %s%s%s", bs.size(), ColorOrange, charDump(bs).c_str(),
           ColorDefault);
    };

  } else {
    WARN("unknown framing : %s ", framing.c_str());
    exit(-1);
  }

  printf("%s%s%s\n", ColorOrange, "Orange", ColorDefault);

  workerThread.run();
}
