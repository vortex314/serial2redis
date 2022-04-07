
#include <Udp.h>
#include <Log.h>
#include <StringUtility.h>

Udp::Udp(UdpAddress addr) : _addr(addr) {}

int Udp::init() {
  struct sockaddr_in servaddr;
  if ((_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    WARN("socket creation failed %d : %s", errno, strerror(errno));
    return (errno);
  }

  int optval = 1;
  setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
             sizeof(int));
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;  // IPv4
  servaddr.sin_addr.s_addr = _addr.ip;
  servaddr.sin_port = htons(_addr.port);
  if (bind(_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    WARN("bind failed %d : %s", errno, strerror(errno));
    return (errno);
  }
  INFO("UDP listening %s socket:%d", _addr.toString().c_str(), _sockfd);
  return 0;
}

int Udp::deInit() {
  int rc = close(_sockfd);
  if (rc) perror("close failed");
  return rc;
}

int Udp::receive(UdpMsg &rxd) {
  struct sockaddr_in clientaddr;
  memset(&clientaddr, 0, sizeof(clientaddr));

  socklen_t len = sizeof(clientaddr);  // len is value/resuslt
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_port = htons(_addr.port);
  clientaddr.sin_addr.s_addr = _addr.ip;

  int rc = recvfrom(_sockfd, (char *)buffer, sizeof(buffer), MSG_WAITALL,
                    (struct sockaddr *)&clientaddr, &len);

  if (rc >= 0) {
    rxd.message.clear();
    rxd.src.ip = clientaddr.sin_addr.s_addr;
    rxd.src.port = ntohs(clientaddr.sin_port);
    rxd.dst = _addr;
  /*  INFO(" received from %s to %s  ", rxd.src.toString().c_str(),
         rxd.dst.toString().c_str());*/
    rxd.message = Bytes(buffer, buffer + rc);
    return 0;
  } else {
    return errno;
  }
}

// Client side implementation of UDP client-server model

// Driver code
int Udp::send(const UdpMsg &udpMsg) {
  struct sockaddr_in dest;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(udpMsg.dst.port);
  dest.sin_addr.s_addr = udpMsg.dst.ip;

  /* INFO("TXD UDP => %s : %s ", udpMsg.dst.toString().c_str(),
        hexDump(udpMsg.message).c_str());*/

  int rc = sendto(_sockfd, udpMsg.message.data(), udpMsg.message.size(), 0,
                  (const struct sockaddr *)&dest, sizeof(dest));
  if (rc < 0) return errno;
  return 0;
}

#include <arpa/inet.h>
#include <errno.h>   //For errno - the error number
#include <netdb.h>   //hostent
#include <stdio.h>   //printf
#include <stdlib.h>  //for exit(0);
#include <string.h>  //memset
#include <sys/socket.h>
bool getInetAddr(in_addr_t &addr, std::string &hostname) {
  BZERO(addr);
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_in *h;
  int rv;

  BZERO(hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(hostname.c_str(), 0, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return false;
  }

  // loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if (p->ai_addr->sa_family == AF_INET) {
      sockaddr *sa = p->ai_addr;
      addr = ((sockaddr_in *)sa)->sin_addr.s_addr;
      freeaddrinfo(servinfo);  // all done with this structure
      return true;
    }
  }
  freeaddrinfo(servinfo);  // all done with this structure
  return false;
}

bool getNetPort(uint16_t &x, const std::string &s) {
  x = 0;
  for (char const &ch : s) {
    if (std::isdigit(ch)) {
      x *= 10;
      x += ch - '0';
    } else {
      return false;
    }
  }
//  INFO("getNetPort(%s)=%d",s.c_str(),x);
//  x = htons(x);
  return true;
}

UdpAddress::UdpAddress(std::string _ip,uint16_t _port) {
  port = _port;
  getInetAddr(ip,_ip);
}

std::string UdpAddress::toString() const {
  char charBuffer[100];
  const char *ipString =
      inet_ntop(AF_INET, &ip, charBuffer, sizeof(charBuffer));
  std::string url = ipString == 0 ? "UNKNOWN" : ipString;
  url += ":";
  url += std::to_string(port);
  return url;
}