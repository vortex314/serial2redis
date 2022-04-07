#ifndef _SERIAL_H
#define _SERIAL_H

// For convenience
#include <Log.h>
#include <asm-generic/ioctls.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <fstream>
#include <vector>

typedef std::vector<uint8_t> Bytes;

class Serial {
  std::string _port;       // /dev/ttyUSB0
  std::string _portShort;  // USB0
  int _baudrate;
  int _fd = 0;
  bool _connected = false;

 public:
  Serial();
  ~Serial();
  // commands
  int init();
  int connect();
  int disconnect();
  bool connected() { return _connected; }
  int fd();

  int rxd(Bytes &buffer);
  int txd(const Bytes &);

  // properties
  int baudrate(uint32_t);
  uint32_t baudrate();
  int port(std::string);
  const std::string &port();

  const std::string shortName(void) const;
};

#endif
