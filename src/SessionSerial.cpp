#include <SessionSerial.h>
#include <StringUtility.h>

SessionSerial::SessionSerial(Thread &thread, JsonObject config)
    : Actor(thread),
      _incomingFrame(thread, 10),
      _outgoingFrame(thread),
      _connected(thread) {
  _port = config["port"] | "/dev/ttyUSB0";
  _baudrate = config["baudrate"] | 115200;
}

bool SessionSerial::init() {
  _serialPort.port(_port);
  _serialPort.baudrate(_baudrate);
  _serialPort.init();
  _outgoingFrame.handler([&](const Bytes &data) {
    INFO("TXD  %s => [%d] %s", _serialPort.port().c_str(), data.size(),
         hexDump(data).c_str());
    _serialPort.txd(data);
  });
  return true;
}

bool SessionSerial::connect() {
  if (_serialPort.connect() != 0) return false;
  thread().addReadInvoker(_serialPort.fd(), this,
                          [](void *pv) { ((SessionSerial *)pv)->onRead(); });
  thread().addErrorInvoker(_serialPort.fd(), this,
                           [](void *pv) { ((SessionSerial *)pv)->onError(); });
  return true;
}

bool SessionSerial::disconnect() {
  thread().delAllInvoker(_serialPort.fd());
  _serialPort.disconnect();
  return true;
}
// on data incoming on filedescriptor
void SessionSerial::onRead() {
  int rc = _serialPort.rxd(_rxdBuffer);
  if (rc == 0) {                   // read ok
    if (_rxdBuffer.size() == 0) {  // but no data
      WARN(" 0 data => connection lost [%d] %s",errno,strerror(errno));
      reconnect();
    } else {
      DEBUG("RXD  %s => [%d] %s : %s ", _serialPort.port().c_str(),
           _rxdBuffer.size(), hexDump(_rxdBuffer).c_str(),
           charDump(_rxdBuffer).c_str());
      _incomingFrame.on(_rxdBuffer);
    }
  }
}
// on error issue onf ile descriptor
void SessionSerial::onError() {
  INFO(" error occured on serial ,disconnecting ");
  reconnect();
}

void SessionSerial::reconnect() {
  disconnect();
  while (true) {
    sleep(1);
    if (connect()) break;
    INFO(" reconnecting...");
  };
}

int SessionSerial::fd() { return _serialPort.fd(); }

Source<Bytes> &SessionSerial::incoming() { return _incomingFrame; }

Sink<Bytes> &SessionSerial::outgoing() { return _outgoingFrame; }

Source<bool> &SessionSerial::connected() { return _connected; }

std::string SessionSerial::port() { return _port; }