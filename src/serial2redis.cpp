#include <ArduinoJson.h>
#include <Common.h>
#include <Frame.h>
#include <Log.h>
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
#include <PPP.h>

Log logger;

void loadConfig(JsonObject cfg, int argc, char **argv) {
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:b:")) != -1) switch (c) {
      case 'b':
        cfg["serial"]["baudrate"] = atoi(optarg);
        break;
      case 's':
        cfg["serial"]["port"] = optarg;
        break;
      case 'h':
        cfg["broker"]["host"] = optarg;
        break;
      case 'p':
        cfg["broker"]["port"] = atoi(optarg);
        break;
      case '?':
        printf("Usage %s -h <host> -p <port> -s <serial_port> -b <baudrate>\n",
               argv[0]);
        break;
      default:
        WARN("Usage %s -h <host> -p <port> -s <serial_port> -b <baudrate>\n",
             argv[0]);
        abort();
    }
  std::string s;
  serializeJson(cfg, s);
  INFO("config:%s", s);
};

//================================================================

//==========================================================================
int main(int argc, char **argv) {
  INFO("Loading configuration.");
  DynamicJsonDocument config(10240);
  loadConfig(config.as<JsonObject>(), argc, argv);
  Thread workerThread("worker");

  JsonObject serialConfig = config["serial"];
  SessionSerial serialSession(workerThread, config["serial"]);

  JsonObject brokerConfig = config["broker"];
  Redis redis(workerThread, brokerConfig);

  PPP ppp;

  serialSession.init();
  serialSession.connect();

  serialSession.incoming() >> [&](const Bytes &s) {
    //        INFO("RXD %s", hexDump(s).c_str());
  };

  auto bytesToJson =
      new LambdaFlow<Bytes, Json>([&](Json &docIn, const Bytes &frame) {
        std::string s = std::string(frame.begin(), frame.end());
        INFO("REQ : '%s'", s.c_str());
        docIn.clear();
        return deserializeJson(docIn, s) == DeserializationError::Ok;
      });

  auto jsonToBytes =
      new LambdaFlow<Json, Bytes>([&](Bytes &msg, const Json &docIn) {
        std::string str;
        size_t sz = serializeJson(docIn, str);
        msg = Bytes(str.begin(), str.end());
        return sz > 0;
      });

  auto pppFrameExtract = new LambdaFlow<Bytes, Bytes>(
      [&](Bytes &out, const Bytes &in) { return true; });

  auto pppFrameEnvelop = new LambdaFlow<Bytes, Bytes>(
      [&](Bytes &out, const Bytes &in) { return true; });

  auto crcCheck = new LambdaFlow<Bytes, Bytes>(
      [&](Bytes &out, const Bytes &in) { return true; });

  auto crcAdd = new LambdaFlow<Bytes, Bytes>(
      [&](Bytes &out, const Bytes &in) { return true; });

  serialSession.incoming() >> bytesToJson >> redis.request();

  redis.response() >> jsonToBytes >> ppp.frame() >>  serialSession.outgoing();

  SinkFunction<String> redisLogStream([&](const String &bs) {
    static String buffer;
    for (uint8_t b : bs) {
      if (b == '\n') {
        printf("%s%s%s\n", ColorOrange, buffer.c_str(), ColorDefault);
        buffer.clear();
      } else if (b == '\r') {  // drop
      } else {
        buffer += (char)b;
      }
    }
  });

  serialSession.logs() >> redisLogStream;
  workerThread.run();
}
