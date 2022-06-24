/*
 * UdpFrame.h
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include <Hardware.h>
#include <Log.h>
#include <lwip/netdb.h>
#include <string.h>
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "limero.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "nvs_flash.h"

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

#define FRAME_MAX 128
#define PORT 9999

class UdpFrame : public Actor {
  char addr_str[128];
  int addr_family;
  volatile bool crcDMAdone;
  uint32_t _txdOverflow = 0;
  uint32_t _rxdOverflow = 0;
  std::string _serverIp;
  uint16_t _serverPort;
  char _rxdBuffer[FRAME_MAX];
  int ip_protocol = 0;
  struct sockaddr_in dest_addr;
  int _socket;

 public:
  QueueFlow<Bytes> rxdFrame;
  SinkFunction<Bytes> txdFrame;
  // ValueFlow<Bytes> txd;
  ValueFlow<bool> online;

  UdpFrame(Thread &thread, const char *serverIp, uint16_t port);
  bool init();
  void rxdByte(uint8_t);
  void start();
  void sendFrame(const Bytes &bs);
  void createSocket();
  void closeSocket();
  void waitForData();
  void sendData(const Bytes &, const char *dstAddress);
};

#endif /* SRC_UART_H_ */
