#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <cJSON.h>

#include "utilities.h"
#include "battery.h"
#include "network_requests.h"
#include "network.h"
#include "location.h"
#include "ble.h"
#include "version.h"
#include "serial.h"
#include "diagnostic.h"

// UART over USB
#ifdef CONFIG_UART_LINE_CTRL
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
  "Console device is not ACM CDC UART device");
#endif

Network network;
Location location;
NetworkRequests network_requests;

static k_work_delayable work;
struct k_work_q periodic_work_q;
K_THREAD_STACK_DEFINE(periodic_stack_area, 2048);
const k_work_queue_config periodic_work_q_config = {
  .name = "periodic_work_q",
};

static void handle_loop_work(struct k_work* work_item) {
  if (network.has_token() && !ble_is_busy() && !diagnostic_running) {
    printk("⏰  [%lld] Bat: %d%%, Checking if background work is scheduled...", k_uptime_get(), last_batt_reading.percent);
    if(last_batt_reading.percent <= 5) {
      Utilities::write_rgb_low_battery();
    } else if (battery_should_send_update()) {
      battery_update();
    } else if (location.should_warm_up()) {
      battery_update_cache();
      if(last_batt_reading.percent > 10) location.start_warm_up();
    } else if (location.should_send_update()) {
      location.send_update(last_batt_reading.real_mV, last_batt_reading.percent);
    }
    printk("\tBackground work complete\n");
  }

  k_work_schedule(&work, K_SECONDS(10));
}

int main(void)
{
  // UART over USB
#ifdef CONFIG_UART_LINE_CTRL
  const struct device* dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  uint32_t dtr = 0;
  if (usb_enable(NULL)) {
    return 0;
  }
  /* Poll if the DTR flag was set, DO NOT SET TO WHILE or device will wait until console connected */
  if (!dtr) {
    uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
    /* Give CPU resources to low priority threads. */
    k_sleep(K_MSEC(100));
  }
#endif

  int64_t boot_time = k_uptime_get();
  printk("############### Sketch version %s ###############\n", VERSION);
  printk("Booting...\n");
  printk("Board: %s\n", CONFIG_BOARD);
  Utilities::setup_pins();
  k_msleep(1000);

  printk("Starting dance\n");
  Utilities::happy_dance();
  printk("\t✔️  Dance complete\n");

  printk("Checking for peripheral connections to SIM module...\n");
  if (network.init() != 0) {
    printk("\tSIM module failed to initialize\n");
    return 1;
  }
  printk("\t✔️   SIM peripherals ready\n");

  network_requests.init(&network);
  location.init(&network, &network_requests);

  // TODO battery init should store battery level when modem off
  printk("Initializing Battery functionality...\n");
  if (battery_init(&network_requests)) {
    printk("Battery init failed\n");
    return 1;
  }
  while(battery_read().percent < 5) {
    Utilities::write_rgb_low_battery();
    
    k_msleep(10000);
  }
  battery_update_cache();

  network.set_power(true);

  while (!network.wait_for_power_on()) {
    printk("\tSIM module failed to power on, retrying\n");
  }

  location.set_gps_power(false);

  printk("\tLocation turned off\n");
  while (!network.configure_modem()) {
    printk("\tSIM module failed to be configured, retrying\n");
  }

  while (strlen(network.device_imei) < 1) {
    network.get_imei();
    printk("\tDevice IMEI: %s\n", network.device_imei);
  }
  printk("\t✔️  SIM module setup complete\n");

  printk("Intializing BLE peripheral, RTC, and button driven interrupts...\n");
  if (init_ble(&network_requests, &network) == 0) {
    printk("\t✔️  BLE, RTC, and IRQs ready\n");
  } else return 1;

  printk("Checking peristent storage for saved configs...\n");
  network.initialize_access_token();
  printk("\t✔️  Persistent storage ready\n");

  if (network.has_token() && network.set_power_on_and_wait_for_reg()) {
    char sensor_query[] = "{\\\"query\\\":\\\"query getMySensors{hubViewer{sensors{id,serial}}}\\\",\\\"variables\\\":{}}";
    cJSON* doc = network.send_request(sensor_query);
    cJSON* sensors = cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "data"), "hubViewer"), "sensors");
    if (cJSON_GetArraySize(sensors)) {
      cJSON* sensor;
      printk("Array size is %d\n", cJSON_GetArraySize(sensors));
      cJSON_ArrayForEach(sensor, sensors) {
        printk("Sensor id %d, and serial is %s\n",
          cJSON_GetObjectItem(sensor, "id")->valueint,
          cJSON_GetObjectItem(sensor, "serial")->valuestring
        );
        add_known_sensor(cJSON_GetObjectItem(sensor, "serial")->valuestring);
      }
    }
    cJSON_Delete(doc);
  }

  network.set_power(false);

  printk("\n>>>>> Setup complete in %lli(ms)! <<<<<\n\n", k_uptime_delta(&boot_time));

  k_msleep(1000);
  printk(">>>> Starting main loop <<<<\n");
  k_msleep(2000);
  start_scan();

  k_work_queue_start(&periodic_work_q, periodic_stack_area,
    K_THREAD_STACK_SIZEOF(periodic_stack_area),
    CONFIG_MAIN_THREAD_PRIORITY + 1, &periodic_work_q_config);
  k_work_init_delayable(&work, handle_loop_work);
  k_work_schedule(&work, K_SECONDS(10));

  return 0;
}
