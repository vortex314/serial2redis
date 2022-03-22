
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
#include <Common.h>

Log logger;

//====================================================


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
//================================================================

class ClientProxy : public Actor {
  UdpAddress _sourceAddress;
  QueueFlow<std::string> _incoming;
  QueueFlow<std::string> _outgoing;
  String nodeName;
  redisAsyncContext *_ac;
  std::string _node;
  bool _connected;
  std::string _redisHost;
  uint16_t _redisPort;
  DynamicJsonDocument _docIn;

 public:
  ClientProxy(Thread &thread, JsonObject config, UdpAddress source)
      : Actor(thread),
        _sourceAddress(source),
        _incoming(10, "incoming"),
        _outgoing(5, "outgoing"),
        _docIn(10240) {
    INFO(" created clientProxy %s ", _sourceAddress.toString().c_str());
    _incoming.async(thread);
    _outgoing.async(thread);
    _redisHost = config["host"] | "localhost";
    _redisPort = config["port"] | 6379;
  };

  static void onPush(redisAsyncContext *c, void *reply) {
    INFO(" PUSH received ");
  }

  static void free_privdata(void *pvdata) {}

  int connect() {
    INFO("Connecting to Redis %s:%d as '%s'.", _redisHost.c_str(), _redisPort,
         _node.c_str());
    redisOptions options = {0};
    REDIS_OPTIONS_SET_TCP(&options, _redisHost.c_str(), _redisPort);
    options.connect_timeout = new timeval{3, 0};  // 3 sec
    options.command_timeout = new timeval{3, 0};  // 3 sec
    REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);
    //    redisAsyncInitialize()
    _ac = redisAsyncConnect(Sys::hostname(), 6379);

    if (_ac == NULL || _ac->err) {
      INFO(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
      return ENOTCONN;
    }
    _ac->c.privdata = this;
    redisAsyncSetConnectCallback(
        _ac, [](const redisAsyncContext *ac, int status) {
          INFO("REDIS connected : %d", status);
          ClientProxy *me = (ClientProxy *)ac->c.privdata;
          me->_connected = true;
        });
    redisAsyncSetPushCallback(_ac, onPush);

    int rc = redisAsyncSetDisconnectCallback(
        _ac, [](const redisAsyncContext *ac, int status) {
          WARN("REDIS disconnected : %d", status);
          ClientProxy *me = (ClientProxy *)ac->c.privdata;
          me->_connected = false;
          me->disconnect();
          me->connect();
        });

    thread().addErrorInvoker(_ac->c.fd, [&](int) { WARN(" error on fd "); });
    thread().addReadInvoker(_ac->c.fd, [&](int) { redisAsyncHandleRead(_ac); });
    thread().addWriteInvoker(_ac->c.fd,
                             [&](int) { redisAsyncHandleWrite(_ac); });
    _connected = true;
    return 0;
  }

  void disconnect() { thread().deleteInvoker(_ac->c.fd); }

  static void replyHandler(redisAsyncContext *c, void *reply, void *me) {
    ClientProxy *proxy = (ClientProxy *)me;
    if (reply) {
      DynamicJsonDocument doc(10240);
      replyToJson(doc.as<JsonVariant>(),(redisReply *)reply);
      std::string resp;
      serializeJson(doc, resp);
      INFO(" reply %s ", resp.c_str());
      proxy->_outgoing.on(resp);
    } else {
      WARN("reply is null ");
    }
  }
  void init() {
    /*  _incoming >> [&](const std::string &bs) {
        INFO(" %s client rxd %s ", _sourceAddress.toString().c_str(),
             hexDump(bs).c_str());
      };*/

    _incoming >> new SinkFunction<std::string>([&](const std::string &frame) {
      INFO("REQ : '%s'", frame.c_str());
      _docIn.clear();
      deserializeJson(_docIn, frame);
      JsonArray array = _docIn.as<JsonArray>();
      const char *argv[100];
      int argc;
      for (int i = 0; i < array.size(); i++)
        argv[i] = array[i].as<const char *>();
      argc = array.size();
      redisAsyncCommandArgv(_ac, replyHandler, this, argc, argv, NULL);
      //     redisAsyncCommand(_ac, replyHandler, this, frame.c_str());
      redisAsyncWrite(_ac);
    });
  }
  Sink<std::string> &incoming() { return _incoming; }
  Source<std::string> &outgoing() { return _outgoing; }
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
    std::string payload =
        std::string((const char *)udpMsg.message.data(), udpMsg.message.size());
    INFO(" UDP RXD %s => %s : %s", udpMsg.src.toString().c_str(),
         udpMsg.dst.toString().c_str(), payload.c_str());

    UdpAddress udpSource = udpMsg.src;
    ClientProxy *clientProxy;
    auto it = clients.find(udpSource);
    if (it == clients.end()) {
      clientProxy = new ClientProxy(workerThread, brokerConfig, udpSource);
      clientProxy->init();
      clientProxy->outgoing() >> [&, clientProxy](const std::string &bs) {
        UdpMsg msg;
        msg.message = std::vector<uint8_t>(bs.data(), bs.data() + bs.size());
        msg.dst = clientProxy->src();
        msg.src = serverAddress;
        INFO("TXD UDP %s=>%s:%s ", msg.src.toString().c_str(),
             msg.dst.toString().c_str(),
             std::string((const char *)msg.message.data(), msg.message.size())
                 .c_str());
        udpSession.send().on(msg);
      };
      clients.emplace(udpSource, clientProxy);
      clientProxy->connect();
    } else {
      clientProxy = it->second;
    }
    clientProxy->incoming().on(payload);
    // create new instance for broker connection
    // connect instrance to UdpMsg Stream
  };

  workerThread.run();
}
