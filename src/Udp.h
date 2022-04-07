// Server side implementation of UDP client-server model
#ifndef UDP_H
#define UDP_H
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>
typedef std::vector<uint8_t> Bytes;

#define PORT 8080
#define MAXLINE 1472

struct UdpAddress {
  in_addr_t ip;
  uint16_t port;
  bool operator==(UdpAddress &other) {
    return memcmp(&other.ip, &ip, sizeof ip) == 0 && other.port == port;
  }
  UdpAddress() {};
  UdpAddress(std::string ip, uint16_t port);
  /* bool operator()(const UdpAddress &lhs, const UdpAddress &rhs) const {
     return false;
   }*/
  bool operator<(const UdpAddress &other) const {
    int rc = memcmp(&other.ip, &ip, sizeof ip);
    if (rc) return rc == -1;
    return other.port < port;
  }
  UdpAddress& operator=(const UdpAddress& rhs){
    port = rhs.port;
    memcpy(&ip,&rhs.ip,sizeof( in_addr_t));
    return *this;
  }
  std::string toString() const;
};

struct UdpMsg {
 public:
  UdpAddress src;
  UdpAddress dst;
  Bytes message;
  void dstIpString(const char *ip) { dst.ip = inet_addr(ip); }
  void dstPort(uint16_t port) { dst.port = htons(port); }
};

class Udp {
  UdpAddress _addr;

  int _sockfd;
  char buffer[MAXLINE];

 public:
  Udp(UdpAddress addr);
  void dst(const char *);
  void address(UdpAddress addr) { _addr = addr; };
  UdpAddress address() { return _addr; };
  int init();
  int receive(UdpMsg &);
  int send(const UdpMsg &);
  int fd() { return _sockfd; };
  int deInit();
};
#endif
