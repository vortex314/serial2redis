#ifndef __CONFIG_FILE_H__
#include <ArduinoJson.h>
#include <Log.h>
#include <unistd.h>

#include <string>
std::string loadFile(const char *name);
bool loadConfig(JsonObject cfg, int argc,  char **argv);
void deepMerge(JsonVariant dst, JsonVariant src);
#endif