/*
 * SerialFrame.h
 *
 *  Created on: Nov 21, 2021
 *      Author: lieven
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include "limero.h"
#include <Hardware.h>
#include <Log.h>

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

#define FRAME_MAX 128

class SerialFrame : public Actor {

  UART &_uart;
  Bytes _frameRxd;
  bool escFlag = false;
  size_t _wrPtr, _rdPtr;
  static void onRxd(void *);

public:
  uint8_t rxBuffer[FRAME_MAX];
  volatile bool crcDMAdone;
  uint32_t _txdOverflow = 0;
  uint32_t _rxdOverflow = 0;

  QueueFlow<Bytes> rxdFrame;
  SinkFunction<Bytes> txdFrame;
  ValueFlow<Bytes> txd;

  SerialFrame(Thread &thread, UART &);
  bool init();
  void rxdIrq(UART *uart);
  void rxdByte(uint8_t);
  void sendBytes(uint8_t *, size_t);
  void sendFrame(const Bytes &bs);
};

extern "C" void uartSendBytes(uint8_t *, size_t);

#endif /* SRC_UART_H_ */
