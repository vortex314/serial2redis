#include <ConfigFile.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "errno.h"

std::string loadFile(const char *name) {
  std::string str = "{}";

  FILE *file = fopen(name, "r");
  if (file != NULL) {
    str = "";
    char buffer[256];
    while (true) {
      int result = fread(buffer, 1, 256, file);
      if (result <= 0) break;
      str.append(buffer, result);
    }
    fclose(file);
  } else {
    WARN(" cannot open %s : %d = %s", name, errno, strerror(errno));
  }
  return str;
}
#include <LogFile.h>
LogFile *logFile;

void logFileWriter(char *line, size_t length) { logFile->append(line, length); }

void logConfig(JsonObject config) {
  if (!config.containsKey("log")) return;
  JsonObject logConfig = config["log"];
  std::string level = logConfig["level"] | "info";
  if (level == "debug") {
    logger.setLevel(Log::L_DEBUG);
  } else if (level == "info") {
    logger.setLevel(Log::L_INFO);
  } else if (level == "warn") {
    logger.setLevel(Log::L_WARN);
  } else if (level == "error") {
    logger.setLevel(Log::L_ERROR);
  }
  if (logConfig["prefix"]) {
    std::string prefix = logConfig["prefix"] | "logFile";
    uint32_t count = logConfig["count"] | 5;
    uint32_t limit = logConfig["limit"] | 1000000;
    logFile = new LogFile(prefix.c_str(), count, limit);
    INFO("Logging to file %snn.log", prefix.c_str());
    logger.setWriter(logFileWriter);
  }
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
        printf("Usage %s -f<configFile.json>\n", argv[0]);
        break;
      default:
        WARN("Usage %s -f<configFile.json>\n", argv[0]);
        abort();
    }
  std::string s;
  serializeJson(cfg, s);
  INFO("config:%s", s.c_str());
  logConfig(cfg);
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
