#include "SerialFrame.h"

SerialFrame *_serialFrame = 0;
SerialFrame::SerialFrame(Thread &thread, UART &uart)
    : Actor(thread), _uart(uart), rxdFrame(5), txdFrame([&](const Bytes &bs) {
        sendFrame(bs);
      }) {
  rxdFrame.async(thread);
  _rdPtr = 0;
  _wrPtr = 0;
  crcDMAdone = true;
  _serialFrame = this;
}

bool SerialFrame::init() {
  _uart.setClock(SERIAL_BAUD);
  _uart.onRxd(onRxd, this);
  _uart.mode("8N1");
  _uart.init();
  return true;
}

inline void addEscaped(Bytes &out, uint8_t c) {
  if (c == PPP_ESC_CHAR || c == PPP_FLAG_CHAR) { // byte stuffing
    out.push_back(PPP_ESC_CHAR);
    out.push_back(c ^ PPP_MASK_CHAR);
  } else {
    out.push_back(c);
  }
}

Bytes frame(const Bytes &in) {
  Bytes out;
  out.clear();
  out.push_back(PPP_FLAG_CHAR);
  for (uint8_t c : in) {
    addEscaped(out, c);
  }
  out.push_back(PPP_FLAG_CHAR);
  return out;
}

void SerialFrame::rxdByte(uint8_t c) {
  if (c == PPP_ESC_CHAR) {
    escFlag = true;
  } else if (c == PPP_FLAG_CHAR) {
    if (_frameRxd.size())
      rxdFrame.on(_frameRxd);
    _frameRxd.clear();
  } else if (escFlag) {
    _frameRxd.push_back(c ^ PPP_MASK_CHAR);
    escFlag = false;
  } else {
    if (_frameRxd.size() < FRAME_MAX) {
      _frameRxd.push_back(c);
    } else {
      _rxdOverflow++;
      _frameRxd.clear();
    }
  }
}

void SerialFrame::onRxd(void *pv) {
  SerialFrame *me = (SerialFrame *)pv;
  while (me->_uart.hasData()) {
    me->rxdByte(me->_uart.read());
  }
}

void SerialFrame::sendFrame(const Bytes &bs) {
  Bytes txBuffer = frame(bs);
  sendBytes(txBuffer.data(), txBuffer.size());
}

void SerialFrame::sendBytes(uint8_t *data, size_t length) {
  _uart.write(data, length);
}

extern "C" void uartSendBytes(uint8_t *data, size_t length) {
  if (_serialFrame)
    _serialFrame->sendBytes(data, length);
}