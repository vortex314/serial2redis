#include <Json.h>

Json JsonNull;

Json::Json() { _type = JS_UNDEFINED; }

Json::~Json() {
  if ( this == & JsonNull ) return;
  if (_type == JS_STRING) {
    delete _string;
  } else if (_type == JS_ARRAY) {
    delete _array;
  } else if (_type == JS_OBJECT) {
    delete _object;
  }
}

Json::Json(const char* s) {
  _type = JS_STRING;
  _string = new std::string(s);
}

Json::Json(bool b) {
  _type = JS_BOOLEAN;
  _boolean = b;
}

Json::Json(double d) {
  _type = JS_NUMBER;
  _number = d;
}

void Json::operator=(double d) {
  _type = JS_NUMBER;
  _number = d;
}

Json& Json::array() {
  if (_type == JS_UNDEFINED) {
    _type = JS_ARRAY;
    _array = new std::vector<Json>();
  } 
}

Json& Json::object() {
  if (_type == JS_UNDEFINED) {
    _type = JS_OBJECT;
    _object = new std::unordered_map<std::string, Json>();
  } 
}

JsonType Json::type() { return _type; }

Json& Json::operator[](const char* field) {
  object();
  if (  _type == JS_OBJECT) {
    if (_object->find(field) != _object->end()) {
      _object->emplace(field, Json());
    }
    return _object->find(field)->second;
  } else {
    return JsonNull;
  }
}

Json& Json::operator[](int idx) {
  array();
  if (  _type == JS_ARRAY) {
    if ( idx>= 0 && _array->size() >idx ) {
      return _array->operator[idx];
    }
  } else {
    return JsonNull;
  }
}

bool Json::isArray() { return _type == JS_ARRAY; }
bool Json::isObject() { return _type == JS_OBJECT; }
bool Json::isString() { return _type == JS_STRING; }

size_t Json::size() {
  if (_type == JS_ARRAY) return _array->size();
  return 0;
}

std::string Json::string() {
  if (_type == JS_STRING) return *_string;
  return std::string("");
}

bool Json::string(std::string& str) {
  if (isString()) {
    str = *_string;
    return true;
  }
  return false;
}

double Json::number() {
  if (_type == JS_NUMBER) return _number;
  return 0;
}
