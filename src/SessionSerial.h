#ifndef _SESSION_SERAIL_H_
#define _SESSION_SERIAL_H_
#include <ArduinoJson.h>
#include <Frame.h>
#include <limero.h>
#include <serial.h>

// typedef enum { CMD_OPEN, CMD_CLOSE } TcpCommand;

class SerialSessionError;

class SessionSerial : public Actor, public Invoker {
  SerialSessionError *_errorInvoker;
  int _serialfd;
  Serial _serialPort;
  string _port;
  uint32_t _baudrate;
  Bytes _rxdBuffer;
  Bytes _inputFrame;
  Bytes _cleanData;
  uint64_t _lastFrameFlag;
  uint64_t _frameTimeout = 2000;
  ValueFlow<Bytes> _incomingFrame;
  ValueFlow<Bytes> _outgoingFrame;
  ValueFlow<bool> _connected;

 public:
  SessionSerial(Thread &thread, JsonObject config);
  bool init();
  bool connect();
  bool disconnect();
  void onError();
  int fd();
  void invoke();
  Source<Bytes> &incoming();
  Sink<Bytes> &outgoing();
  Source<bool> &connected();
  string port();
};

class SerialSessionError : public Invoker {
  SessionSerial &_serialSession;

 public:
  SerialSessionError(SessionSerial &serialSession)
      : _serialSession(serialSession){};
  void invoke() { _serialSession.onError(); }
};
#endif