// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/i2s_types.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bt_app_core.h"
#include "driver/i2s.h"
#include "dsp.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "server.h"
#include <lwip/netdb.h>
#include <rom/rtc.h>

#define EXAMPLE_ESP_WIFI_SSID "micro-dsp"
#define EXAMPLE_ESP_WIFI_PASS "jnfcxuz7"
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN
static const char *TAG = "wifi softAP";
void initWIFI();
void initAudioAuxMode();
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

#define CONFIG_BT_SSP_ENABLED false

void app_main(void) {
  /* Initialize NVS — it is used to store PHY calibration data */
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_LOGE("heelp", "help");
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  RestoreParametersFromFlash();
  initAudioAuxMode();
  initWIFI();
}

void initAudioAuxMode() {

  initDSPFilters(44100.0f);
  PrintParameters();
  i2s_config_t i2s_config = {

      .mode = I2S_MODE_SLAVE | I2S_MODE_RX, // Only TX

      .sample_rate = 44100,
      .bits_per_sample = 16,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // 2-channels
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .dma_buf_count = 4,
      .dma_buf_len = 128,
      .intr_alloc_flags = 0, // Default interrupt priority
      .tx_desc_auto_clear = false,
      .use_apll = false // Auto clear tx descriptor on underflow
  };

  i2s_config_t i2s_config2 = {

      .mode = I2S_MODE_MASTER | I2S_MODE_TX,
      .sample_rate = 44100,
      .bits_per_sample = 16,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // 2-channels
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // Interrupt level
      .dma_buf_count = 4,
      .dma_buf_len = 128,
      .fixed_mclk = 0,
      .tx_desc_auto_clear = true,
      .use_apll = true // Auto clear tx descriptor on underflow
  };

  i2s_pin_config_t pin_config = {.bck_io_num = 17,
                                 .ws_io_num = 16,
                                 .data_out_num = I2S_PIN_NO_CHANGE,
                                 .data_in_num = 21};

  i2s_pin_config_t pin_config2 = {.bck_io_num = 23,
                                  .ws_io_num = 18,
                                  .data_out_num = 19,
                                  .data_in_num = I2S_PIN_NO_CHANGE};

  i2s_driver_install(0, &i2s_config, 0, NULL);
  i2s_set_pin(0, &pin_config);
  i2s_driver_install(1, &i2s_config2, 0, NULL);
  i2s_set_pin(1, &pin_config2);

  // enable MCLK on GPIO0
  // REG_WRITE(PIN_CTRL, 0xFF0);
  // PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);

  aux_i2s_task_start_up();
}

void initWIFI() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

  wifi_config_t wifi_config = {
      .ap = {.ssid = EXAMPLE_ESP_WIFI_SSID,
             .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
             .channel = EXAMPLE_ESP_WIFI_CHANNEL,
             .password = EXAMPLE_ESP_WIFI_PASS,
             .max_connection = EXAMPLE_MAX_STA_CONN,
             .authmode = WIFI_AUTH_WPA_WPA2_PSK},
  };
  if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
           EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS,
           EXAMPLE_ESP_WIFI_CHANNEL);

  xTaskCreatePinnedToCore(tcp_server_task, "tcp_server", 4096, (void *)AF_INET,
                          configMAX_PRIORITIES - 3, NULL, 0);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac),
             event->aid);
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac),
             event->aid);
  }
}
