
#include <BrokerRedisJson.h>
#include <StringUtility.h>

BrokerRedis::BrokerRedis(Thread &thread, JsonObject cfg)
    : _thread(thread),
      _incoming(10, "incoming"),
      _outgoing(10, "outgoing"),
      _reconnectTimer(thread, 3000, true, "reconnectTimer")
{
  _hostname = cfg["host"] | "localhost";
  _port = cfg["port"] | 6379;

  _incoming.async(thread);
  _outgoing.async(thread);
  _reconnectHandler.async(thread);
  _reconnectHandler >> [&](const bool &)
  { reconnect(); };

  connected = false;
  _reconnectTimer >> [&](const TimerMsg &)
  {
    if (!connected())
    {
      if (connect(_node) == 0)
        subscribeAll();
    }
  };
  _incoming >> [&](const PubMsg &msg)
  {
    std::string s;
    serializeJson(msg.payload, s);
    DEBUG("Redis RXD %s : %s ", msg.topic.c_str(), s.c_str());
  };
  _outgoing >> [&](const PubMsg &msg)
  {
    std::string s;
    serializeJson(msg.payload, s);
    DEBUG("publish %s %s ", msg.topic.c_str(), s.c_str());
    publish(msg.topic, s.c_str());
  };
}

BrokerRedis::~BrokerRedis() {}
//
//=========================================================================
//
void BrokerRedis::onReply(redisAsyncContext *c, void *reply, void *me)
{
  DynamicJsonDocument doc(10240);
  JsonArray array=doc.as<JsonArray>();
  array.add("");
  
  //  INFO("%s",replyToString(reply).c_str());
  BrokerRedis *pBroker = (BrokerRedis *)me;
  if (reply == NULL)
    return;
  redisReply *r = (redisReply *)reply;
  if (r->type == REDIS_REPLY_ARRAY)
  {
    if (strcmp(r->element[0]->str, "pmessage") == 0)
    {
      std::string topic = r->element[2]->str;
      std::string payload = r->element[3]->str;
      DynamicJsonDocument json(10240);
      deserializeJson(json, payload);
      json.shrinkToFit();
      //      pBroker->_incoming.on({topic, json});
    }
    else
    {
      WARN("unexpected array %s ", r->element[0]->str);
    }
  }
  else
  {
    WARN(" unexpected reply ");
  }
}
//
//=========================================================================
//
void BrokerRedis::onPush(redisAsyncContext *ac, void *reply)
{
  /* Handle the reply */
  INFO(" received push ");
  /* Note:  Because async hiredis always frees replies, you should
            not call freeReplyObject in an async push callback. */
}
//
//=========================================================================
//
bool isFailedReply(redisReply *reply)
{
  return reply == 0 || reply->type == REDIS_REPLY_ERROR;
}
//
//=========================================================================
//
std::string BrokerRedis::replyToString(void *r)
{
  if (r == 0)
  {
    return "Reply failed ";
  }
  redisReply *reply = (redisReply *)r;

  switch (reply->type)
  {
  case REDIS_REPLY_ARRAY:
  {
    std::string result = "[";
    for (int j = 0; j < reply->elements; j++)
    {
      result += replyToString(reply->element[j]);
      result += ",";
    }
    result += "]";
    return result;
  }
  case REDIS_REPLY_INTEGER:
  {
    return std::to_string(reply->integer);
  }
  case REDIS_REPLY_STRING:
    return stringFormat("'%s'", reply->str);
  case REDIS_REPLY_STATUS:
    return stringFormat("(status) %s", reply->str);
  case REDIS_REPLY_NIL:
    return "(nill)";
  case REDIS_REPLY_ERROR:
    return stringFormat(" (error) %s", reply->str);
  default:
    return stringFormat("unexpected redisReply type : %d", reply->type);
  }
  return "XXX";
}

void free_privdata(void *pvdata) {}

int BrokerRedis::init() { return 0; }

int BrokerRedis::connect(std::string node)
{
  _node = node;
  _dstPrefix = "dst/";
  _dstPrefix += _node + "/";
  _srcPrefix = "src/";
  _srcPrefix += _node + "/";
  if (connected())
  {
    INFO(" Connecting but already connected.");
    connected = true;
    return 0;
  }
  redisOptions options = {0};
  REDIS_OPTIONS_SET_TCP(&options, _hostname.c_str(), _port);
  options.connect_timeout = new timeval{3, 0}; // 3 sec
  options.command_timeout = new timeval{3, 0}; // 3 sec
  REDIS_OPTIONS_SET_PRIVDATA(&options, this, free_privdata);

  INFO("Connecting to Redis %s:%d as '%s'.", _hostname.c_str(), _port,
       _node.c_str());
  _asyncContext = redisAsyncConnect(_hostname.c_str(), 6379);

  if (_asyncContext == NULL || _asyncContext->err)
  {
    INFO(" Connection %s:%d failed", _hostname.c_str(), _port);
    return ENOTCONN;
  }
  inr r = redisSetPushCallback(_asyncContext, onPush);

  int rc = redisAsyncSetDisconnectCallback(
      _asyncContext, [](const redisAsyncContext *c, int status)
      { WARN("REDIS disconnected : %d", status); });

  _thread.addReadInvoker(_asyncContext->c.fd, [&](int)
                         { redisAsyncHandleRead(_asyncContext); });
  _thread.addWriteInvoker(_asyncContext->c.fd, [&](int)
                          { redisAsyncHandleWrite(_asyncContext); });
  connected = true;
  return 0;
}

int BrokerRedis::subscribeAll()
{
  INFO("subscribeAll again");
  if (connected())
  {
    for (auto sub : _subscriptions)
    {
      INFO(" subscribing %s", sub.c_str());
      subscribe(sub);
    }
  }
  return 0;
}

int BrokerRedis::reconnect()
{
  int rc;
  disconnect();
  if ((rc = connect(_node)) == 0)
  {
    subscribeAll();
    return 0;
  }
  else
    return rc;
}

int BrokerRedis::disconnect()
{
  INFO(" disconnecting.");
  if (!connected())
    return 0;
  redisAsyncFree(_asyncContext);
  connected = false;
  return 0;
}

int BrokerRedis::subscribe(std::string pattern)
{
  //  INFO(" REDIS psubscribe %s", pattern.c_str());
  _subscriptions.insert(pattern);
  std::string cmd = stringFormat("PSUBSCRIBE %s", pattern.c_str());
  int rc = redisAsyncCommand(_asyncContext, onReply, this, cmd.c_str());
  if (rc != REDIS_OK)
  {
    WARN("%s failed.", cmd.c_str());
    return EIO;
  }
}

int BrokerRedis::unSubscribe(std::string pattern)
{
  auto it = _subscriptions.find(pattern);
  if (it != _subscriptions.end())
  {
    _subscriptions.erase(pattern);
    int rc = redisAsyncCommand(_asyncContext, onReply, this,
                               "PUNSUBSCRIBE %s", pattern.c_str());
    if (rc != REDIS_OK)
    {
      INFO(" PUNSUBSCRIBE : %s created.", pattern.c_str());
      _subscriptions.erase(pattern);
    }
  }
  return 0;
}

int BrokerRedis::publish(std::string topic, const std::string &bs)
{
  if (!connected())
    return ENOTCONN;
  int rc = redisAsyncCommand(
      _asyncContext, onReply, this, "PUBLISH %s %b", topic.c_str(), bs.data(),
      bs.size());
  if (rc == REDIS_OK)
  {
    INFO("Redis PUBLISH %s : [%d] ", topic.c_str(), bs.size());
  }
  return 0;
}

int BrokerRedis::command(const char *format, ...)
{
  INFO("%s", format);
  if (!connected())
    return ENOTCONN;
  va_list ap;
  va_start(ap, format);
  int rc = redisvAsyncCommand(_asyncContext, onReply,
                                                       this, format, ap);
  va_end(ap);
  return rc==REDIS_OK ? 0 : EFAULT;
}

int BrokerRedis::request(std::string cmd,
                         std::function<void(redisReply *)> func)
{
  if (!connected())
    return ENOTCONN;

  int rc= redisAsyncCommand(_asyncContext, onReply,
                                                      this, cmd.c_str());
  return rc==REDIS_OK ? 0 : EFAULT;

}

#include <regex>
bool BrokerRedis::match(std::string pattern, std::string topic)
{
  std::regex patternRegex(pattern);
  return std::regex_match(topic, patternRegex);
}

redisReply *BrokerRedis::xread(std::string key)
{
  if (!connected())
    return 0;
  return (redisReply *)redisAsyncCommand(_asyncContext, onReply, this,
                                         "XREAD BLOCK 0 STREAMS %s $",
                                         key.c_str());
}