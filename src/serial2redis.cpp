#include <ArduinoJson.h>
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
#include <ConfigFile.h>
#include <Flows.h>
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

#include <thread>
#include <unordered_map>
#include <utility>

Log logger;

//================================================================
#include <cborjson.h>

int main(int argc, char **argv) {
  INFO("Loading configuration.");
  Json config;
  config["serial"]["port"] = "/dev/ttyUSB0";
  config["serial"]["baudrate"] = 115200;
  config["serial"]["format"] = "json";
  config["serial"]["frame"] = "crlf";
  config["redis"]["host"] = "localhost";
  config["redis"]["port"] = 6379;
  configurator(config, argc, argv);

  Thread workerThread("worker");

  Redis redis(workerThread, config["redis"].as<JsonObject>());
  redis.connect();

  SessionSerial serial(workerThread, config["serial"].as<JsonObject>());
  serial.init();
  serial.connect();
  string port = config["serial"]["port"];

  Timer &timer = workerThread.createTimer(1000, true);
  auto publishAlive =
      new Flow<Timer, Json>([&](Json &systemAlive, const Timer &) {
        DEBUG("Publishing system alive");
        std::string shortPort = port.substr(strlen("/dev/tty"));
        std::string systemAliveTopic = "src/";
        systemAliveTopic += shortPort + "/system/alive";
        systemAlive[0] = "publish";
        systemAlive[1] = systemAliveTopic;
        systemAlive[2] = "true";
        return true;
      });

  timer >> *publishAlive >> redis.request();  // send alive every second

  Framing crlf("\r\n", 10000);
  PPP ppp(workerThread, 1024);
  std::string format = config["serial"]["format"] | "json";
  std::string framing = config["serial"]["frame"] | "crlf";
  Sink<Bytes> logBytes([](const Bytes &bs) {
    INFO("RXD[%d] : %s%s%s", bs.size(), ColorOrange, charDump(bs).c_str(),
         ColorDefault);
  });

  if (framing == "crlf" && format == "json") {
    serial.incoming() >> crlf.deframe() >> *crlfCrcToRequest() >>
        redis.request();
    redis.response() >> *responseToCrlfCrc() >> crlf.frame() >>
        serial.outgoing();

  } else if (framing == "ppp" && format == "json") {
    serial.incoming() >> ppp.deframe() >> *bytesToRequest() >> redis.request();
    redis.response() >> *responseToBytes() >> ppp.frame() >> serial.outgoing();

    ppp.garbage() >> logBytes;

  } else if (framing == "ppp" && format == "cbor") {
    serial.incoming() >> ppp.deframe() >> *cborToRequest() >> redis.request();
    redis.response() >> *responseToCbor() >> ppp.frame() >> serial.outgoing();

    ppp.garbage() >> logBytes;

  } else {
    WARN("unknown framing : %s ", framing.c_str());
    exit(-1);
  }

  workerThread.run();
}
