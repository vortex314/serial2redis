#include <Redis.h>

Redis::Redis(Thread &thread, JsonObject config)
    : Actor(thread), _request(10, "request"), _response(5, "response") {
  _request.async(thread);
  _response.async(thread);
  _redisHost = config["host"] | "localhost";
  _redisPort = config["port"] | 6379;
  _ac = 0;
  _reconnectOnConnectionLoss = true;
};

Redis::~Redis() {
  INFO("~Redis()");
  _reconnectOnConnectionLoss = false;
  thread().deleteInvoker(_ac->c.fd);

  // redisAsyncSetDisconnectCallback(_ac, NULL); // this doens't work a second
  // time redisAsyncSetConnectCallback(_ac, NULL);
  if (_connected) disconnect();
  //  if (_ac) redisAsyncFree(_ac);
}

void Redis::onPush(redisAsyncContext *c, void *reply) {
  INFO(" PUSH received ");
}

int Redis::connect() {
  INFO("Connecting to Redis %s:%d.", _redisHost.c_str(), _redisPort);
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
  redisAsyncSetConnectCallback(_ac,
                               [](const redisAsyncContext *ac, int status) {
                                 INFO("REDIS connected : %d", status);
                                 Redis *me = (Redis *)ac->c.privdata;
                                 me->_connected = true;
                               });
  redisAsyncSetPushCallback(_ac, onPush);

  int rc = redisAsyncSetDisconnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        WARN("REDIS disconnected : %d", status);
        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = false;
        me->thread().deleteInvoker(me->_ac->c.fd);
        if (me->_reconnectOnConnectionLoss) me->connect();
      });

  thread().addErrorInvoker(_ac->c.fd, [&](int) { WARN(" error on fd "); });
  thread().addReadInvoker(_ac->c.fd, [&](int) { redisAsyncHandleRead(_ac); });
  thread().addWriteInvoker(_ac->c.fd, [&](int) { redisAsyncHandleWrite(_ac); });
  _connected = true;
  return 0;
}

void Redis::disconnect() {
  INFO(" disconnect called");
  redisAsyncDisconnect(_ac);
  _connected = false;
}

void Redis::replyHandler(redisAsyncContext *c, void *reply, void *me) {
  Redis *redis = (Redis *)me;
  if (reply) {
    Json doc;
    replyToJson(doc.as<JsonVariant>(), (redisReply *)reply);
    redis->_response.on(doc);
  } else {
    WARN("reply is null ");
    Json doc;
    doc.to<JsonVariant>().set(nullptr);
    redis->_response.on(doc);
  }
}
void Redis::init() {
  _request >> new SinkFunction<Json>([&](const Json &docIn) {
    if (!_connected || !docIn.is<JsonArray>()) return;
    Json *js = (Json *)&docIn;
    JsonArray array = js->as<JsonArray>();
    const char *argv[100];
    int argc;
    for (int i = 0; i < array.size(); i++) {
      if (!array[i].is<const char *>()) return;
      argv[i] = array[i].as<const char *>();
    }
    argc = array.size();
    redisAsyncCommandArgv(_ac, replyHandler, this, argc, argv, NULL);
    redisAsyncWrite(_ac);
  });
}

Sink<Json> &Redis::request() { return _request; }
Source<Json> &Redis::response() { return _response; }
