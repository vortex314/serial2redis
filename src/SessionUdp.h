#ifndef _SESSION_UDP_H_
#define _SESSION_UDP_H_
#include <Udp.h>
#include <limero.h>

#include <ArduinoJson.h>

// typedef enum { CMD_OPEN, CMD_CLOSE } TcpCommand;

class UdpSessionError;

class SessionUdp : public Actor {
  UdpSessionError *_errorInvoker;
  int _serialfd;
  Udp _udp;
  int _port;
  std::string _interface;
  uint64_t _frameTimeout = 2000;
  QueueFlow<Bytes> _incomingMessage;
  QueueFlow<Bytes> _outgoingMessage;
  ValueFlow<String> _logs;

  QueueFlow<UdpMsg> _recv;
  QueueFlow<UdpMsg> _send;

  ValueFlow<bool> _connected;
  UdpMsg _udpMsg;

 public:
  //  ValueSource<TcpCommand> command;
  SessionUdp(Thread &thread, JsonObject config);
  bool init();
  bool connect();
  bool disconnect();
  void onError();
  int fd();
  void invoke();
  Source<Bytes> &incoming();
  Sink<Bytes> &outgoing();
  Source<bool> &connected();
  Source<String> &logs();
  Source<UdpMsg> &recv();
  Sink<UdpMsg> &send();
};

class UdpSessionError : public Invoker {
  SessionUdp &_udpSession;

 public:
  UdpSessionError(SessionUdp &udpSession) : _udpSession(udpSession){};
  void invoke() { _udpSession.onError(); }
};
#endif