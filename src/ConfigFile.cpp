#include "errno.h"
#include <string.h>
#include <stdio.h>
#include <string>
#include <ConfigFile.h>

std::string loadFile(const char *name) {

  std::string str = "{}";

  FILE *file = fopen(name, "r");
  if (file != NULL) {
    str = "";
    char buffer[256];
    while (true) {
      int result = fread(buffer, 1, 256, file);
      if (result <= 0)
        break;
      str.append(buffer, result);
    }
    fclose(file);
  } else {
    WARN(" cannot open %s : %d = %s", name, errno, strerror(errno));
  }
  return str;
}


bool loadConfig(JsonObject cfg, int argc, char **argv) {
  // defaults

  // override args
  int c;
  while ((c = getopt(argc, argv, "f:v")) != -1) switch (c) {
      case 'f': {
        std::string s = loadFile(optarg);
        DynamicJsonDocument doc(10240);
        deserializeJson(doc, s);
        deepMerge(cfg, doc);
        break;
      }
      case 'v': {
        logger.setLevel(Log::L_DEBUG);
        break;
      }
      case '?':
        printf("Usage %s -f<configFile.json>\n",
               argv[0]);
        break;
      default:
        WARN("Usage %s -f<configFile.json>\n",
             argv[0]);
        abort();
    }
  std::string s;
  serializeJson(cfg, s);
  INFO("config:%s", s.c_str());
  return true;
};

void deepMerge(JsonVariant dst, JsonVariant src) {
  if (src.is<JsonObject>()) {
    for (auto kvp : src.as<JsonObject>()) {
      deepMerge(dst.getOrAddMember(kvp.key()), kvp.value());
    }
  } else {
    dst.set(src);
  }
}

