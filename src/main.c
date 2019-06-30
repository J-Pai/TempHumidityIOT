#include "mgos.h"
#include "mgos_system.h"
#include "mgos_dht.h"
#include "mgos_http_server.h"

#define C_TO_F(c) (((c) * 1.8f) + 32.0f)

#define MAIN_LED 16
#define SECONDARY_LED mgos_sys_config_get_board_led1_pin()
#define TERTIARY_LED mgos_sys_config_get_board_led2_pin()

typedef struct {
  struct mgos_dht * sensor;
  struct mgos_rlock_type * data_lock;
  float temperature;
  float humidity;
  struct mbuf temperature_history;
  struct mbuf humidity_history;
  struct mgos_rlock_type * hist_data_lock;
} Sensor_DHT;

static void dht_measurement(void * arg) {
  Sensor_DHT * dht = (Sensor_DHT *)arg;
  static bool dht_tick_tock = false;
  mgos_rlock(dht->data_lock);
  float temp = dht->temperature;
  float humi = dht->humidity;
  mgos_runlock(dht->data_lock);
  if (dht_tick_tock) {
    temp = mgos_dht_get_temp(dht->sensor);
    if (true == mgos_sys_config_get_app_dht_fahrenheit()) {
      temp = C_TO_F(temp);
    }
    mgos_rlock(dht->data_lock);
    dht->temperature = temp;
    mgos_runlock(dht->data_lock);
  } else {
    humi = mgos_dht_get_humidity(dht->sensor) + mgos_sys_config_get_app_dht_humidity_offset();
    mgos_rlock(dht->data_lock);
    dht->humidity = humi;
    mgos_runlock(dht->data_lock);
  }
  if (false == mgos_sys_config_get_app_silent()) {
    LOG(LL_INFO, ("Pin: %d | Temperature: %.1f | Humidity: %.1f (Offset: %d)",
      mgos_sys_config_get_app_dht_pin(),
      temp,
      humi,
      mgos_sys_config_get_app_dht_humidity_offset()));
  }
  dht_tick_tock = !dht_tick_tock;
}

static void server_status(void * arg) {
  enum mgos_wifi_status status = mgos_wifi_get_status();
  static bool s_tick_tock = false;
  if (status == MGOS_WIFI_IP_ACQUIRED) {
    char * ssid = mgos_wifi_get_connected_ssid();
    struct mgos_net_ip_info ip_info;
    bool obtained = mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, 0, &ip_info);
    char ip[16];
    mgos_net_ip_to_str(&(ip_info.ip), ip);
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free | Network Connected (Status: %d)[%s][%s]",
      (s_tick_tock ? "Tick" : "Tock"),
      mgos_uptime(), (unsigned long) mgos_get_heap_size(),
      (unsigned long) mgos_get_free_heap_size(),
      status, ssid, true == obtained ? ip : ""));
    free(ssid);
  } else if (status == MGOS_WIFI_CONNECTED || status == MGOS_WIFI_CONNECTING) {
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free | Network Pending... (Status: %d)",
      (s_tick_tock ? "Tick" : "Tock"),
      mgos_uptime(), (unsigned long) mgos_get_heap_size(),
      (unsigned long) mgos_get_free_heap_size(),
      status));
  } else {
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free | Network Disconnected (Status: %d)",
      (s_tick_tock ? "Tick" : "Tock"),
      mgos_uptime(), (unsigned long) mgos_get_heap_size(),
      (unsigned long) mgos_get_free_heap_size(),
      status));
  }
  s_tick_tock = !s_tick_tock;
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

static void get_dht_data_handler(struct mg_connection * c, int ev, void * p, void * user_data) {
  Sensor_DHT * dht = (Sensor_DHT *)user_data;
  (void) p;
  if (ev != MG_EV_HTTP_REQUEST) return;
  mgos_rlock(dht->data_lock);
  float temp = dht->temperature;
  float humi = dht->humidity;
  mgos_runlock(dht->data_lock);
  LOG(LL_INFO, ("DHT Data Requested | Temperature: %.1f | Humidity: %.1f (Offset: %d)",
    temp, humi,
    mgos_sys_config_get_app_dht_humidity_offset()));
  mg_send_response_line(c, 200,
                        "Content-Type: text/json\r\n");
  mg_printf(c, "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, humi);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

enum mgos_app_init_result mgos_app_init(void) {
  // Initializing DHT Structure
  Sensor_DHT * dht = (Sensor_DHT *)malloc(sizeof(Sensor_DHT));
  dht->data_lock = mgos_rlock_create();
  dht->sensor = mgos_dht_create(mgos_sys_config_get_app_dht_pin(), DHT11);
  dht->hist_data_lock = mgos_rlock_create();
  mbuf_init(&dht->temperature_history, 8);
  mbuf_init(&dht->humidity_history, 8);

  // Initializing Status LEDs
  mgos_gpio_setup_output(MAIN_LED, 1);
  mgos_gpio_setup_output(SECONDARY_LED, 1);
  mgos_gpio_setup_output(TERTIARY_LED, 1);

  // Setup HTTP call to obtain current Temperature and Humidity
  mgos_register_http_endpoint("/dht", get_dht_data_handler, dht);
  // Setup Periodic Measurement of Temperature and Humidity
  mgos_set_timer(2500, true, dht_measurement, dht);
  // Setup Periodic Led Flashing
  mgos_set_timer(1000, true, led_status, NULL);
  // Setup Periodic Reporting of HTTP Server Info
  mgos_set_timer(10000, true, server_status, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
