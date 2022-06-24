#ifndef NEO6M_H
#define NEO6M_H
#include <Hardware.h>
#include <Log.h>
#include <limero.h>

struct Location {
  double latitude;
  double longitude;
};

class Neo6m : public Actor {
  Uext* _connector;
  UART& _uart;
  static void onRxd(void*);
  std::string _line;

 public:
  ValueFlow<Location> location;
  ValueFlow<int> satellitesInView, satellitesInUse;
  Neo6m(Thread& thr, Uext* connector);
  virtual ~Neo6m();
  void init();
  void handleRxd();
  void request();
};

#endif  // NEO6M_H
