/**
 * @file htu21d.h
 * @brief HTU21D Sensor ESP-IDF Component header file.
 *
 * esp-idf component to interface with HTU21D humidity and temperature sensor
 * by TE Connectivity (http://www.te.com/usa-en/product-CAT-HSC0004.html)
 *
 * @author Dentella, www.lucadentella.it
 * @author rob4226 <rob4226@yahoo.com>
 * @date 10.8.2017, 11.29.2023
 */

#pragma once

#ifndef __ESP_HTU21D_H__
#define __ESP_HTU21D_H__

#include "esp_err.h"
#include "driver/i2c.h"
#include "freertos/task.h"

#define HTU21D_ADDR     0x40 /**< I2C address of the HTU21D sensor. */

// HTU21D commands
#define TRIGGER_TEMP_MEASURE_HOLD       0xE3
#define TRIGGER_HUMD_MEASURE_HOLD       0xE5
#define TRIGGER_TEMP_MEASURE_NOHOLD     0xF3
#define TRIGGER_HUMD_MEASURE_NOHOLD     0xF5
#define WRITE_USER_REG                  0xE6
#define READ_USER_REG                   0xE7
#define SOFT_RESET                      0xFE

// return values
#define HTU21D_ERR_OK               0x00
#define HTU21D_ERR_CONFIG           0x01
#define HTU21D_ERR_INSTALL          0x02
#define HTU21D_ERR_NOTFOUND         0x03
#define HTU21D_ERR_INVALID_ARG      0x04
#define HTU21D_ERR_FAIL             0x05
#define HTU21D_ERR_INVALID_STATE    0x06
#define HTU21D_ERR_TIMEOUT          0x07

#ifdef __cplusplus
extern "C" {
#endif

// functions
int htu21d_init(i2c_port_t port, int sda_pin, int scl_pin, gpio_pullup_t sda_internal_pullup, gpio_pullup_t scl_internal_pullup);
float htu21d_read_temperature();
float htu21d_read_humidity();
uint8_t htu21d_get_resolution();
int htu21d_set_resolution(uint8_t resolution);
int htu21d_soft_reset();

// helper functions
uint8_t htu21d_read_user_register();
int htu21d_write_user_register(uint8_t value);
uint16_t read_value(uint8_t command);
bool is_crc_valid(uint16_t value, uint8_t crc);

// Extra functions:
float celsius_to_fahrenheit(float celsius_degrees);
float htu21_compute_compensated_humidity(float temperature, float relative_humidity);
float htu21d_compute_partial_pressure(float temperature);
float htu21d_compute_dew_point(float temperature, float relative_humidity);

#ifdef __cplusplus
}
#endif

#endif  // __ESP_HTU21D_H__
