
#include <ArduinoJson.h>
#include <Fnv.h>
#include <Frame.h>
#include <Log.h>
#include <SessionUdp.h>
#include <StringUtility.h>
#include <Udp.h>
#include <async.h>
#include <hiredis.h>
#include <stdio.h>

#include <map>
#include <thread>
#include <unordered_map>
#include <utility>

using namespace std;

Log logger;

//====================================================

#define fatal(message)       \
  {                          \
    LOGW << message << LEND; \
    exit(-1);                \
  }

bool loadConfig(JsonObject cfg, int argc, char **argv) {
  cfg["udp"]["port"] = 9999;
  cfg["udp"]["host"] = "0.0.0.0";
  cfg["redis"]["host"] = "localhost";
  cfg["redis"]["port"] = 6379;

  // override args
  int c;
  while ((c = getopt(argc, argv, "h:p:s:u:")) != -1) switch (c) {
      case 'u':
        cfg["udp"]["port"] = atoi(optarg);
        break;
      case 's':
        cfg["udp"]["net"] = optarg;
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
  std::string str;
  serializeJson(cfg, str);
  INFO("%s", str.c_str());
  return true;
};
// "[ssn" "{"
bool validate(JsonVariant js, std::string format) {
  for (auto ch : format) {
    switch (ch) {
      case '[': {
        if (!js.is<JsonArray>()) return false;
        return validate(js[0], format.substr(1));
        break;
      }
      case '{': {
        if (!js.is<JsonObject>()) return false;
        return validate(js[0], format.substr(1));
        break;
      }
      case '}':
        return true;
      case ']':
        return true;
      case 's': {
        if (!js.is<std::string>()) return false;
        break;
      }
      case 'x': {
        return true;
        break;
      }
    }
  }
  return false;
}

int token(JsonVariant v) {
  if (v.is<std::string>()) {
    std::string s = v;
    return H(s.c_str());
  } else if (v.is<int>()) {
    return v.as<int>();
  }
  return -1;
}

void addReply(JsonArray array, redisReply *reply) {
  switch (reply->type) {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_BIGNUM:
    case REDIS_REPLY_VERB:
    case REDIS_REPLY_STRING: {
      array.add(reply->str);
      break;
    };
    case REDIS_REPLY_DOUBLE: {
      array.add(reply->dval);
      break;
    }
    case REDIS_REPLY_INTEGER: {
      array.add(reply->integer);
      break;
    }
    case REDIS_REPLY_NIL: {
      array.add(nullptr);
      break;
    }
    case REDIS_REPLY_BOOL: {
      array.add(reply->integer != 0);
      break;
    }
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_ARRAY: {
      auto nested = array.createNestedArray();
      for (int i = 0; i < reply->elements; i++)
        addReply(nested, reply->element[i]);
      break;
    }
  }
}

//================================================================

class ClientProxy : public Actor {
  UdpAddress _sourceAddress;
  QueueFlow<Bytes> _incoming;
  QueueFlow<Bytes> _outgoing;
  String nodeName;
  redisAsyncContext *_ac;
  std::string _node;
  bool _connected;
  std::string _redisHost;
  uint16_t _redisPort;

 public:
  ClientProxy(Thread &thread, JsonObject config, UdpAddress source)
      : Actor(thread),
        _sourceAddress(source),
        _incoming(10, "incoming"),
        _outgoing(5, "outgoing") {
    INFO(" created clientProxy %s ", _sourceAddress.toString().c_str());
    _incoming.async(thread);
    _outgoing.async(thread);
    _redisHost = config["host"] | "localhost";
    _redisPort = config["port"] | 6379;
  };

  static void onPush(redisAsyncContext *c, void *reply) {}

  int connect() {
    INFO("Connecting to Redis %s:%d as '%s'.", _redisHost.c_str(), _redisPort,
         _node.c_str());
    _ac = redisAsyncConnect(Sys::hostname(), 6379);

    if (_ac == NULL || _ac->err) {
      INFO(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
      return ENOTCONN;
    }
    redisAsyncSetPushCallback(_ac, onPush);

    int rc = redisAsyncSetDisconnectCallback(
        _ac, [](const redisAsyncContext *c, int status) {
          WARN("REDIS disconnected : %d", status);
        });

    thread().addReadInvoker(_ac->c.fd, [&](int) { redisAsyncHandleRead(_ac); });
    thread().addWriteInvoker(_ac->c.fd,
                             [&](int) { redisAsyncHandleWrite(_ac); });
    _connected = true;
  }
  void init() {
    /*  _incoming >> [&](const Bytes &bs) {
        INFO(" %s client rxd %s ", _sourceAddress.toString().c_str(),
             hexDump(bs).c_str());
      };*/

    auto getAnyMsg = new SinkFunction<Bytes>([&](const Bytes &frame) {
      int msgType;
      DynamicJsonDocument msg(10240);
      deserializeJson(msg,
                      std::string((const char *)frame.data(), frame.size()));
      JsonVariant m = msg.as<JsonVariant>();
      //    if (validate(m, "[x")) msgType = token(msg[0]);

      switch (msgType) {
        case H("PUBLISH"): {
          DynamicJsonDocument doc(1024);
          std::string str;
          doc.set(msg[2]);
          serializeJson(doc, str);
          redisAsyncCommand(
              _ac,
              [](redisAsyncContext *c, void *reply, void *me) {
                DynamicJsonDocument doc(10240);
                addReply(doc.as<JsonArray>(), (redisReply *)reply);
              },
              this, "PUBLISH %s %s", msg[1].as<const char *>(), str.c_str());

          break;
        }
        case H("PSUBSCRIBE"): {
          DynamicJsonDocument doc(1024);
          std::string str;
          doc.set(msg[1]);
          serializeJson(doc, str);
          redisAsyncCommand(
              _ac,
              [](redisAsyncContext *c, void *reply, void *me) {
                DynamicJsonDocument doc(10240);
                addReply(doc.as<JsonArray>(), (redisReply *)reply);
              },
              this, "PSUBSCRIBE %s %s", msg[1].as<const char *>(), str.c_str());
          break;
        }
        case H("HELLO"): {
          DynamicJsonDocument doc(1024);
          redisAsyncCommand(
              _ac,
              [](redisAsyncContext *c, void *reply, void *me) {
                DynamicJsonDocument doc(10240);
                addReply(doc.as<JsonArray>(), (redisReply *)reply);
              },
              this, "HELLO");
          break;
          break;
        }
      }
    });

    _incoming >> getAnyMsg;

    SinkFunction<std::string> redisLogStream([&](const std::string &bs) {
      static std::string buffer;
      for (uint8_t b : bs) {
        if (b == '\n') {
          printf("%s%s%s\n", ColorOrange, buffer.c_str(), ColorDefault);
          redisAsyncCommand(
              _ac, [](redisAsyncContext *ac, void *reply, void *me) {}, this,
              "XADD logs * node %s message %s ", nodeName.c_str(),
              buffer.c_str());
          buffer.clear();
        } else if (b == '\r') {  // drop
        } else {
          buffer += (char)b;
        }
      }
    });
  }
  Sink<Bytes> &incoming() { return _incoming; }
  Source<Bytes> &outgoing() { return _outgoing; }
  UdpAddress src() { return _sourceAddress; }
};

//==========================================================================
int main(int argc, char **argv) {
  INFO("Loading configuration.");
  DynamicJsonDocument config(10240);
  loadConfig(config.to<JsonObject>(), argc, argv);
  Thread workerThread("worker");
  JsonObject udpConfig = config["udp"];

  SessionUdp udpSession(workerThread, config["udp"]);

  JsonObject brokerConfig = config["redis"];
  std::map<UdpAddress, ClientProxy *> clients;

  udpSession.init();
  udpSession.connect();
  UdpAddress serverAddress;
  UdpAddress::fromUri(serverAddress, "0.0.0.0:9999");
  INFO("%s", serverAddress.toString().c_str());

  udpSession.recv() >> [&](const UdpMsg &udpMsg) {
    INFO(" UDP RXD %s => %s ", udpMsg.src.toString().c_str(),
         udpMsg.dst.toString().c_str());
    UdpAddress udpSource = udpMsg.src;
    ClientProxy *clientProxy;
    auto it = clients.find(udpSource);
    if (it == clients.end()) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, udpSource);
      clientProxy->init();
      clientProxy->outgoing() >> [&, clientProxy](const Bytes &bs) {
        UdpMsg msg;
        msg.message = bs;
        msg.dst = clientProxy->src();
        msg.src = serverAddress;
        INFO("TXD UDP %s=>%s:%s ", msg.src.toString().c_str(),
             msg.dst.toString().c_str(),
             std::string((const char *)msg.message.data(), msg.message.size())
                 .c_str());
        udpSession.send().on(msg);
      };
      clients.emplace(udpSource, clientProxy);
    } else {
      clientProxy = it->second;
    }
    clientProxy->incoming().on(udpMsg.message);
    // create new instance for broker connection
    // connect instrance to UdpMsg Stream
  };

  workerThread.run();
}
