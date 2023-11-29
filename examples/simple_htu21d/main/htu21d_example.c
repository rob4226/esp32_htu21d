/**
 * @file htu21_example.c
 * @author Rob4226 (Rob4226@yahoo.com)
 * @brief Example of using this HTU21D sensor ESP-IDF component.
 * @version 0.1
 * @date 11.27.2023 3:57PM
 * @copyright MIT License 2023
 */

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "htu21d.h"

static const char *TAG = "EXAMPLE";

void app_main(void) {
  ESP_ERROR_CHECK(
      htu21d_init(I2C_NUM_0, 1, 2, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE));

  ESP_LOGI(TAG,
           "The I2C bus was setup successfully and the HTU21D sensor found!");

  while (1) {
    float temp = ht21d_read_temperature();
    float humidity = ht21d_read_humidity();

    ESP_LOGI(TAG, "Temperature: %.02f°C / %.02f°F  Humidity: %.02f%%", temp,
             celsius_to_fahrenheit(temp), humidity);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
