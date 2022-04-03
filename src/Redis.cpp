#include <Redis.h>

struct RedisReplyContext {
  std::string command;
  Redis *me;
};

void Redis::addWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addWriteInvoker(
      redis->_ac->c.fd, [redis](int) { redisAsyncHandleWrite(redis->_ac); });
}

void Redis::addReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addReadInvoker(
      redis->_ac->c.fd, [redis](int) { redisAsyncHandleRead(redis->_ac); });
}

void Redis::delWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().deleteInvoker(redis->_ac->c.fd);
}

void Redis::delReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().deleteInvoker(redis->_ac->c.fd);
}

void Redis::cleanupFd(void *pv) {
  Redis *redis = (Redis *)pv;
  if (redis->_ac->c.fd<0 ) return;
  redis->thread().deleteInvoker(redis->_ac->c.fd);
}

Redis::Redis(Thread &thread, JsonObject config)
    : Actor(thread), _request(10, "request"), _response(5, "response") {
  _request.async(thread);
  _response.async(thread);
  _redisHost = config["host"] | "localhost";
  _redisPort = config["port"] | 6379;
  _ac = 0;
  _reconnectOnConnectionLoss = true;
  _addReplyContext = true;
};

Redis::~Redis() {
  INFO("~Redis()");
  _reconnectOnConnectionLoss = false;
  if (_connected) disconnect();
  cleanupFd(this);
}

void Redis::onPush(void *pac, void *reply) {
  redisAsyncContext *ac = (redisAsyncContext *)pac;
  INFO(" PUSH received ");
}

int Redis::connect() {
  INFO("Connecting to Redis %s:%d.", _redisHost.c_str(), _redisPort);
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _redisHost.c_str(), _redisPort);
  options.connect_timeout = new timeval{3, 0};  // 3 sec
  options.push_cb = onPush;
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);
  _ac = redisAsyncConnectWithOptions(&options);

  if (_ac == NULL || _ac->err) {
    INFO(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
    return ENOTCONN;
  }
  _ac->c.privdata = this;
  redisAsyncSetConnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        INFO("REDIS %X connected : %d fd : %d ", ac, status, ac->c.fd);
        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = true;
      });

  int rc = redisAsyncSetDisconnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        WARN("REDIS disconnected : %d", status);
        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = false;
        if (me->_reconnectOnConnectionLoss) me->connect();
      });

  _ac->ev.addRead = addReadFd;
  _ac->ev.delRead = delReadFd;
  _ac->ev.addWrite = addWriteFd;
  _ac->ev.delWrite = delWriteFd;
  _ac->ev.cleanup = cleanupFd;
  _ac->ev.data = this;
  /*
    thread().addErrorInvoker(_ac->c.fd, [&](int) { WARN(" error on fd "); });
    thread().addReadInvoker(_ac->c.fd, [&](int) {
      //    INFO("READ");
      redisAsyncHandleRead(_ac);
    });
      thread().addWriteInvoker(_ac->c.fd, [&](int) {
        INFO("WRITE");
        redisAsyncHandleWrite(_ac);
        }); */
  //  redisAsyncWrite(_ac);

  _connected = true;
  return 0;
}

void Redis::disconnect() {
  INFO(" disconnect called");
  redisAsyncDisconnect(_ac);
  _connected = false;
}

void Redis::replyHandler(redisAsyncContext *ac, void *repl, void *pv) {
  redisReply *reply = (redisReply *)repl;
  Json doc;
  if (reply == 0) return;  // disconnect ?
  if (reply->type == REDIS_REPLY_PUSH &&
      strcmp(reply->element[0]->str, "pmessage") == 0) {  // no context
    Redis *redis = (Redis *)ac->c.privdata;
    replyToJson(doc.as<JsonVariant>(), reply);
    std::string str;
    serializeJson(doc, str);
    INFO(" push %s ", str.c_str());
    redis->_response.on(doc);
    return;
  }

  RedisReplyContext *redisReplyContext = (RedisReplyContext *)pv;
  Redis *redis = redisReplyContext->me;

  if (redis->_addReplyContext) {
    doc[0] = redisReplyContext->command;
    replyToJson(doc[1].to<JsonVariant>(), reply);
  } else {
    replyToJson(doc.as<JsonVariant>(), reply);
  }

  std::string str;
  serializeJson(doc, str);
  INFO(" reply on %s : %s ", redisReplyContext->command.c_str(), str.c_str());
  delete redisReplyContext;
}

void Redis::init() {
  _request >> new SinkFunction<Json>([&](const Json &docIn) {
    std::string s;
    serializeJson(docIn, s);
    INFO("Redis:request  %s ", s.c_str());
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
    redisAsyncCommandArgv(_ac, replyHandler,
                          new RedisReplyContext{argv[0], this}, argc, argv,
                          NULL);
  });
}

Sink<Json> &Redis::request() { return _request; }
Source<Json> &Redis::response() { return _response; }
