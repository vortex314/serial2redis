#include <Framing.h>

#include <algorithm>

class Deframer : public Flow<Bytes, Bytes> {
  Bytes _buffer;
  size_t _maxFrameLength;
  std::string _delimiter;

 public:
  Deframer(const char* delimiter, size_t maxFrameLength) {
    _maxFrameLength = maxFrameLength;
    _delimiter = delimiter;
  }
  void on(const Bytes& in) {
    for (auto c : in) {
      if (std::find(_delimiter.begin(), _delimiter.end(), (char)c) != _delimiter.end()) {
        if (_buffer.size() > 0) {
          emit(_buffer);
          _buffer.clear();
        }
      } else {
        if (_buffer.size() > _maxFrameLength) _buffer.clear();
        _buffer.push_back(c);
      }
    }
  }
};

Framing::Framing(const char* delimiter, size_t maxFrameLength) {
  _maxFrameLength = maxFrameLength;
  _delimiter = delimiter;

  _frame = LambdaFlow<Bytes, Bytes>([&](Bytes& out, const Bytes& in) {
    //    for (auto c : _delimiter) out.push_back(c);
    out.insert(out.end(), in.begin(), in.end());
    for (auto c : _delimiter) out.push_back(c);
    return true;
  });

  _deframe = new Deframer(delimiter, maxFrameLength);
}
Flow<Bytes, Bytes>& Framing::deframe() { return *_deframe; }
Flow<Bytes, Bytes>& Framing::frame() { return _frame; }
