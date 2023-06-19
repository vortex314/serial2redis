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
  std::string _interface;
  uint64_t _frameTimeout = 2000;
  ValueFlow<Bytes> _incomingMessage;
  ValueFlow<Bytes> _outgoingMessage;
  ValueFlow<String> _logs;

  ValueFlow<UdpMsg> _recv;
  QueueFlow<UdpMsg> _send;

  ValueFlow<bool> _connected;
  UdpMsg _udpMsg;

 public:
  //  ValueSource<TcpCommand> command;
  SessionUdp(Thread &thread, UdpAddress address);
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
  UdpAddress address();
};

class UdpSessionError : public Invoker {
  SessionUdp &_udpSession;

 public:
  UdpSessionError(SessionUdp &udpSession) : _udpSession(udpSession){};
  void invoke() { _udpSession.onError(); }
};
#endif