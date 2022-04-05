#ifndef __PPP_H__
#define __PPP_H__
#include <limero.h>
#include <stdint.h>
#include <unistd.h>

#include <vector>

typedef std::vector<uint8_t> Bytes;

#define PPP_FCS_SIZE 2

#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

class PPP {
  uint64_t _lastFrameFlag;
  size_t _maxFrameLength;
  Bytes _buffer;

  LambdaFlow<Bytes, Bytes> _frame;
  Flow<Bytes, Bytes> *_deframe;

 public:
  PPP(size_t);
  ~PPP();

  void addEscaped(Bytes &out, uint8_t c);
  bool handleFrame(const Bytes &bs);
  void handleRxd(const Bytes &bs);
  void request();

  Flow<Bytes, Bytes> &frame();
  Flow<Bytes, Bytes> &deframe();
};

#endif
