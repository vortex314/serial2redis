#include <SessionSerial.h>


SessionSerial::SessionSerial(Thread &thread, JsonObject config)
    : Actor(thread) {
  _errorInvoker = new SerialSessionError(*this);
  _port = config["port"] | "/dev/ttyUSB0";
  _baudrate = config["baudrate"] | 115200;
}

bool SessionSerial::init() {
  _serialPort.port(_port);
  _serialPort.baudrate(_baudrate);
  _serialPort.init();
  _outgoingFrame >>  [&](const Bytes &data) {
 //   INFO("TXD  %s => [%d] %s", _serialPort.port().c_str(),data.size(), hexDump(data).c_str());
    _serialPort.txd(data);
  };
  _outgoingFrame >> [&](const Bytes &bs) {
 //   INFO("TXD [%d] %s ",bs.size(), hexDump(bs).c_str());
    INFO("TXD %s ", std::string(bs.begin(),bs.end()).c_str());
  };

  return true;
}

bool SessionSerial::connect() {
  _serialPort.connect();
  thread().addReadInvoker(_serialPort.fd(), [&](int) { invoke(); });
  thread().addErrorInvoker(_serialPort.fd(), [&](int) { onError(); });
  return true;
}

bool SessionSerial::disconnect() {
  thread().deleteInvoker(_serialPort.fd());
  _serialPort.disconnect();
  return true;
}
// on data incoming on filedescriptor
void SessionSerial::invoke() {
  int rc = _serialPort.rxd(_rxdBuffer);
  if (rc == 0) {                  // read ok
    if (_rxdBuffer.size() == 0) { // but no data
      WARN(" 0 data ");
    } else {
      _incomingFrame.on(_rxdBuffer);
    }
  }
}
// on error issue onf ile descriptor
void SessionSerial::onError() {
  INFO(" error occured on serial ,disconnecting ");
  disconnect();
}

int SessionSerial::fd() { return _serialPort.fd(); }

Source<Bytes> &SessionSerial::incoming() { return _incomingFrame; }

Sink<Bytes> &SessionSerial::outgoing() { return _outgoingFrame; }

Source<bool> &SessionSerial::connected() { return _connected; }

string SessionSerial::port() { return _port; }