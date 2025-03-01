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
size_t readsize = 0;
int blocksize = 512;

// fifo
signed long data32b[128], data32b2[128];
int wptr = 0, rptr = 0;
uint64_t wptrtotal = 0, rptrtotal = 0;

static int entries() { return wptrtotal - rptrtotal; }

static void aux_i2s_task_handler(void *arg) {

  ESP_LOGI("I2S", "I2S Task started on Core %d", xPortGetCoreID());

  for (int i = 0; i < 128; i++) {
    data32b[i] = 0;
  }

  while (1) {
    wptr = 0;
    rptr = 0;
    wptrtotal = 0;
    rptrtotal = 0;

    i2s_read(0, data32b, blocksize, &readsize, portMAX_DELAY);

    DoDSP(data32b, blocksize, 1.0f);

    i2s_write(1, data32b, readsize, &bytes_written, portMAX_DELAY);
  }
}

void aux_i2s_task_start_up(void) {

  // startup Aux Task
  if (s_aux_i2s_task_handle == NULL) {
    xTaskCreate(aux_i2s_task_handler /*generate_sine_wave*/, "AuxI2ST", 3072,
                NULL, configMAX_PRIORITIES, &s_aux_i2s_task_handle);
  }
}
