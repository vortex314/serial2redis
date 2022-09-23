
#include <ArduinoJson.h>
#include <ConfigFile.h>
#include <Flows.h>
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
void deepMerge(JsonVariant dst, JsonVariant src);

//================================================================
class RedisProxy : public Actor {
  ValueFlow<Bytes> _toRedis;
  ValueFlow<Bytes> _fromRedis;
  bool _connected;
  Redis _redis;
  uint64_t _lastMessage;
  LambdaFlow<Bytes, Json> *_stringToJson;
  LambdaFlow<Json, Bytes> *_jsonToString;

 public:
  RedisProxy(Thread &thread, Json &config);
  ~RedisProxy();

  int connect();
  void stop();
  void disconnect();

  Sink<Bytes> &toRedis();
  Source<Bytes> &fromRedis();
  uint64_t inactivity();
};

//==========================================================================
int main(int argc, char **argv) {
  Json config;
  config["udp"]["port"] = 9001;
  config["udp"]["net"] = "0.0.0.0";
  config["redis"]["host"] = "localhost";
  config["redis"]["port"] = 6379;
  config["proxy"]["timeout"] = 5000;

  configurator(config, argc, argv);
  std::string s;
  serializeJson(config, s);
  INFO("config:%s", s.c_str());

  Thread workerThread("worker");
  uint32_t proxyTimeout = config["proxy"]["timeout"];

  JsonObject brokerConfig = config["redis"];
  std::map<UdpAddress, RedisProxy *> clients;

  JsonObject udpConfig = config["udp"];
  UdpAddress serverAddress(udpConfig["net"], udpConfig["port"]);
  SessionUdp udpServer(workerThread, serverAddress);
  udpServer.connect();

  udpServer.recv() >> [&](const UdpMsg &udpMsg) {
    Bytes payload = udpMsg.payload;
    DEBUG("UDP RXD %s => %s : %s", udpMsg.src.toString().c_str(),
          udpMsg.dst.toString().c_str(), hexDump(payload).c_str());

    UdpAddress udpSource = udpMsg.src;
    RedisProxy *redisProxy;

    auto it = clients.find(udpSource);
    if (it == clients.end()) {
      redisProxy = new RedisProxy(workerThread, config);
      redisProxy->fromRedis() >> [&, udpSource](const Bytes &bs) {
        UdpMsg msg{serverAddress, udpSource, bs};
        INFO("UDP TXD %s => %s : %s ", msg.src.toString().c_str(),
             msg.dst.toString().c_str(), hexDump(msg.payload).c_str());
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

RedisProxy::RedisProxy(Thread &thread, Json &config)
    : Actor(thread), _redis(thread, config["redis"].as<JsonObject>()) {
  std::string format = config["udp"]["format"] | "json";
  if (format == " json") {
    _redis.response() >> responseToString() >> stringToBytes() >> _fromRedis;
    _toRedis >> bytesToString() >> stringToRequest() >> _redis.request();
    _toRedis >> [&](const Bytes &s) { _lastMessage = Sys::millis(); };
  } else if (format == "cbor") {
    _redis.response() >> responseToCbor() >> _fromRedis;
    _toRedis >> cborToRequest() >> _redis.request();
    _toRedis >> [&](const Bytes &s) { _lastMessage = Sys::millis(); };
  } else {
    _redis.response() >> responseToBytes() >> _fromRedis;
    _toRedis >> bytesToRequest() >> _redis.request();
    _toRedis >> [&](const Bytes &s) { _lastMessage = Sys::millis(); };
  }
}

RedisProxy::~RedisProxy() {
  INFO("dtor RedisProxy")
  delete _stringToJson;
  delete _jsonToString;
}

int RedisProxy::connect() { return _redis.connect(); }
void RedisProxy::stop() { _redis.stop(); }
void RedisProxy::disconnect() { _redis.disconnect(); }
Sink<Bytes> &RedisProxy::toRedis() { return _toRedis; }
Source<Bytes> &RedisProxy::fromRedis() { return _fromRedis; }
uint64_t RedisProxy::inactivity() { return Sys::millis() - _lastMessage; }
