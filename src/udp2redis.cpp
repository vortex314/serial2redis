
#include <ArduinoJson.h>
#include <BrokerRedisJson.h>
#include <Fnv.h>
#include <Frame.h>
#include <Log.h>
#include <SessionUdp.h>
#include <StringUtility.h>
#include <Udp.h>
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

//================================================================

class ClientProxy : public Actor {
  BrokerRedis _broker;
  UdpAddress _sourceAddress;
  QueueFlow<Bytes> _incoming;
  QueueFlow<Bytes> _outgoing;
  String nodeName;

 public:
  ClientProxy(Thread &thread, JsonObject config, UdpAddress source)
      : Actor(thread),
        _broker(thread, config),
        _sourceAddress(source),
        _incoming(10, "incoming"),
        _outgoing(5, "outgoing") {
    INFO(" created clientProxy %s ", _sourceAddress.toString().c_str());
    _incoming.async(thread);
    _outgoing.async(thread);
  };
  void init() {
    _broker.init();
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
          if (_broker.connected()) {
            _broker.publish(msg[1], msg[2]);
          }
          break;
        }
        case H("PSUBSCRIBE"): {
          _broker.subscribe(msg[1]);
          _broker.request("PSUBSCRIBE %s",msg[1].c_str(),[](JsonArray reply){
            
          })
          break;
        }
        case H("HELLO"): {
          validate(msg[1], "s");
          std::string nodeName = msg[1];
          _broker.connect(nodeName);
          String topic = "dst/";
          topic += nodeName;
          topic += "/*";
          _broker.subscribe(topic);
          break;
        }
      }
    });

    _incoming >> getAnyMsg;

    SinkFunction<String> redisLogStream([&](const String &bs) {
      static String buffer;
      for (uint8_t b : bs) {
        if (b == '\n') {
          printf("%s%s%s\n", ColorOrange, buffer.c_str(), ColorDefault);
          _broker.command("XADD logs * node %s message %s ", nodeName.c_str(),
                          buffer.c_str());
          buffer.clear();
        } else if (b == '\r') {  // drop
        } else {
          buffer += (char)b;
        }
      }
    });

    // serialSession.logs() >> redisLogStream;

    /*
          ::write(1, ColorOrange, strlen(ColorOrange));
        ::write(1, bs.data(), bs.size());
        ::write(1, ColorDefault, strlen(ColorDefault));
    */
    _broker.incoming() >>
        new LambdaFlow<PubMsg, Bytes>([&](Bytes &msg, const PubMsg &pub) {
          DynamicJsonDocument out(10240);
          std::string js;
          out[0] = H("PUBLISH");
          out[1] = pub.payload;
          serializeJson(out, js);
          js += "CRC";

          msg = Bytes(js.begin(), js.end());
          return true;
        }) >>
        _outgoing;
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
  BrokerRedis brokerProxy(workerThread, brokerConfig);
  std::map<UdpAddress, ClientProxy *> clients;

  udpSession.init();
  udpSession.connect();
  brokerProxy.init();
  brokerProxy.connect("udp-proxy");
  UdpAddress serverAddress;
  UdpAddress::fromUri(serverAddress, "0.0.0.0:9999");
  INFO("%s", serverAddress.toString().c_str());

  udpSession.recv() >> [&](const UdpMsg &udpMsg) {
    //   INFO(" UDP RXD %s => %s ", udpMsg.src.toString().c_str(),
    //        udpMsg.dst.toString().c_str());
    UdpAddress src = udpMsg.src;
    ClientProxy *clientProxy;
    auto it = clients.find(src);
    if (it == clients.end()) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, src);
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
      clients.emplace(src, clientProxy);
    } else {
      clientProxy = it->second;
    }
    clientProxy->incoming().on(udpMsg.message);
    // create new instance for broker connection
    // connect instrance to UdpMsg Stream
  };

  workerThread.run();
}
