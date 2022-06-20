// Copyright (c) 2013-2017, Matt Godbolt
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <ConfigFile.h>
#include <Log.h>
#include <Redis.h>

#include <memory>
#include <set>
#include <string>

#include "seasocks/PageHandler.h"
#include "seasocks/PrintfLogger.h"
#include "seasocks/Server.h"
#include "seasocks/StringUtil.h"
#include "seasocks/WebSocket.h"
using namespace seasocks;

Log logger;
Thread workerThread("worker");
class MyLogger : public Logger {
 public:
  void log(Level level, const char* message) override {
    switch (level) {
      case Level::Debug:
        DEBUG(message);
        break;
      case Level::Access:
        INFO(message);
        break;
      case Level::Info:
        INFO(message);
        break;
      case Level::Warning:
        WARN(message);
        break;
      case Level::Error:
        ERROR(message);
        break;
      case Level::Severe:
        ERROR(message);
        break;
    }
  }
};

Server server(std::make_shared<MyLogger>());
Json config;

class WsProxy {
  Redis* _redis;
  WebSocket* _ws;

 public:
  WsProxy(WebSocket* ws) : _ws(ws) {
    _redis = new Redis(workerThread, config["redis"]);
    _redis->connect();
  }
  ~WsProxy() {
    _redis->disconnect();
    delete _redis;
  }
  WebSocket* ws() { return _ws; }
  Redis* redis() { return _redis; }
};

// Simple chatroom server, showing how one might use authentication.

namespace {

struct Handler : WebSocket::Handler {
  std::unordered_map<WebSocket*, WsProxy*> _cons;

  void onConnect(WebSocket* con) override {
    WsProxy* proxy = new WsProxy(con);
    _cons.emplace(con, proxy);
    proxy->redis()->response() >> [proxy](const Json& json) {
      std::string str;
      serializeJson(json, str);
      server.execute([proxy, str] { proxy->ws()->send(str.c_str()); });
    };
  }
  void onDisconnect(WebSocket* con) override {
    auto it = _cons.find(con);
    if (it != _cons.end()) {
      delete it->second;
      _cons.erase(it);
    }
  }

  void onData(WebSocket* con, const char* data) override {
    Json jsonIn;
    DeserializationError err = deserializeJson(jsonIn, data);
    auto it = _cons.find(con);
    if (it != _cons.end()) {
      it->second->redis()->request().on(jsonIn);
    }
  }
};

struct MyAuthHandler : PageHandler {
  std::shared_ptr<Response> handle(const Request& request) override {
    // Here one would handle one's authentication system, for example;
    // * check to see if the user has a trusted cookie: if so, accept it.
    // * if not, redirect to a login handler page, and await a redirection
    //   back here with relevant URL parameters indicating success. Then,
    //   set the cookie.
    // For this example, we set the user's authentication information purely
    // from their connection.
    request.credentials()->username = formatAddress(request.getRemoteAddress());
    return Response::unhandled();  // cause next handler to run
  }
};

}  // namespace

bool checkDir() {
  std::string dir = seasocks::getWorkingDir();
  if (!seasocks::endsWith(dir, "seasocks")) {
    ERROR("Samples must be run in the main seasocks directory")
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  INFO("Loading configuration.");
  config["web"]["port"] = 9000;
  config["web"]["dir"] = ".";
  config["web"]["socket"] = "/redis";
  config["web"]["interface"] = "0.0.0.0";
  config["redis"]["host"] = "localhost";
  config["redis"]["port"] = 6379;
  configurator(config, argc, argv);
  std::string str;
  serializeJson(config, str);
  INFO("config:%s", str.c_str());

  int port = config["web"]["port"];
  std::string webDir = config["web"]["dir"];
  std::string webRedisUrl = config["web"]["socket"];
  std::string webInterface = config["web"]["interface"];

  server.addPageHandler(std::make_shared<MyAuthHandler>());
  server.addWebSocketHandler(webRedisUrl.c_str(), std::make_shared<Handler>());
  server.setStaticPath(webDir.c_str());
  server.startListening(port);
  workerThread.addReadInvoker(server.fd(), &server,
                              [](void* srv) { ((Server*)srv)->poll(2); });
  workerThread.run();
  //  server.serve("/home/lieven/workspace/serial2redis/web", 9000);
  return 0;
}
