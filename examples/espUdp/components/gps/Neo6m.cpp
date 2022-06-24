#include "Neo6m.h"

Neo6m::Neo6m(Thread& thr, Uext* connector)
    : Actor(thr), _connector(connector), _uart(connector->getUART()) {}

Neo6m::~Neo6m() {}

std::string stringify(std::string in) {
  std::string out = "\"";
  out += in;
  out += "\"";
  return out;
}

std::vector<std::string> split(std::string s, std::string delimiter) {
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  std::string token;
  std::vector<std::string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }
  res.push_back(s.substr(pos_start));
  return res;
}

void Neo6m::init() {
  _uart.setClock(9600);
  _uart.onRxd(onRxd, this);
  _uart.init();
}

bool toLocation(Location& location, std::string& lat, std::string& lon) {
  if (lat.length() > 2 && lon.length() > 2) {
    location.latitude =
        std::stod(lat.substr(0, 2)) + (std::stod(lat.substr(2)) / 60.0);
    location.longitude =
        std::stod(lon.substr(0, 3)) + (std::stod(lon.substr(3)) / 60.0);
    return true;
  }
  return false;
}

void Neo6m::request() { WARN(" data will be send async"); }

void Neo6m::onRxd(void* me) { ((Neo6m*)me)->handleRxd(); }

void Neo6m::handleRxd() {
  while (_uart.hasData()) {
    char ch = _uart.read();
    if (ch == '\n' || ch == '\r') {
      if (_line.size() > 8) {
        std::string key = _line.substr(1, 5);
        if (key == "GPGLL") {
          std::vector<std::string> fields = split(_line.substr(7), ",");
          Location loc;
          if (toLocation(loc, fields[0], fields[2])) {
            INFO("%f,%f", loc.latitude, loc.longitude);
            location = {loc};
          };
        } else if (key == "GPGSV") {
          std::vector<std::string> fields = split(_line.substr(7), ",");
          if (fields.size() > 4) {
            satellitesInView = std::stoi(fields[2]);
          }
        } else if (key == "GPGSA") {
          std::vector<std::string> fields = split(_line.substr(7), ",");
          int count = 0;
          for (int i = 0; i < 12; i++) {
            if (fields[i + 2].length() > 0) count++;
          }
          satellitesInUse = count;
        }
      }
      _line.clear();
    } else {
      _line += ch;
    }
  }
}
