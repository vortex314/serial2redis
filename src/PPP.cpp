#include <PPP.h>
#include <unistd.h>
//================================================================

// add a byte to the buffer, escaped according to PPP standard
inline void addEscaped(Bytes &out, uint8_t c) {
  if (c == PPP_ESC_CHAR || c == PPP_FLAG_CHAR) {  // byte stuffing
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

/**
 * @brief gets the payload out of a frame , check fcs and remove byte stuffing,
 *PPP_FLAG already removed
 * @param[in] InputStream of Bytes without PPP_FLAG_CHAR
 * @param[out] OutputStream of Bytes containing complete package , checked on
 *FCS, unterminated ESC char and removed byte stuffing
 * @return Error code : 0 : insufficient data yet, 1 : frame extracted , 2 :
 *invalid frame
 **/

bool deframe(Bytes &out, const Bytes &in) {
  out.clear();
  bool escFlag = false;
  if (in.size() < 3) {
    WARN(" buffer too small [%d]", in.size());
    return false;
  }
  // The receiver must reverse the octet stuffing procedure
  uint32_t count = 0;
  for (count = 0; count < in.size(); count++) {  // keep 2 Bytes for CRC
    // Read a single character
    uint8_t c = in[count];
    if (c == PPP_ESC_CHAR) {
      // All occurrences of 0x7D indicate that the next character is escaped
      escFlag = true;
    } else if (c == PPP_FLAG_CHAR) {
      WARN(" didn't expect FLAG char ");
      return false;
    } else if (escFlag) {
      // The character is XOR'ed with 0x20

      out.push_back(c ^ PPP_MASK_CHAR);
      escFlag = false;
    } else {
      // Copy current character
      out.push_back(c);
    }
  }
  return !escFlag;
}

PPP::PPP()  {}

void PPP::on(const Bytes &bs) { handleRxd(bs); }

void PPP::toStdout(const Bytes &bs) {
  if (bs.size() > 1) {
#ifdef ARDUINO
    Serial.write(bs.data(), bs.size());
#else
    logs.on(bs);
#endif
  }
}

bool PPP::handleFrame(const Bytes &bs) {
  if (bs.size() == 0) return false;
  if (deframe(_cleanData, bs)) {
    emit(_cleanData);
    return true;
  } else {
    toStdout(bs);
    return false;
  }
}

void PPP::handleRxd(const Bytes &bs) {
  for (uint8_t b : bs) {
    if (b == PPP_FLAG_CHAR) {
      _lastFrameFlag = Sys::millis();
      handleFrame(_inputFrame);
      _inputFrame.clear();
    } else {
      _inputFrame.push_back(b);
    }
  }
  if ((Sys::millis() - _lastFrameFlag) > _frameTimeout) {
    toStdout(bs);
    _inputFrame.clear();
  }
}
void PPP::request(){};

Flow<Bytes, Bytes> &PPP::frame() {
    return * new LambdaFlow<Bytes, Bytes>([&](Bytes &out, const Bytes &in) {
    out = frame(in);
    return true;
      };
}

Flow<Bytes,Bytes>& PPP::deframe(){

}