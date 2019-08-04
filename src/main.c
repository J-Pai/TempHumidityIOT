#include "mgos.h"
#include "mgos_system.h"
#include "mgos_dht.h"
#include "mgos_http_server.h"

#define C_TO_F(c) (((c) * 1.8f) + 32.0f)

#define MAIN_LED 16
#define DATA_POINT_LEN 52
#define HISTORY_POINT_LEN (DATA_POINT_LEN + 11)
#define SECONDARY_LED mgos_sys_config_get_board_led1_pin()
#define TERTIARY_LED mgos_sys_config_get_board_led2_pin()

typedef struct Sensor_DHT {
  struct mgos_dht * sensor;
  struct mgos_rlock_type * data_lock;
  float temperature;
  float humidity;
  struct mbuf history;
} Sensor_DHT;

static void dht_measurement(void * arg) {
  Sensor_DHT * dht = (Sensor_DHT *)arg;
  static bool dht_tick_tock = false;
  float temp = -999.9f;
  float humi = -999.9f;
  if (dht_tick_tock) {
    temp = mgos_dht_get_temp(dht->sensor);
    if (mgos_sys_config_get_app_dht_fahrenheit()) {
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
  /*
  if (!mgos_sys_config_get_app_silent()) {
    LOG(LL_INFO, ("Pin: %d | Temperature: %05.1f | Humidity: %05.1f (Offset: %d)",
      mgos_sys_config_get_app_dht_pin(),
      temp,
      humi,
      mgos_sys_config_get_app_dht_humidity_offset()));
  }
  */
  dht_tick_tock = !dht_tick_tock;
}

static void server_status(void * arg) {
  enum mgos_wifi_status status = mgos_wifi_get_status();
  static bool s_tick_tock = false;
  if (status == MGOS_WIFI_IP_ACQUIRED) {
    char * ssid = mgos_wifi_get_connected_ssid();
    struct mgos_net_ip_info ip_info;
    bool obtained = mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, 0, &ip_info);
    char ip[16] = "";
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
  if (!mgos_sys_config_get_app_silent()) {
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

static void get_uptime_handler(struct mg_connection * c, int ev, void * p, void * user_data) {
  (void) user_data;
  (void) p;
  if (ev != MG_EV_HTTP_REQUEST) return;
  LOG(LL_INFO, ("Uptime Requested"));
  time_t now;
  time(&now);
  // 9223372036854775807 --> 19 Chars
  char uptime[19] = "";
  sprintf(uptime, "%019ld", now);
  mg_send_response_line(c, 200,
                        "Content-Type: text/plain; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n");
  mg_send(c, uptime, 19);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void get_dht_data_handler(struct mg_connection * c, int ev, void * p, void * user_data) {
  Sensor_DHT * dht = (Sensor_DHT *)user_data;
  (void) p;
  if (ev != MG_EV_HTTP_REQUEST) return;
  mgos_rlock(dht->data_lock);
  float temp = dht->temperature;
  float humi = dht->humidity;
  mgos_runlock(dht->data_lock);
  LOG(LL_INFO, ("DHT Data Requested"));
  time_t now;
  time(&now);
  char data[DATA_POINT_LEN] = "";
  // {"d":"9223372036854775807","t":000.0F","h":"000.0"}
  sprintf(data, "{\"d\":\"%019ld\",\"t\":\"%05.1f%c\",\"h\":\"%05.1f\"}",
    now, temp, mgos_sys_config_get_app_dht_fahrenheit() ? 'F' : 'C', humi);
  mg_send_response_line(c, 200,
                        "Content-Type: text/plain; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n");
  mg_send(c, data, DATA_POINT_LEN);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void get_dht_history_data_handler(struct mg_connection * c, int ev, void * p, void * user_data) {
  enum mgos_wifi_status status = mgos_wifi_get_status();
  if (status != MGOS_WIFI_IP_ACQUIRED) {
    return;
  }
  Sensor_DHT * dht = (Sensor_DHT *)user_data;
  (void) p;
  if (ev != MG_EV_HTTP_REQUEST) return;
  mgos_rlock(dht->data_lock);
  size_t len = dht->history.len;
  char histStore[len];
  MEMCPY(histStore, dht->history.buf, len);
  mgos_runlock(dht->data_lock);
  LOG(LL_INFO, ("DHT History Data Requested"));
  mg_send_response_line(c, 200,
                        "Content-Type: text/plain; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n");
  mg_send(c, histStore, len);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void store_dht_measurement(void * arg) {
  enum mgos_wifi_status status = mgos_wifi_get_status();
  if (status != MGOS_WIFI_IP_ACQUIRED) {
    return;
  }
  Sensor_DHT * dht = (Sensor_DHT *)arg;
  static unsigned int index = 0;
  mgos_rlock(dht->data_lock);
  float temp = dht->temperature;
  float humi = dht->humidity;
  mgos_runlock(dht->data_lock);
  time_t now;
  time(&now);
  /**
   * Ensure that current time is past 2019/01/01.
   *
   * If time is before that, chances are internal time has been reset...
   */
  if (temp >= 0 && humi >= 0) {
    char data[HISTORY_POINT_LEN] = "";
    // ,{"i":"000","d":"9223372036854775807","t":000.0F","h":"000.0"}
    sprintf(data, ",{\"i\":\"%03u\",\"d\":\"%019ld\",\"t\":\"%05.1f%c\",\"h\":\"%05.1f\"}",
      index, now, temp, mgos_sys_config_get_app_dht_fahrenheit() ? 'F' : 'C', humi);
    // LOG(LL_INFO, ("Data: [%s]", data));
    mgos_rlock(dht->data_lock);
    int num_elements = (dht->history.len + 1) / HISTORY_POINT_LEN;
    if (num_elements >= mgos_sys_config_get_app_dht_history_length()) {
      mbuf_remove(&dht->history, HISTORY_POINT_LEN);
    }
    num_elements = (dht->history.len + 1) / HISTORY_POINT_LEN;

    if (num_elements < mgos_sys_config_get_app_dht_history_length()) {
      size_t appended = mbuf_append(
        &dht->history,
        dht->history.len ? data : &data[1],
        (dht->history.len ? HISTORY_POINT_LEN : HISTORY_POINT_LEN - 1) * sizeof(char));
      num_elements = (dht->history.len + 1) / HISTORY_POINT_LEN;
      if (!appended) {
        LOG(LL_ERROR, ("Temperature not stored! [Num Elements: %u, Appended Bytes: %d, mbuf Len: %d, mbuf Size: %d] [Temperature: %05.1f | Humidity: %05.1f (Offset: %d)]",
          num_elements,
          appended,
          dht->history.len,
          dht->history.size,
          temp,
          humi,
          mgos_sys_config_get_app_dht_humidity_offset()));
      } else { // if (!mgos_sys_config_get_app_silent()) {
        LOG(LL_INFO, ("Stored DHT Measurement! [Num Elements: %u, Appended Bytes: %d, mbuf Len: %d, mbuf Size: %d] [Temperature: %05.1f | Humidity: %05.1f (Offset: %d)]",
          num_elements,
          appended,
          dht->history.len,
          dht->history.size,
          temp,
          humi,
          mgos_sys_config_get_app_dht_humidity_offset()));
          index = (index + 1) % 999;
      }
    } else {
      LOG(LL_ERROR, ("Bad config history length?"));
    }
    mgos_runlock(dht->data_lock);
  }
}

enum mgos_app_init_result mgos_app_init(void) {
  // Initializing DHT Structure
  Sensor_DHT * dht = (Sensor_DHT *)malloc(sizeof(Sensor_DHT));

  dht->sensor = mgos_dht_create(mgos_sys_config_get_app_dht_pin(), DHT11);
  dht->data_lock = mgos_rlock_create();
  dht->temperature = 0.0f;
  dht->humidity = 0.0f;
  mbuf_init(&dht->history, 0);

  // Initializing Status LEDs
  mgos_gpio_setup_output(MAIN_LED, 1);
  mgos_gpio_setup_output(SECONDARY_LED, 1);
  mgos_gpio_setup_output(TERTIARY_LED, 1);

  mgos_register_http_endpoint("/uptime", get_uptime_handler, NULL);
  // Setup HTTP call to obtain current Temperature and Humidity
  mgos_register_http_endpoint("/dht", get_dht_data_handler, dht);
  // Setup HTTP call to obtain history Temperature and Humidity
  mgos_register_http_endpoint("/dht/history", get_dht_history_data_handler, dht);
  // Setup Periodic Measurement of Temperature and Humidity
  mgos_set_timer(2500, true, dht_measurement, dht);
  // Setup Periodic Storing of Temperature and Humidity
  mgos_set_timer(mgos_sys_config_get_app_dht_history_period(), true, store_dht_measurement, dht);
  // Setup Periodic Led Flashing
  mgos_set_timer(1000, true, led_status, NULL);
  // Setup Periodic Reporting of HTTP Server Info
  mgos_set_timer(10000, true, server_status, NULL);
  return MGOS_APP_INIT_SUCCESS;
}