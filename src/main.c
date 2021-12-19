#include "mgos.h"
#include "mgos_system.h"
#include "mgos_dht.h"
#include "mgos_http_server.h"

#define MAIN_LED 16
#define SECONDARY_LED mgos_sys_config_get_board_led1_pin()
#define TERTIARY_LED mgos_sys_config_get_board_led2_pin()

#define C_TO_F(c) (((c) * 1.8f) + 32.0f)
#define DATA_POINT_FORMAT \
        "{\"d\":\"%019ld\",\"t\":\"%05.1f%c\",\"h\":\"%05.1f\"}"
#define DATA_POINT_LEN 53
#define HTTP_HDR \
        "Content-Type: text/plain; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\n"
#define HIST_DATA_STR_LEN (DHT_HISTORY_MAX_SIZE * DATA_POINT_LEN + 1)

struct Data {
  time_t timestamp;
  float temperature;
  float humidity;
};

struct SensorDHT {
  struct mgos_dht *sensor;
  float temperature;
  float humidity;
  uint16_t start;
  uint16_t end;
  struct Data history[DHT_HISTORY_MAX_SIZE];
};

struct mgos_rlock_type *data_lock;
struct SensorDHT dht;
bool dht_toggle = false;

static void dht_measurement(void *arg) {
  float temp = -999.9f;
  float humi = -999.9f;
  mgos_rlock(data_lock);
  if (dht_toggle) {
    temp = mgos_dht_get_temp(dht.sensor);
    if (mgos_sys_config_get_app_dht_fahrenheit()) {
      temp = C_TO_F(temp);
    }
    humi = dht.humidity;
    dht.temperature = temp;
  } else {
    humi = mgos_dht_get_humidity(dht.sensor) +
           mgos_sys_config_get_app_dht_humidity_offset();
    dht.humidity = humi;
    temp = dht.temperature;
  }

  LOG(LL_INFO, ("Pin: %d | Temperature: %05.1f | Humidity: %05.1f (Offset: %d)",
      DHT_PIN,
      temp,
      humi,
      mgos_sys_config_get_app_dht_humidity_offset()));

  dht_toggle = !dht_toggle;
  mgos_runlock(data_lock);
}

static void server_status(void *arg) {
  enum mgos_wifi_status status = mgos_wifi_get_status();

  // Garbage college.
  mgos_fs_gc();
  mgos_wdt_feed();

  static bool s_tick_tock = false;

  if (status == MGOS_WIFI_IP_ACQUIRED) {
    char * ssid = mgos_wifi_get_connected_ssid();
    struct mgos_net_ip_info ip_info;
    bool obtained = mgos_net_get_ip_info(MGOS_NET_IF_TYPE_WIFI, 0, &ip_info);
    char ip[16] = "";
    mgos_net_ip_to_str(&(ip_info.ip), ip);
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free, %lu min_free | "
                  "Network Connected (Status: %d)[%s][%s]",
                  (s_tick_tock ? "Tick" : "Tock"),
                  mgos_uptime(), (unsigned long) mgos_get_heap_size(),
                  (unsigned long) mgos_get_free_heap_size(),
                  (unsigned long) mgos_get_min_free_heap_size(),
                  status, ssid, true == obtained ? ip : ""));
    free(ssid);
  } else if (status == MGOS_WIFI_CONNECTED || status == MGOS_WIFI_CONNECTING) {
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free, %lu min_free | "
                  "Network Pending... (Status: %d)",
                  (s_tick_tock ? "Tick" : "Tock"),
                  mgos_uptime(), (unsigned long) mgos_get_heap_size(),
                  (unsigned long) mgos_get_free_heap_size(),
                  (unsigned long) mgos_get_min_free_heap_size(),
                  status));
  } else {
    LOG(LL_INFO, ("%s uptime: %.2lf, RAM: %lu, %lu free, %lu min_free | "
                  "Network Disconnected (Status: %d)",
                  (s_tick_tock ? "Tick" : "Tock"),
                  mgos_uptime(), (unsigned long) mgos_get_heap_size(),
                  (unsigned long) mgos_get_free_heap_size(),
                  (unsigned long) mgos_get_min_free_heap_size(),
                  status));
  }
  s_tick_tock = !s_tick_tock;
}

static void led_status(void *arg) {
  uint8_t sw = mgos_gpio_read(LED_ENABLE_PIN);

  if (sw == true) {
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
}

static void get_uptime_handler(struct mg_connection *c, int ev, void *p,
    void *user_data) {
  if (ev != MG_EV_HTTP_REQUEST)
    return;
  LOG(LL_INFO, ("Uptime Requested"));
  time_t now;
  time(&now);
  // 9223372036854775807 --> 19 Chars
  char uptime[20];
  sprintf(uptime, "%019ld", now);
  mg_send_response_line(c, 200, HTTP_HDR);
  mg_send(c, uptime, 19);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void get_dht_data_handler(struct mg_connection *c, int ev, void *p,
    void *user_data) {
  if (ev != MG_EV_HTTP_REQUEST)
    return;
  mgos_rlock(data_lock);
  float temp = dht.temperature;
  float humi = dht.humidity;
  mgos_runlock(data_lock);
  LOG(LL_INFO, ("DHT Data Requested"));
  time_t now;
  time(&now);
  char data[DATA_POINT_LEN];
  sprintf(data, DATA_POINT_FORMAT, now, temp,
          mgos_sys_config_get_app_dht_fahrenheit() ? 'F' : 'C', humi);
  mg_send_response_line(c, 200, HTTP_HDR);
  mg_send(c, data, DATA_POINT_LEN - 1);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void get_dht_history_data_handler(struct mg_connection *c, int ev,
    void *p, void *user_data) {
  if (ev != MG_EV_HTTP_REQUEST)
    return;
  LOG(LL_INFO, ("DHT History Data Requested"));

  mgos_rlock(data_lock);
  LOG(LL_INFO, ("Data request start..."));

  char hist_data[HIST_DATA_STR_LEN];
  hist_data[0] = 0;

  uint16_t history_index = dht.start;
  uint16_t index = 0;
  uint16_t first = 1;

  char data_point[DATA_POINT_LEN + 1];

  while (first == 1 || history_index != dht.start) {
    first = 0;
    if (dht.history[history_index].timestamp == 0) {
      history_index = (history_index + 1) % DHT_HISTORY_MAX_SIZE;
      continue;
    }

    sprintf(data_point,
            DATA_POINT_FORMAT,
            dht.history[history_index].timestamp,
            dht.history[history_index].temperature,
            mgos_sys_config_get_app_dht_fahrenheit() ? 'F' : 'C',
            dht.history[history_index].humidity);
    data_point[DATA_POINT_LEN - 1] = ',';
    data_point[DATA_POINT_LEN] = 0;

    memcpy(hist_data + (DATA_POINT_LEN * index), data_point, DATA_POINT_LEN);

    history_index = (history_index + 1) % DHT_HISTORY_MAX_SIZE;
    ++index;
  }
  hist_data[DATA_POINT_LEN * index - 1] = 0;

  LOG(LL_INFO, ("Data request end..."));
  mgos_runlock(data_lock);

  mg_send_response_line(c, 200, HTTP_HDR);
  mg_send(c, hist_data, DATA_POINT_LEN * index);
  c->flags |= (MG_F_SEND_AND_CLOSE);
}

static void store_dht_measurement(void *arg) {
  mgos_rlock(data_lock);

  float temp = dht.temperature;
  float humi = dht.humidity;
  time_t now;
  time(&now);

  if (temp >= 0 && humi >= 0) {
    uint16_t index = dht.end;
    LOG(LL_INFO, ("Storing DHT Measurement "
        "Index: %d | Temperature: %05.1f | Humidity: %05.1f (Offset: %d)",
        index,
        temp,
        humi,
        mgos_sys_config_get_app_dht_humidity_offset()));

    dht.history[index].timestamp = now;
    dht.history[index].temperature = temp;
    dht.history[index].humidity = humi;

    dht.end = (index + 1) % DHT_HISTORY_MAX_SIZE;
    if (dht.end == dht.start) {
      dht.start = (dht.start + 1) % DHT_HISTORY_MAX_SIZE;
    }
  }
  mgos_runlock(data_lock);
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_wdt_set_timeout(60);
  mgos_wdt_enable();

  // Initializing DHT Structure
  dht.sensor = mgos_dht_create(DHT_PIN, DHT11);
  data_lock = mgos_rlock_create();
  dht.temperature = 0.0f;
  dht.humidity = 0.0f;
  dht.start = 0;
  dht.end = 0;

  memset(&dht.history, 0, sizeof(dht.history));

  // Initializing Status LEDs
  mgos_gpio_setup_output(MAIN_LED, 1);
  mgos_gpio_setup_output(SECONDARY_LED, 1);
  mgos_gpio_setup_output(TERTIARY_LED, 1);

  // Initializing Switch Pin
  mgos_gpio_set_mode(LED_ENABLE_PIN, MGOS_GPIO_MODE_INPUT);
  mgos_gpio_set_pull(LED_ENABLE_PIN, MGOS_GPIO_PULL_UP);

  mgos_register_http_endpoint("/uptime", get_uptime_handler, NULL);
  // Setup HTTP call to obtain current Temperature and Humidity
  mgos_register_http_endpoint("/dht", get_dht_data_handler, NULL);
  // Setup HTTP call to obtain history Temperature and Humidity
  mgos_register_http_endpoint("/dht/history", get_dht_history_data_handler,
      NULL);
  // Setup Periodic Measurement of Temperature and Humidity
  mgos_set_timer(2500, true, dht_measurement, NULL);
  // Setup Periodic Storing of Temperature and Humidity
  mgos_set_timer(DHT_HISTORY_PERIOD, true, store_dht_measurement, NULL);
  // Setup Periodic Led Flashing
  mgos_set_timer(1000, true, led_status, NULL);
  // Setup Periodic Reporting of HTTP Server Info
  mgos_set_timer(10000, true, server_status, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
