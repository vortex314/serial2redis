#ifndef WIFI_H
#define WIFI_H
#define ESP_IDF

#include <Log.h>
#include <limero.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "freertos/event_groups.h"
//#include "nvs_flash.h"

#ifndef WIFI_SSID
#error "WIFI_SSID should be defined !"
#endif

#ifndef WIFI_PASS
#error "WIFI_PASS should be defined !"
#endif

#define STRINGIFY(X) #X
#define S(X) STRINGIFY(X)

class Wifi : public Actor {
  uint8_t _mac[6];
  wifi_init_config_t _wifiConfig;

 public:
  ValueFlow<bool> connected = false;
  ValueFlow<int> rssi;
  ValueFlow<std::string> ipAddress;
  ValueFlow<std::string> ssid;
  ValueFlow<uint64_t> mac;
  ValueFlow<std::string> macAddress;
  ValueFlow<std::string> password;
  ValueFlow<std::string> prefix;

  Wifi(Thread& thr);
  ~Wifi();
  void init();
  static void wifi_event_handler(void* event_arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data);
  static void ip_event_handler(void* event_arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

  bool scanDoneHandler();
  void connectToAP(const char* AP);
  void startScan();
  void wifiInit();
};

#endif  // WIFI_H
