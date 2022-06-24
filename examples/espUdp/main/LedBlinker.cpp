#include "LedBlinker.h"

LedBlinker::LedBlinker(Thread& thr, uint32_t pin, uint32_t delay)
    : Actor(thr), blinkTimer(thr, delay, true) {
  _pin = pin;

  blinkTimer >> ([&](const TimerMsg tm) {
    gpio_set_level((gpio_num_t)_pin, _on);
    _on = _on ? 0 : 1;
  });

  blinkSlow >> [&](bool flag) {
  //  INFO(" blink %s", flag ? "slow" : "fast");
    if (flag)
      blinkTimer.interval(BLINK_SLOW_INTERVAL);
    else
      blinkTimer.interval(BLINK_FAST_INTERVAL);
  };
}
void LedBlinker::init() {
  gpio_config_t io_conf;
  io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = 1 << _pin;
  io_conf.pull_down_en = (gpio_pulldown_t)1;
  io_conf.pull_up_en = (gpio_pullup_t)1;
  gpio_config(&io_conf);
}

void LedBlinker::delay(uint32_t d) { blinkTimer.interval(d); }
