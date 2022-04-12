
#include <ArduinoJson.h>
#include <Common.h>
#include <Fnv.h>
#include <Frame.h>
#include <Log.h>
#include <Redis.h>
#include <SessionUdp.h>
#include <StringUtility.h>
#include <Udp.h>
#include <assert.h>
#include <stdio.h>

#include <map>
#include <thread>
#include <unordered_map>
#include <utility>

Log logger;

//====================================================
std::string loadFile(const char *name);
bool loadConfig(JsonObject cfg, int argc, char **argv);
void deepMerge(JsonVariant dst, JsonVariantConst src);

//================================================================
class RedisProxy : public Actor {
  ValueFlow<std::string> _toRedis;
  ValueFlow<std::string> _fromRedis;
  bool _connected;
  Redis _redis;
  uint64_t _lastMessage;
  LambdaFlow<std::string, Json> *_stringToJson;
  LambdaFlow<Json, std::string> *_jsonToString;

 public:
  RedisProxy(Thread &thread, JsonObject config);
  ~RedisProxy();

  int connect();
  void stop();
  void disconnect();

  Sink<std::string> &toRedis();
  Source<std::string> &fromRedis();
  uint64_t inactivity();
};

//==========================================================================
int main(int argc, char **argv) {
  INFO("%s start.", argv[0]);
  DynamicJsonDocument config(10240);
  loadConfig(config.to<JsonObject>(), argc, argv);
  Thread workerThread("worker");
  uint32_t proxyTimeout = config["proxy"]["timeout"];

  JsonObject brokerConfig = config["redis"];
  std::map<UdpAddress, RedisProxy *> clients;

  JsonObject udpConfig = config["udp"];
  UdpAddress serverAddress(udpConfig["net"], udpConfig["port"]);
  SessionUdp udpServer(workerThread, serverAddress);
  udpServer.connect();

  udpServer.recv() >> [&](const UdpMsg &udpMsg) {
    std::string payload =
        std::string(udpMsg.message.begin(), udpMsg.message.end());
    DEBUG("UDP RXD %s => %s : %s", udpMsg.src.toString().c_str(),
          udpMsg.dst.toString().c_str(), payload.c_str());

    UdpAddress udpSource = udpMsg.src;
    RedisProxy *redisProxy;

    auto it = clients.find(udpSource);
    if (it == clients.end()) {
      redisProxy = new RedisProxy(workerThread, brokerConfig);
      redisProxy->fromRedis() >> [&, udpSource](const std::string &bs) {
        UdpMsg msg{serverAddress, udpSource,
                   std::vector<uint8_t>(bs.data(), bs.data() + bs.size())};
        DEBUG("UDP TXD %s => %s : %s ", msg.src.toString().c_str(),
              msg.dst.toString().c_str(), bs.c_str());
        udpServer.send().on(msg);
      };
      auto res = clients.insert({udpSource, redisProxy});
      assert(res.second);
      redisProxy->connect();
    } else {
      redisProxy = it->second;
    }
    redisProxy->toRedis().on(payload);
  };

  // cleanup inactive clients

  TimerSource ts(workerThread, 3000, true);
  ts >> [&](const TimerMsg &) {
    INFO(" active clients : %d ", clients.size());
    auto itr = clients.begin();
    while (itr != clients.end()) {
      RedisProxy *proxy = itr->second;
      UdpAddress clientAddress = itr->first;
      if (proxy->inactivity() > proxyTimeout) {
        INFO(" Delete client %s after %d timeout.",
             clientAddress.toString().c_str(), proxyTimeout);
        itr = clients.erase(itr);
        proxy->stop();
        delete proxy;  // disconnect is  async , can we delete ?
      } else {
        ++itr;
      }
    }
  };

  workerThread.run();
}

RedisProxy::RedisProxy(Thread &thread, JsonObject config)
    : Actor(thread), _redis(thread, config) {
  _stringToJson =
      new LambdaFlow<std::string, Json>([&](Json &docIn, const std::string &s) {
        if (s.c_str() == nullptr) return false;
        docIn.clear();
        auto rc = deserializeJson(docIn, s);
        if (rc != DeserializationError::Ok) {
          WARN("NO JSON : %s%s%s", ColorOrange, s.c_str(), ColorDefault);
        }
        return rc == DeserializationError::Ok;
      });
  _jsonToString = new LambdaFlow<Json, std::string>(
      [&](std::string &msg, const Json &docIn) {
        size_t sz = serializeJson(docIn, msg);
        return true;
      });

  _redis.response() >> _jsonToString >> _fromRedis;
  _toRedis >> _stringToJson >> _redis.request();
  _toRedis >> [&](const std::string &s) { _lastMessage = Sys::millis(); };
};

RedisProxy::~RedisProxy() {
  INFO("dtor RedisProxy")
  delete _stringToJson;
  delete _jsonToString;
}

int RedisProxy::connect() { return _redis.connect(); }
void RedisProxy::stop() { _redis.stop(); }
void RedisProxy::disconnect() { _redis.disconnect(); }
Sink<std::string> &RedisProxy::toRedis() { return _toRedis; }
Source<std::string> &RedisProxy::fromRedis() { return _fromRedis; }
uint64_t RedisProxy::inactivity() { return Sys::millis() - _lastMessage; }

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
  while ((c = getopt(argc, argv, "h:p:n:u:t:f:v")) != -1) {
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
      case 'v': {
        logger.setLevel(Log::L_DEBUG);
        break;
      }
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