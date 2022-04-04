#include <SessionUdp.h>
#include <StringUtility.h>

SessionUdp::SessionUdp(Thread &thread, JsonObject config)
    : Actor(thread),
      _incomingMessage(10, "_incomingMessage"),
      _outgoingMessage(10, "_outgoingMessage"),
      _send(10, "send"),
      _recv(10, "recv") {
  _recv.async(thread);
  _send.async(thread);
  _errorInvoker = new UdpSessionError(*this);
  _port = config["port"].as<uint32_t>();
  _send >> [&](const UdpMsg &um) { _udp.send(um); };
}

bool SessionUdp::init() {
  _udp.port(_port);
  _udp.init();

  return true;
}

bool SessionUdp::connect() {
  thread().addReadInvoker(_udp.fd(), [&](int) { invoke(); });
  thread().addErrorInvoker(_udp.fd(), [&](int) { onError(); });
  return true;
}

bool SessionUdp::disconnect() {
  thread().delAllInvoker(_udp.fd());
  _udp.deInit();
  return true;
}
// on data incoming on file descriptor
void SessionUdp::invoke() {
  int rc = _udp.receive(_udpMsg);
  if (rc == 0) {  // read ok
    DEBUG("UDP RXD %s => %s ", _udpMsg.src.toString().c_str(),
         hexDump(_udpMsg.message).c_str());
    _recv.on(_udpMsg);
  }
}
// on error issue on file descriptor
void SessionUdp::onError() {
  WARN(" Error occured on SessionUdp. Disconnecting.. ");
  disconnect();
}

int SessionUdp::fd() { return _udp.fd(); }

Source<Bytes> &SessionUdp::incoming() { return _incomingMessage; }
Source<UdpMsg> &SessionUdp::recv() { return _recv; }

Sink<Bytes> &SessionUdp::outgoing() { return _outgoingMessage; }
Sink<UdpMsg> &SessionUdp::send() { return _send; }

Source<bool> &SessionUdp::connected() { return _connected; }
Source<String> &SessionUdp::logs() { return _logs; }