#include "UdpFrame.h"

UdpFrame::UdpFrame(Thread &thread, const char *serverIp, uint16_t port)
    : Actor(thread),
      _serverIp(serverIp),
      _serverPort(port),
      rxdFrame(5),
      txdFrame([&](const Bytes &bs) { sendData(bs, _serverIp.c_str()); }) {
  rxdFrame.async(thread);
}

bool UdpFrame::init() {
  online >> [&](const bool &on) {
    if (on)
      createSocket();
    else
      closeSocket();
  };

  return true;
}
Thread receiverThread("udpReceiver");

void UdpFrame::start() {
  INFO("start");
  TimerSource *receiverTimer =
      new TimerSource(receiverThread, 1000, true, "receiverTimer");
  *receiverTimer >> [&](const TimerMsg &) {
    INFO("timer triggered ");
    if (online()) waitForData();
  };
  INFO("kickoff thread");
  receiverThread.start();
}

void UdpFrame::sendData(const Bytes &bs, const char *dst) {
  if (!online()) return;
  struct sockaddr_in dstAddress;
  inet_pton(AF_INET, dst, &dstAddress.sin_addr);
  dstAddress.sin_port = htons(PORT);
  dstAddress.sin_family = AF_INET;

  int err = sendto(_socket, bs.data(), bs.size(), 0,
                   (struct sockaddr *)&dstAddress, sizeof(dstAddress));
  if (err < 0 || err != bs.size()) {
    ERROR("Error occurred during sending: errno %d :  %s", errno,
          strerror(errno));
    return;
  }
}

void UdpFrame::createSocket() {
  BZERO(dest_addr);
  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(_serverPort);
  ip_protocol = IPPROTO_IP;
  addr_family = AF_INET;
  _socket = ::socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (_socket < 0) {
    ERROR("Unable to create socket: errno %d", errno);
    return;
  }
  INFO("Socket created");
  int err = bind(_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    ERROR("Socket unable to bind: errno %d : %s", errno, strerror(errno));
    return;
  }
  INFO("Socket bound, port %d", _serverPort);
}

void UdpFrame::waitForData() {
  INFO("Waiting for data");
  while (online()) {
    struct sockaddr_storage source_addr;  // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(source_addr);
    int len = recvfrom(_socket, _rxdBuffer, sizeof(_rxdBuffer) - 1, 0,
                       (struct sockaddr *)&source_addr, &socklen);
    if (len < 0) {
      ERROR("recvfrom failed: errno %d", errno);
      return;
    } else {
      rxdFrame.on(Bytes(_rxdBuffer, _rxdBuffer + len));
    }
  }
}

void UdpFrame::closeSocket() {
  if (_socket != -1) {
    ERROR("Shutting down socket and restarting...");
    shutdown(_socket, 0);
    close(_socket);
  }
}
