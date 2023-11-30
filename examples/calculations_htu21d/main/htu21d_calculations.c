/**
 * @file htu21d_calculations.c
 * @brief Example of calculations using this HTU21D sensor ESP-IDF component.
 * @author Rob4226 <Rob4226@yahoo.com>
 * @version 0.1
 * @date 11.29.2023
 * @copyright MIT License 2023
 */

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "htu21d.h"

#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2

static const char *TAG = "EXAMPLE";

void app_main(void) {
  ESP_ERROR_CHECK(htu21d_init(I2C_NUM_0, I2C_SDA_PIN, I2C_SCL_PIN,
                              GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE));

  ESP_LOGI(TAG,
           "The I2C bus was setup successfully and the HTU21D sensor found!");

  while (1) {
    // Read sensor values:
    float temp = htu21d_read_temperature();
    float humidity = htu21d_read_humidity();

    // Calculations using read sensor values:
    float temp_compensated_humidity =
        htu21_compute_compensated_humidity(temp, humidity);
    float dew_point = htu21d_compute_dew_point(temp, temp_compensated_humidity);
    float partial_pressure = htu21d_compute_partial_pressure(temp);

    ESP_LOGI(TAG,
             "Temperature: %.02f°C / %.02f°F\n"
             "Humidity: %.02f%% / Temperature Compensated Humidity: %.02f%%\n"
             "Dew Point: %.02f°C / %.02f°F\n"
             "Partial Pressure: %.02fmmHg\n",
             temp, celsius_to_fahrenheit(temp), humidity,
             temp_compensated_humidity, dew_point,
             celsius_to_fahrenheit(dew_point), partial_pressure);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
