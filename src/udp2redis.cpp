
#include <ArduinoJson.h>
#include <Common.h>
#include <Fnv.h>
#include <Frame.h>
#include <Log.h>
#include <Redis.h>
#include <SessionUdp.h>
#include <StringUtility.h>
#include <Udp.h>
#include <stdio.h>

#include <map>
#include <thread>
#include <unordered_map>
#include <utility>

Log logger;

//====================================================
std::string loadFile(const char *name);

void deepMerge(JsonVariant dst, JsonVariantConst src) {
  if (src.is<JsonObject>()) {
    for (auto kvp : src.as<JsonObject>()) {
      deepMerge(dst.getOrAddMember(kvp.key()), kvp.value());
    }
  } else {
    dst.set(src);
  }
}

bool loadConfig(JsonObject cfg, int argc, char **argv) {
  cfg["udp"]["port"] = 9999;
  cfg["udp"]["net"] = "0.0.0.0";
  cfg["redis"]["host"] = "localhost";
  cfg["redis"]["port"] = 6379;
  cfg["proxy"]["timeout"] = 5000;

  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:n:u:t:f:")) != -1) {
    switch (c) {
      case 'u':
        cfg["udp"]["port"] = atoi(optarg);
        break;
      case 'f': {
        std::string s = loadFile(optarg);
        DynamicJsonDocument doc(10240);
        deserializeJson(doc, s);
        deepMerge(cfg, doc);
        break;
      }
      case 'n':
        cfg["udp"]["net"] = optarg;
        break;
      case 'h':
        cfg["broker"]["host"] = optarg;
        break;
      case 't':
        cfg["proxy"]["timeout"] = atoi(optarg);
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
  }
  std::string str;
  serializeJson(cfg, str);
  INFO("%s", str.c_str());
  return true;
};
//================================================================

class ClientProxy : public Actor {
  UdpAddress _sourceAddress;
  QueueFlow<std::string> _incoming;  // recv UDP
  ValueFlow<std::string> _outgoing;  // send UDP
  bool _connected;
  DynamicJsonDocument _docIn;
  Redis _redis;
  uint64_t _lastMessage;
  LambdaFlow<std::string, Json> *_stringToJson;
  LambdaFlow<Json, std::string> *_jsonToString;

 public:
  ClientProxy(Thread &thread, JsonObject config, UdpAddress source)
      : Actor(thread),
        _sourceAddress(source),
        _incoming(10, "incoming"),
        _docIn(10240),
        _redis(thread, config) {
    INFO(" created clientProxy %s ", _sourceAddress.toString().c_str());
    _stringToJson = new LambdaFlow<std::string, Json>(
        [&](Json &docIn, const std::string &s) {
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
    _jsonToString = new LambdaFlow<Json, std::string>(
        [&](std::string &msg, const Json &docIn) {
          size_t sz = serializeJson(docIn, msg);
          return true;
        });

    _incoming.async(thread);

    _redis.response() >> _jsonToString >> _outgoing;
    _incoming >> _stringToJson >> _redis.request();
    _incoming >> [&](const std::string &s) { _lastMessage = Sys::millis(); };
  };

  ~ClientProxy() {
    INFO(" deleted clientProxy %s ", _sourceAddress.toString().c_str());
    delete _stringToJson;
    delete _jsonToString;
  }

  int connect() { return _redis.connect(); }

  void disconnect() { _redis.disconnect(); }

  Sink<std::string> &incoming() { return _incoming; }
  Source<std::string> &outgoing() { return _outgoing; }
  UdpAddress src() { return _sourceAddress; }
  uint64_t lastMessage() { return _lastMessage; }
};

//==========================================================================
int main(int argc, char **argv) {
  INFO("Loading configuration.");
  DynamicJsonDocument config(10240);
  loadConfig(config.to<JsonObject>(), argc, argv);
  Thread workerThread("worker");
  uint32_t proxyTimeout = config["proxy"]["timeout"];
  JsonObject udpConfig = config["udp"];

  SessionUdp udpSession(workerThread, config["udp"]);

  JsonObject brokerConfig = config["redis"];
  std::map<UdpAddress, ClientProxy *> clients;

  udpSession.init();
  udpSession.connect();
  UdpAddress serverAddress;
  UdpAddress::fromUri(serverAddress,
                      udpConfig["net"].as<std::string>() + ":" +
                          std::to_string(udpConfig["port"].as<int>()));
  INFO("%s", serverAddress.toString().c_str());

  udpSession.recv() >> [&](const UdpMsg &udpMsg) {
    std::string payload =
        std::string(udpMsg.message.begin(), udpMsg.message.end());
    INFO("UDP RXD %s => %s : %s", udpMsg.src.toString().c_str(),
         udpMsg.dst.toString().c_str(), payload.c_str());

    UdpAddress udpSource = udpMsg.src;
    ClientProxy *clientProxy;

    auto it = clients.find(udpSource);
    if (it == clients.end()) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, udpSource);
      clientProxy->outgoing() >> [&, clientProxy](const std::string &bs) {
        UdpMsg msg;
        msg.message = std::vector<uint8_t>(bs.data(), bs.data() + bs.size());
        msg.dst = clientProxy->src();
        msg.src = serverAddress;
        INFO("UDP TXD %s => %s : %s ", msg.src.toString().c_str(),
             msg.dst.toString().c_str(),
             std::string((const char *)msg.message.data(), msg.message.size())
                 .c_str());
        udpSession.send().on(msg);
      };
      auto res = clients.insert(
          std::pair<UdpAddress, ClientProxy *>(udpSource, clientProxy));
      assert(res.second);
      clientProxy->connect();
    } else {
      clientProxy = it->second;
    }
    clientProxy->incoming().on(payload);
  };

  // cleanup inactive clients

  TimerSource ts(workerThread, proxyTimeout, true);
  ts >> [&](const TimerMsg &) {
    INFO(" active clients : %d ", clients.size());
    auto itr = clients.begin();
    while (itr != clients.end()) {
      if (itr->second->lastMessage() < (Sys::millis() - 5000)) {
        INFO(" deleting client after timeout.");
        ClientProxy *proxy = itr->second;
        itr = clients.erase(itr);
        proxy->disconnect();
        delete proxy; // disconnect is  async , can we delete ? 
        INFO("cleanup done");
      } else {
        ++itr;
      }
    }
  };

  workerThread.run();
}
