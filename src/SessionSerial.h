#ifndef _SESSION_SERAIL_H_
#define _SESSION_SERIAL_H_
#include <ArduinoJson.h>
#include <Frame.h>
#include <limero.h>
#include <serial.h>

// typedef enum { CMD_OPEN, CMD_CLOSE } TcpCommand;

class SessionSerial : public Actor {
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
  void reconnect();
  void onError();
  void onRead();
  int fd();

  Source<Bytes> &incoming();
  Sink<Bytes> &outgoing();
  Source<bool> &connected();
  string port();
};

#endif