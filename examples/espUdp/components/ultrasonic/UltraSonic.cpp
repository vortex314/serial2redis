#include "UltraSonic.h"

UltraSonic::UltraSonic(Thread& thr, Uext& connector)
    : Actor(thr), _connector(connector), _pollTimer(thr, 1000, true) {
  _hcsr = new HCSR04(_connector);
  distance = 0;
  distance.async(thr);
  delay = 0;
  _pollTimer >> [&](const TimerMsg& tm) { on(tm); };
}

UltraSonic::~UltraSonic() { delete _hcsr; }

void UltraSonic::init() {
  INFO("init");
  _hcsr->init();
}

void UltraSonic::on(const TimerMsg& tm) {
  int mm = _hcsr->getMillimeters();
  // INFO("%d", mm);
  // distance = mm;
  if (mm < 4000 && mm > 0) {
    distance = distance() + (mm - distance()) / 2;
    delay = delay() + (_hcsr->getTime() - delay()) / 2;
  }
  _hcsr->trigger();
}
