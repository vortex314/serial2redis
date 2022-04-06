#include <SessionUdp.h>
#include <StringUtility.h>

SessionUdp::SessionUdp(Thread &thread, UdpAddress address)
    : Actor(thread), _udp(address), _connected(false) {
  _errorInvoker = new UdpSessionError(*this);
  _udp.address(address);
  _send >> [&](const UdpMsg &um) { _udp.send(um); };
}

UdpAddress SessionUdp::address() { return _udp.address(); }

bool SessionUdp::connect() {
  if( _udp.init() ) return false;
  thread().addReadInvoker(_udp.fd(), this,
                          [](void *pv) { ((SessionUdp *)pv)->invoke(); });
  thread().addErrorInvoker(_udp.fd(), this,
                           [](void *pv) { ((SessionUdp *)pv)->onError(); });
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

Source<UdpMsg> &SessionUdp::recv() { return _recv; }

Sink<UdpMsg> &SessionUdp::send() { return _send; }

Source<bool> &SessionUdp::connected() { return _connected; }
Source<String> &SessionUdp::logs() { return _logs; }