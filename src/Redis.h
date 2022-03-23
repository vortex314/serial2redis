#ifndef __REDIS_H__
#define __REDIS_H__
#include <ArduinoJson.h>
#include <Common.h>
#include <async.h>
#include <hiredis.h>
#include <limero.h>

class Redis : public Actor {
  QueueFlow<Json> _request;
  QueueFlow<Json> _response;
  redisAsyncContext *_ac;
  std::string _node;
  bool _connected;
  std::string _redisHost;
  uint16_t _redisPort;
  Json _docIn;

 public:
  Redis(Thread &thread, JsonObject config);
  void init();

  static void onPush(redisAsyncContext *c, void *reply);
  static void replyHandler(redisAsyncContext *c, void *reply, void *me);
  static void free_privdata(void *pvdata) {}

  int connect();
  void disconnect();

  Sink<Json> &request();
  Source<Json> &response();
};
#endif