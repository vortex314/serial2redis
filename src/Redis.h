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
  ValueFlow<bool> _connected;
  std::string _redisHost;
  uint16_t _redisPort;
  Json _docIn;
  bool _reconnectOnConnectionLoss;
  bool _addReplyContext;
  SinkFunction<Json> *_jsonToRedis;
  std::vector<std::string> _ignoreReplies;

 public:
  Redis(Thread &thread, JsonObject config);
  ~Redis();

  static void onPush(redisAsyncContext *ac, void *reply);
  static void replyHandler(redisAsyncContext *ac, void *reply, void *me);
  static void free_privdata(void *pvdata);

  int connect();
  void disconnect();
  void stop();

  Sink<Json> &request();
  Source<Json> &response();
  Source<bool> &connected();

  static void addWriteFd(void *pv);
  static void addReadFd(void *pv);
  static void delWriteFd(void *pv);
  static void delReadFd(void *pv);
  static void cleanupFd(void *pv);
};
#endif