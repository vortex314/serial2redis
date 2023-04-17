
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
      if (json[0] == "pub" && json[1].is<std::string>()) {
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

Flow<Json, Bytes> *responseToBytes() {
  return new Flow<Json, Bytes>([](Bytes &msg, const Json &response) {
    std::string str;
    size_t sz = serializeJson(response, str);
    INFO("TXD[%d] : '%s'", str.length(), str.c_str());
    msg = Bytes(str.begin(), str.end());
    return str.size() > 0;
  });
}

Flow<std::string, Json> *stringToRequest() {
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
  return new Flow<Bytes, std::string>([](std::string &msg, const Bytes &frame) {
    std::string s = std::string(frame.begin(), frame.end());
    msg = s;
    return s.size() > 0;
  });
}
Flow<std::string, Bytes> *stringToBytes() {
  return new Flow<std::string, Bytes>([](Bytes &msg, const std::string &frame) {
    msg = Bytes(frame.data(), frame.data() + frame.size());
    return msg.size() > 0;
  });
}

Bytes crc16(Bytes data) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < data.size(); i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc = crc >> 1;
      }
    }
  }
  char buf[5];
  sprintf(buf, "%04X", crc);
  return Bytes(buf, buf + 4);
}

String crc16(String data) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < data.length(); i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc = crc >> 1;
      }
    }
  }
  char buf[5];
  sprintf(buf, "%04X", crc);
  return String(buf);
}

Flow<Bytes, Json> *crlfCrcToRequest() {
  return new Flow<Bytes, Json>([](Json &request, const Bytes &frame) {
    std::string line = std::string(frame.begin(), frame.end());
    if (line.c_str() == nullptr || line.size() < 6) return false;
    String crc = line.substr(line.size() - 4, 4);
    String payload = line.substr(0, line.size() - 4);
    String crc2 = crc16(payload);
    if (crc != crc2) {
      WARN("CRC MISMATCH %s != %s for %s ", crc.c_str(), crc2.c_str(),
           line.c_str());
      return false;
    }
    request.clear();
    Json json;
    auto rc = deserializeJson(json, payload);
    if (rc != DeserializationError::Ok) {
      WARN("RXD[%d] : %s%s%s", payload.length(), ColorOrange, payload.c_str(),
           ColorDefault);
    } else {
      DEBUG("RXD[%d] : '%s'", payload.length(), payload.c_str());
    }
    if (json.is<JsonArray>()) {
      if (json[0] == "pub" && json[1].is<std::string>()) {
        std::string out;
        std::string key = json[1];
        JsonVariant value = json[2];
        serializeJson(value, out);
        request[0] = "publish";
        request[1] = key;
        request[2] = out;
        return true;
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
    };
    INFO("RXD[%d] : %s%s%s", line.length(), ColorOrange, line.c_str(),
         ColorDefault);
    return false;
  });
};
Flow<Json, Bytes> *responseToCrlfCrc() {
  return new Flow<Json, Bytes>([](Bytes &frame, const Json &response) {
    std::string str;
    size_t sz = serializeJson(response, str);
    DEBUG("%s", str.c_str());
    if (response[0] == "pmessage") {
      Json txd;
      std::string topic = response[2];
      std::string valueString = response[3];
      Json valueJson;
      deserializeJson(valueJson, valueString);
      txd[0] = "pub";
      txd[1] = topic;
      txd[2] = valueJson;
      std::string txdString;
      serializeJson(txd, txdString);
      frame = Bytes(txdString.data(), txdString.data() + txdString.size());
    } else if (response[0] == "message") {
      Json txd;
      std::string topic = response[1];
      std::string valueString = response[2];
      Json valueJson;
      deserializeJson(valueJson, valueString);
      txd[0] = "pub";
      txd[1] = topic;
      txd[2] = valueJson;
      std::string txdString;
      serializeJson(txd, txdString);
      frame = Bytes(txdString.data(), txdString.data() + txdString.size());
    } else if (response[0] == "xadd") {
      Json txd;
      std::string stream = response[1];
      std::string valueString = response[2];
      Json valueJson;
      deserializeJson(valueJson, valueString);
      txd[0] = "log";
      txd[1] = stream;
      txd[2] = valueJson;
      std::string txdString;
      serializeJson(txd, txdString);
      frame = Bytes(txdString.data(), txdString.data() + txdString.size());
    } else {
      return false;
    }

    if (frame.size() > 0) {
      Bytes crc = crc16(frame);
      frame.insert(frame.end(), crc.begin(), crc.end());
      DEBUG("TXD : %s", String(frame.begin(), frame.end()).c_str());
      return true;
    }
    return false;
  });
}
