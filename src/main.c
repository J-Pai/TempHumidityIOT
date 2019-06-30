#include "mgos.h"
#include "mgos_dht.h"

#define C_TO_F(c) (((c) * 1.8f) + 32.0f)

#define MAIN_LED 16
#define SECONDARY_LED mgos_sys_config_get_board_led1_pin()
#define TERTIARY_LED mgos_sys_config_get_board_led2_pin()

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
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
      mgos_uptime(), (unsigned long) mgos_get_heap_size(),
      (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
  if (status == MGOS_WIFI_IP_ACQUIRED) {
    char * ssid = mgos_wifi_get_connected_ssid();
    LOG(LL_INFO, ("Network Connected [%s][%d]", ssid, status));
    free(ssid);
  } else if (status == MGOS_WIFI_CONNECTED || status == MGOS_WIFI_CONNECTING) {
    LOG(LL_INFO, ("Network Pending... [%d]", status));
  } else {
    LOG(LL_INFO, ("Network Disconnected [%d]", status));
  }
  (void) arg;
}

static void led_status(void * arg) {
  if (false == mgos_sys_config_get_app_silent()) {
    mgos_gpio_toggle(MAIN_LED);
    if (MGOS_WIFI_IP_ACQUIRED == mgos_wifi_get_status()) {
      mgos_gpio_toggle(SECONDARY_LED);
    } else {
      mgos_gpio_setup_output(SECONDARY_LED, 1);
    }
  } else {
    mgos_gpio_setup_output(MAIN_LED, 1);
    mgos_gpio_setup_output(SECONDARY_LED, 1);
    mgos_gpio_setup_output(TERTIARY_LED, 1);
  }
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  struct mgos_dht * dht = mgos_dht_create(mgos_sys_config_get_app_dht_pin(), DHT11);
  mgos_gpio_setup_output(MAIN_LED, 1);
  mgos_gpio_setup_output(SECONDARY_LED, 1);
  mgos_gpio_setup_output(TERTIARY_LED, 1);
  // Setup Periodic Measurement of Temperature and Humidity
  mgos_set_timer(1000, true, dht_measurement, dht);
  // Setup Periodic Led Flashing
  mgos_set_timer(1000, true, led_status, NULL);
  // Setup Periodic Reporting of HTTP Server Info
  mgos_set_timer(5000, true, server_status, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
