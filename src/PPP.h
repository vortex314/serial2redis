#ifndef A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB
#define A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB
#include <limero.h>

typedef std::vector<uint8_t> Bytes;

#define PPP_FCS_SIZE 2
// PPP special characters
#define PPP_MASK_CHAR 0x20
#define PPP_ESC_CHAR 0x7D
#define PPP_FLAG_CHAR 0x7E

Bytes frame(const Bytes &in);
bool deframe(Bytes &out, const Bytes &in);
//================================================================
class PPP {
  Bytes _inputFrame;
  Bytes _cleanData;
  uint64_t _lastFrameFlag;
  uint32_t _frameTimeout = 1000;

 public:
  ValueFlow<Bytes> logs;
  PPP();
  void on(const Bytes &bs);
  void toStdout(const Bytes &bs);
  bool handleFrame(const Bytes &bs);
  void handleRxd(const Bytes &bs);
  void request();
  Flow<Bytes, Bytes> &frame();
  Flow<Bytes, Bytes> &deframe();
};
class FrameToBytes : public LambdaFlow<Bytes, Bytes> {
 public:
  FrameToBytes();
};

#endif /* A4D87CFA_B381_4A7A_9FB2_67EDFB2285CB */
