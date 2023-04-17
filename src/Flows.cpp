
#include <Flows.h>

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

Flow<Bytes, Json> *cborToRequest() {
  return new Flow<Bytes, Json>([](Json &request, const Bytes &frame) {
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
              request[0] = "publish";
              request[1] = prefix + key;
              request[2] = out;
              return true;
            }
            return false;  // don't emit last so return false
          } else {
            std::string out;
            std::string key = json[1];
            JsonVariant value = json[2];
            serializeJson(value, out);
            request[0] = "publish";
            request[1] = key;
            request[2] = out;
            return true;
          }
        }
      } else if (json[0] == "sub" && json[1].is<std::string>()) {
        std::string pattern = json[1];
        request[0] = "psubscribe";
        request[1] = pattern;
        return true;
      } else if (json[0] == "log" && json[1].is<std::string>()) {
        std::string stream = json[1];
        request[0] = "xadd";
        request[1] = stream;
        return true;
      }
    }
    return false;
  });
}

Flow<Json, Bytes> *responseToCbor() {
  return new Flow<Json, Bytes>([](Bytes &frame, const Json &response) {
    std::string str;
    size_t sz = serializeJson(response, str);
    DEBUG("%s", str.c_str());
    if (response[0] == "pmessage") {
      CborSerializer ser(10240);
      std::string topic = response[2];
      std::string valueString = response[3];
      Json valueJson;
      deserializeJson(valueJson, valueString);
      if (valueJson.is<std::string>()) {
        std::string value = valueJson.as<std::string>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else if (valueJson.is<int64_t>()) {
        int64_t value = valueJson.as<int64_t>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else if (valueJson.is<uint64_t>()) {
        uint64_t value = valueJson.as<uint64_t>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else if (valueJson.is<float>()) {
        float value = valueJson.as<float>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else if (valueJson.is<std::string>()) {
        std::string value = valueJson.as<std::string>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else if (valueJson.is<bool>()) {
        bool value = valueJson.as<bool>();
        ser.begin().add("pub").add(topic).add(value).end();
      } else {
        ser.begin().add("pub").add(topic).add(valueString).end();
      }
      frame = ser.toBytes();
      INFO("TXD : %s", cborToJson(frame).toString().c_str());
      DEBUG("TXD : %s ", hexDump(ser.toBytes()).c_str());
      return true;
    }
    return false;
  });
}

Flow<Bytes, Json> *bytesToRequest() {
  return new Flow<Bytes, Json>([](Json &request, const Bytes &frame) {
    std::string s = std::string(frame.begin(), frame.end());
    if (s.c_str() == nullptr) return false;
    request.clear();
    auto rc = deserializeJson(request, s);
    if (rc != DeserializationError::Ok) {
      INFO("RXD[%d] : %s%s%s", s.length(), ColorOrange, s.c_str(),
           ColorDefault);
    } else {
      INFO("RXD[%d] : '%s'", s.length(), s.c_str());
    }
    return rc == DeserializationError::Ok;
  });
}

Flow<Json, Bytes>* responseToBytes() {
  return new Flow<Json, Bytes>([](Bytes &msg, const Json &response) {
    std::string str;
    size_t sz = serializeJson(response, str);
    INFO("TXD[%d] : '%s'", str.length(), str.c_str());
    msg = Bytes(str.begin(), str.end());
    return str.size() > 0;
  });
}

Flow<std::string, Json>* stringToRequest() {
  return new Flow<std::string, Json>(
      [](Json &request, const std::string &frame) {
        if (frame.c_str() == nullptr) return false;
        request.clear();
        auto rc = deserializeJson(request, frame);
        Bytes bs = Bytes(frame.data(), frame.data() + frame.size());
        if (rc != DeserializationError::Ok) {
          WARN("NO JSON [%d]: %s%s%s %s", frame.length(), ColorOrange,
               frame.c_str(), ColorDefault, hexDump(bs, ' ').c_str());
        }
        return rc == DeserializationError::Ok;
      });
}

Flow<Json, std::string> *responseToString() {
  return new Flow<Json, std::string>(
      [](std::string &msg, const Json &response) {
        std::string str;
        size_t sz = serializeJson(response, str);
        INFO("TXD[%d] : '%s'", str.length(), str.c_str());
        msg = str;
        return str.size() > 0;
      });
}

Flow<Bytes, std::string> *bytesToString() {
  return new Flow<Bytes, std::string>(
      [](std::string &msg, const Bytes &frame) {
        std::string s = std::string(frame.begin(), frame.end());
        msg = s;
        return s.size() > 0;
      });
}
Flow<std::string, Bytes> *stringToBytes() {
  return new Flow<std::string, Bytes>(
      [](Bytes &msg, const std::string &frame) {
        msg = Bytes(frame.data(), frame.data() + frame.size());
        return msg.size() > 0;
      });
}