#include <ArduinoJson.h>
#include <CborDeserializer.h>
#include <CborDump.h>
#include <CborSerializer.h>
#include <ConfigFile.h>
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
Json cborToJson(const Bytes &);
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

  Framing crlf("\r\n", 10000);
  PPP ppp(workerThread, 1024);
  std::string format = config["serial"]["format"];

  auto cborRequester = new SinkFunction<Bytes>([&](const Bytes &frame) {
    Json json = cborToJson(frame);
    INFO("RXD : %s", json.toString().c_str());
    if (json.is<JsonArray>()) {
      if (json[0] == "pub") {
        if (json[1].is<std::string>()) {
          if (json[2].is<JsonObject>()) {
            JsonObject props = json[2].as<JsonObject>();
            std::string prefix = json[1];
            for (JsonPair kv : props) {
              std::string key = kv.key().c_str();
              JsonVariant value = kv.value();
              std::string out;
              serializeJson(value, out);
              Json request;
              request[0] = "publish";
              request[1] = prefix + key;
              request[2] = out;
              redis.request().on(request);
            }
          } else {
            std::string out;
            std::string key = json[1];
            JsonVariant value = json[2];
            serializeJson(value, out);
            Json request;
            request[0] = "publish";
            request[1] = key;
            request[2] = out;
            redis.request().on(request);
          }
        }
      } else if (json[0] == "sub" && json[1].is<std::string>()) {
        std::string pattern = json[1];
        Json request;
        request[0] = "psubscribe";
        request[1] = pattern;
        redis.request().on(request);
      } else if (json[0] == "log" && json[1].is<std::string>()) {
        std::string stream = json[1];
        Json request;
        request[0] = "xadd";
        request[1] = stream;
        redis.request().on(request);
      }
    }
  });

  auto cborResponder =
      new LambdaFlow<Json, Bytes>([&](Bytes &msg, const Json &docIn) {
        std::string str;
        size_t sz = serializeJson(docIn, str);
        DEBUG("%s", str.c_str());
        if (docIn[0] == "pmessage") {
          CborSerializer ser(10240);
          std::string topic = docIn[2];
          std::string valueString = docIn[3];
          Json valueJson;
          deserializeJson(valueJson, valueString);
          if (valueJson.is<std::string>()) {
            std::string value = valueJson.as<std::string>();
            ser.begin().add("pub").add(topic).add(value).end();
          } else if (valueJson.is<uint64_t>()) {
            uint64_t value = valueJson.as<uint64_t>();
            ser.begin().add("pub").add(topic).add(value).end();
          } 
          else if (valueJson.is<std::string>()) {
            std::string value = valueJson.as<std::string>();
            ser.begin().add("pub").add(topic).add(value).end();
          } else {
            ser.begin().add("pub").add(topic).add(valueString).end();
          }
          msg = ser.toBytes();
          INFO("TXD : %s", cborToJson(msg).toString().c_str());
          INFO("TXD : %s ",hexDump(ser.toBytes()).c_str());
          return true;
        }
        return false;
      });
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

  } else if (framing == "ppp" && format == "json") {
    serial.incoming() >> ppp.deframe() >> bytesToJson >> redis.request();
    redis.response() >> jsonToBytes >> ppp.frame() >> serial.outgoing();

    ppp.garbage() >> [&](const Bytes &bs) {
      INFO("RXD[%d] : %s%s%s", bs.size(), ColorOrange, charDump(bs).c_str(),
           ColorDefault);
    };

  } else if (framing == "ppp" && format == "cbor") {
   /* TimerSource *pinger = new TimerSource(workerThread, 10000, true, "pinger");
    *pinger >> [&](const TimerMsg &) {
      CborSerializer serializer(1024);
      Bytes bs = serializer.begin().add("ping").end().toBytes();
      ppp.frame().on(bs);
      INFO("TXD : %s ", cborDump(bs).c_str());
    };*/

    serial.incoming() >> ppp.deframe() >> cborRequester;
    redis.response() >> cborResponder >> ppp.frame() >> serial.outgoing();

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

Json cborToJson(const Bytes &frame) {
  char *ptr;
  size_t size = 10240;
  FILE *out = open_memstream(&ptr, &size);
  CborParser decoder;
  CborValue root;
  Json json;
  json["error"] = "Failed";
  // INFO("RXD[%d] %s", frame.size(), hexDump(frame).c_str());
  if (cbor_parser_init(frame.data(), frame.size(), 0, &decoder, &root) ==
      CborNoError) {
    if (cbor_value_to_json(out, &root, 0) == CborNoError) {
      fclose(out);
      std::string js = ptr;
      auto erc = deserializeJson(json, js);
      if (erc == DeserializationError::Ok) {
        return json;
      }
    }
  }

  return json;
}