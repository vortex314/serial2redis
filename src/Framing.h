#ifndef __FRAMING_H__
#define __FRAMING_H__
#include <limero.h>
class Framing {
  size_t _maxFrameLength;
  std::string _delimiter;
  LambdaFlow<Bytes, Bytes> _frame;
  Flow<Bytes, Bytes>* _deframe;

 public:
  Framing(const char* delimiter, size_t maxFrameLength);
  ~Framing();
  Flow<Bytes, Bytes>& deframe();
  Flow<Bytes, Bytes>& frame();
};
#endif