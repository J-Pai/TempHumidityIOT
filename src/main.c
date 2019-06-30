#include "mgos.h"
#include "mgos_dht.h"

#define C_TO_F(c) (((c) * 1.8f) + 32.0f)

static void dht_measurement(void * dht) {
  float temp = mgos_dht_get_temp(dht);

  if (true == mgos_sys_config_get_app_dht_fahrenheit()) {
    temp = C_TO_F(temp);
  }

  LOG(LL_INFO, ("Pin: %d | Temperature: %1f | Humidity: %1f",
    mgos_sys_config_get_app_dht_pin(),
    temp,
    mgos_dht_get_humidity(dht)));
}

static void server_status(void * arg) {
  enum mgos_wifi_status status = mgos_wifi_get_status();

  if (status == MGOS_WIFI_IP_ACQUIRED) {
    LOG(LL_INFO, ("Network Connected [%d]", status));
  } else if (status == MGOS_WIFI_CONNECTED || status == MGOS_WIFI_CONNECTING) {
    LOG(LL_INFO, ("Network Pending... [%d]", status));
  } else {
    LOG(LL_INFO, ("Network Disconnected [%d]", status));
  }

  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  struct mgos_dht * dht = mgos_dht_create(mgos_sys_config_get_app_dht_pin(), DHT11);
  // Setup Periodic Measurement of Temperature and Humidity
  mgos_set_timer(1000, true, dht_measurement, dht);
  // Setup Periodic Reporting of HTTP Server Info
  mgos_set_timer(5000, true, server_status, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
