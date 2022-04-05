#include <Redis.h>
#include <assert.h>

struct RedisReplyContext {
  std::string command;
  Redis *me;
};

void Redis::addWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addWriteInvoker(redis->_ac->c.fd, redis, [](void *pv) {
    redisAsyncHandleWrite(((Redis *)pv)->_ac);
  });
}

void Redis::addReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().addReadInvoker(redis->_ac->c.fd, redis, [](void *pv) {
    redisAsyncHandleRead(((Redis *)pv)->_ac);
  });
}

void Redis::delWriteFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().delWriteInvoker(redis->_ac->c.fd);
}

void Redis::delReadFd(void *pv) {
  Redis *redis = (Redis *)pv;
  redis->thread().delReadInvoker(redis->_ac->c.fd);
}

void Redis::cleanupFd(void *pv) {
  Redis *redis = (Redis *)pv;
  if (redis->_ac->c.fd < 0) {
    WARN(" cleanupFd for negative fd");
    return;
  }
  redis->thread().delAllInvoker(redis->_ac->c.fd);
}

Redis::Redis(Thread &thread, JsonObject config)
    : Actor(thread), _request(10, "request") {
  _request.async(thread);
  _response.async(thread);
  _redisHost = config["host"] | "localhost";
  _redisPort = config["port"] | 6379;
  _reconnectOnConnectionLoss = config["reconnectOnConnectionLoss"] | true;
  _addReplyContext = config["addReplyContext"] | false;
  _ac = 0;

  _jsonToRedis = new SinkFunction<Json>([&](const Json &docIn) {
    if (!_connected()) return;
    std::string s;
    serializeJson(docIn, s);
    INFO("Redis:request  %s ", s.c_str());
    if (!docIn.is<JsonArray>()) return;
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
  _request >> _jsonToRedis;
};

Redis::~Redis() {
  INFO("~Redis()");
  _reconnectOnConnectionLoss = false;
  if (_connected()) disconnect();
  cleanupFd(this);
  delete _jsonToRedis;
}

void Redis::free_privdata(void *pvdata) {
  WARN(" freeing private data of context %X", pvdata);
}

void Redis::onPush(redisAsyncContext *ac, void *reply) {
  INFO(" PUSH received ");  // why do I never come here ????
}

int Redis::connect() {
  INFO("Connecting to Redis %s:%d ... ", _redisHost.c_str(), _redisPort);
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _redisHost.c_str(), _redisPort);
  options.connect_timeout = new timeval{3, 0};  // 3 sec
  options.async_push_cb = onPush;
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);
  _ac = redisAsyncConnectWithOptions(&options);

  if (_ac == NULL || _ac->err) {
    WARN(" Connection %s:%d failed", _redisHost.c_str(), _redisPort);
    return ENOTCONN;
  }

  _ac->ev.addRead = addReadFd;
  _ac->ev.delRead = delReadFd;
  _ac->ev.addWrite = addWriteFd;
  _ac->ev.delWrite = delWriteFd;
  _ac->ev.cleanup = cleanupFd;
  _ac->ev.data = this;

  int rc = redisAsyncSetConnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        INFO("Redis connected status : %d fd : %d ", status, ac->c.fd);
        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = true;
      });

  assert(rc == 0);

  rc = redisAsyncSetDisconnectCallback(
      _ac, [](const redisAsyncContext *ac, int status) {
        WARN("Redis disconnected status : %d fd : %d ", status, ac->c.fd)

        Redis *me = (Redis *)ac->c.privdata;
        me->_connected = false;
        if (me->_reconnectOnConnectionLoss) me->connect();
      });

  assert(rc == 0);

  auto oldFn = redisAsyncSetPushCallback(_ac, onPush);

  return 0;
}

void Redis::disconnect() {
  INFO(" disconnect called");
  redisAsyncDisconnect(_ac);
}

void Redis::replyHandler(redisAsyncContext *ac, void *repl, void *pv) {
  redisReply *reply = (redisReply *)repl;
  Json doc;
  if (reply == 0) {
    WARN(" replyHandler caught null ");
    return;  // disconnect ?
  };
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
  INFO("Redis:reply '%s' =>  %s ", redisReplyContext->command.c_str(), str.c_str());
  delete redisReplyContext;
}

Sink<Json> &Redis::request() { return _request; }
Source<Json> &Redis::response() { return _response; }
