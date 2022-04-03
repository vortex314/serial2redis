#include <ArduinoJson.h>
#include <Common.h>
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

void loadConfig(JsonDocument &cfg, int argc, char **argv) {
  // defaults
  cfg["serial"]["port"] = "/dev/ttyUSB0";
  cfg["serial"]["baudrate"] = 115200;
  cfg["serial"]["frame"] = "crlf";
  cfg["broker"]["host"] = "localhost";
  cfg["broker"]["port"] = 6379;
  cfg["proxy"]["timeout"] = 5000;
  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:b:f:t:")) != -1) switch (c) {
      case 'b':
        cfg["serial"]["baudrate"] = atoi(optarg);
        break;
      case 's':
        cfg["serial"]["port"] = optarg;
        break;
      case 'f':
        cfg["serial"]["frame"] = optarg;
        break;
      case 'h':
        cfg["broker"]["host"] = optarg;
        break;
      case 'p':
        cfg["broker"]["port"] = atoi(optarg);
        break;
      case 't':
        cfg["proxy"]["timeout"] = atoi(optarg);
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
  INFO("config:%s", s.c_str());
};

//================================================================

//==========================================================================
int main(int argc, char **argv) {
  INFO("Loading configuration.");
  DynamicJsonDocument config(10240);
  loadConfig(config, argc, argv);
  Thread workerThread("worker");

  Redis redis(workerThread, config["broker"].as<JsonObject>());
  redis.init();
  redis.connect();

  SessionSerial serial(workerThread, config["serial"].as<JsonObject>());
  serial.init();
  serial.connect();

  Framing crlf("\r\n", 10000);
  PPP ppp(1024);

  auto bytesToJson =
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

  auto jsonToBytes =
      new LambdaFlow<Json, Bytes>([&](Bytes &msg, const Json &docIn) {
        std::string str;
        size_t sz = serializeJson(docIn, str);
                INFO("TXD : '%s'", str.c_str());
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

  } else {
    WARN("unknown framing : %s ",framing.c_str());
    exit(-1);
  }

  printf("%s%s%s\n", ColorOrange, "Orange", ColorDefault);

  workerThread.run();
}
