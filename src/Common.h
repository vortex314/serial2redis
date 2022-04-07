#ifndef __COMMON_H__
#define __COMMON_H__
#include <ArduinoJson.h>
#include <async.h>
#include <hiredis.h>

void replyToJson(JsonVariant, redisReply*);
bool validate(JsonVariant js, std::string format);
int token(JsonVariant v);

class Json : public DynamicJsonDocument {
 public:
  Json() : DynamicJsonDocument(10240) {}
  Json(int x) : DynamicJsonDocument(x){};
  Json(DynamicJsonDocument jsd) : DynamicJsonDocument(jsd){}
};
#endif