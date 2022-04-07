#include <Common.h>

void replyToJson(JsonVariant result, redisReply *reply) {
  if (reply == 0) {
    result.set(nullptr);
  };
  switch (reply->type) {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_BIGNUM:
    case REDIS_REPLY_VERB:
    case REDIS_REPLY_STRING:
      result.set(reply->str);
      break;

    case REDIS_REPLY_DOUBLE:
      result.set(reply->dval);
      break;

    case REDIS_REPLY_INTEGER:
      result.set(reply->integer);
      break;

    case REDIS_REPLY_NIL:
      result.set(nullptr);
      break;

    case REDIS_REPLY_BOOL:
      result.set(reply->integer != 0);
      break;

    case REDIS_REPLY_MAP:
      for (int i = 0; i < reply->elements; i += 2)
        replyToJson(result[reply->element[i]->str].to<JsonVariant>(),
                    reply->element[i + 1]);
      break;

    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_ARRAY:
      for (int i = 0; i < reply->elements; i++)
        replyToJson(result.addElement(), reply->element[i]);
      break;
    default: {
      result.set(" Unhandled reply to JSON type");
      break;
    }
  }
}

DynamicJsonDocument replyToJson(redisReply *reply) {
  DynamicJsonDocument doc(10240);
  replyToJson(doc.as<JsonVariant>(), reply);
  doc.shrinkToFit();
  return doc;
}

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

