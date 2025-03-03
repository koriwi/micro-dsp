/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "bt_app_core.h"
#include "driver/i2s.h"
#include "dsp.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "freertos/xtensa_api.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static xTaskHandle s_aux_i2s_task_handle = NULL;

// i2s helper
size_t bytes_written = 0;
size_t bytes_read = 0;

int16_t i2s_rx_buffer[128];

static void aux_i2s_task_handler(void *arg) {

  ESP_LOGI("I2S", "I2S Task started on Core %d", xPortGetCoreID());

  while (1) {
    i2s_read(0, i2s_rx_buffer, sizeof(i2s_rx_buffer), &bytes_read,
             portMAX_DELAY);
    DoDSP(i2s_rx_buffer, bytes_read, 1.0f);
    i2s_write(1, i2s_rx_buffer, bytes_read, &bytes_written, portMAX_DELAY);
  }
}

void aux_i2s_task_start_up(void) {

  // startup Aux Task
  if (s_aux_i2s_task_handle == NULL) {
    xTaskCreate(aux_i2s_task_handler /*generate_sine_wave*/, "AuxI2ST", 3072,
                NULL, configMAX_PRIORITIES, &s_aux_i2s_task_handle);
  }
}
