/**
 * @file htu21d.c
 * @brief HTU21D Sensor ESP-IDF Component source code file.
 *
 * ESP-IDF component to interface with the HTU21D humidity and temperature sensor
 * by TE Connectivity (http://www.te.com/usa-en/product-CAT-HSC0004.html).
 *
 * @author Luca Dentella, www.lucadentella.it
 * @author rob4226 <rob4226@yahoo.com>
 * @date 10.8.2017, 11.29.2023
 */

#include <math.h>
#include "esp_log.h"
#include "htu21d.h"

#define HTU21_TEMPERATURE_COEFFICIENT   (-0.15F)   /**< Used in equation to convert Measured Relative Humidity to Temperature Compensated Relative Humidity. */
#define HTU21_CONSTANT_A                (8.1332F)  /**< Constant `A` used in Partial Pressure from Ambient Temperature formula. */
#define HTU21_CONSTANT_B                (1762.39F) /**< Constant `B` used in Partial Pressure from Ambient Temperature formula. */
#define HTU21_CONSTANT_C                (235.66F)  /**< Constant `C` used in Partial Pressure from Ambient Temperature formula. */
#define HTU21_RESET_TIME                (15)       /**< It takes the HTU21D 15ms or less for a soft reset. */

static const char* TAG = "htu21d_driver";

static i2c_port_t _port = 0; /**< The I2C port that the HTU21D sensor is connected to. */

/**
 * @brief Initializes the HTU21D temperature/humidity sensor and the I2C bus.
 *
 * I2C bus runs in master mode @ 100,000.
 * @param port I2C port number to use, can be `I2C_NUM_0` ~ (`I2C_NUM_MAX` - 1).
 * @param sda_pin The GPIO pin number to use for the I2C sda (data) signal.
 * @param scl_pin The GPIO pin number to use for the I2C scl (clock) signal.
 * @param sda_internal_pullup Internal GPIO pull mode for I2C sda signal.
 * @param scl_internal_pullup Internal GPIO pull mode for I2C scl signal.
 * @return Returns #HTU21D_ERR_OK if I2C bus is initialized successfully and the
 * HTU21D sensor is found. Returns #HTU21D_ERR_CONFIG if there is an error
 * configuring the I2C bus. Returns #HTU21D_ERR_INSTALL if the I2C driver fails
 * to install. Returns #HTU21D_ERR_NOTFOUND if the HTU21D sensor could not be
 * found on the I2C bus. Also, a soft reset command is sent, so any error that
 * #htu21d_soft_reset returns is also possible.
 */
int htu21d_init(i2c_port_t port, int sda_pin, int scl_pin,  gpio_pullup_t sda_internal_pullup,  gpio_pullup_t scl_internal_pullup)
{
    esp_err_t ret;
    _port = port;

    // setup i2c controller
    i2c_config_t conf = {0};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda_pin;
    conf.scl_io_num = scl_pin;
    conf.sda_pullup_en = sda_internal_pullup;
    conf.scl_pullup_en = scl_internal_pullup;
    conf.master.clk_speed = 100000;
    ret = i2c_param_config(port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C (port %d, sda_pin %d, scl_pin %d): %s", port, sda_pin, scl_pin, esp_err_to_name(ret));
        return HTU21D_ERR_CONFIG;
    }

    // install the driver
    ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
        return HTU21D_ERR_INSTALL;
    }

    // verify if a sensor is present
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return HTU21D_ERR_FAIL;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(
                                      cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTU21D sensor not found on bus: %s", esp_err_to_name(ret));
        return HTU21D_ERR_NOTFOUND;
    }
    ESP_LOGI(TAG, "HTU21D sensor initialized successfully.");

    // Per datasheet, it is recommended to soft reset the HTU21D sensor on start:
    ret = htu21d_soft_reset();
    if (ret != HTU21D_ERR_OK) {
        ESP_LOGE(TAG, "Failed to soft reset the HTU21D sensor after initializing it, error: 0x%02X", ret);
        return ret;
    }

    return HTU21D_ERR_OK;
}

/**
 * @brief Read the temperature from the HTU21D sensor.
 * @return Returns the temperature read from the HTU21D sensor in degrees
 * Celsius. Returns `-999` if it fails to read the temperature from the sensor.
 */
float htu21d_read_temperature()
{
    // get the raw value from the sensor
    uint16_t raw_temperature = read_value(TRIGGER_TEMP_MEASURE_NOHOLD);
    if (raw_temperature == 0) {
        return -999;
    }

    // return the real value, formula in datasheet
    return (raw_temperature * 175.72 / 65536.0) - 46.85;
}

/**
 * @brief Read the relative humidity from the HTU21D sensor.
 *
 * Relative Humidity is the ratio of the actual water vapor pressure in the air
 * to the saturation water vapor pressure in the air at a specific temperature,
 * expressed as a percentage.
 *
 * In other words, relative humidity represents the amount of water vapor
 * present in the air as a percentage of the total amount of water vapor the air
 * can hold relative to temperature.
 *
 * For example, if the relative humidity is at 20% at the ambient air
 * temperature of 25°C, then the air currently holds 20% of the maximum amount
 * of water vapor it can hold at 25°C.
 *
 * If the ambient temperature increases, the air can hold more water vapor, and
 * the relative humidity will decrease since the air can hold more water vapor.
 *
 * ### Temperature Compensated Relative Humidity
 *
 * For greater accuracy, you can run the result of this function, and the
 * current temperature, through the #htu21_compute_compensated_humidity
 * function, which will compensate for the effect that temperature has on
 * humidity.
 * @return Returns the relative humidity percentage % read from the HTU21D
 * sensor. Returns `-999` if it fails to read the humidity from the sensor.
 */
float htu21d_read_humidity()
{
    // get the raw value from the sensor
    uint16_t raw_humidity = read_value(TRIGGER_HUMD_MEASURE_NOHOLD);
    if (raw_humidity == 0) {
        return -999;
    }

    // return the real value, formula in datasheet
    return (raw_humidity * 125.0 / 65536.0) - 6.0;
}

/**
 * @brief Calculates the Partial Pressure at ambient temperature, by using the
 * ambient temperature read from the HTU21D sensor.
 *
 * This function is really only used to calculate the dew point.
 * @param[in] temperature Actual ambient temperature measured from sensor (degC).
 * @return Returns the current Partial Pressure in mmHg at ambient temperature.
 */
float htu21d_compute_partial_pressure(float temperature)
{
    return pow(10, HTU21_CONSTANT_A - HTU21_CONSTANT_B / (temperature + HTU21_CONSTANT_C));
}

/**
 * @brief Calculates the Dew Point.
 *
 * The dew point is the temperature at which the water vapor in the air becomes
 * saturated and condensation begins.
 *
 * The dew point is associated with relative humidity. A high relative humidity
 * indicates that the dew point is closer to the current air temperature.
 * Relative humidity of 100% indicates that the dew point is equal to the
 * current temperature (and the air is maximally saturated with water so it
 * cannot hold any more water). When the dew point stays constant and ambient
 * temperature increases, relative humidity decreases.
 *
 * Dew point temperature of the air is calculated using Ambient Relative
 * Humidity and Temperature measurements from HTU21D sensor.
 * @param[in] temperature Actual ambient temperature (degC) measured from
 * sensor.
 * @param[in] relative_humidity Actual relative humidity (%RH) measured from
 * sensor.
 * @return Returns the calculated Dew Point in °C.
 */
float htu21d_compute_dew_point(float temperature, float relative_humidity)
{
    float partial_pressure = htu21d_compute_partial_pressure(temperature);

    return - HTU21_CONSTANT_B /
           (log10(relative_humidity * partial_pressure / 100.0F) - HTU21_CONSTANT_A)
           - HTU21_CONSTANT_C;
}

uint8_t htu21d_get_resolution()
{
    uint8_t reg_value = htu21d_read_user_register();
    return reg_value & 0b10000001;
}

int htu21d_set_resolution(uint8_t resolution)
{

    // get the actual resolution
    uint8_t reg_value = htu21d_read_user_register();
    reg_value &= 0b10000001;

    // update the register value with the new resolution
    resolution &= 0b10000001;
    reg_value |= resolution;

    return htu21d_write_user_register(reg_value);
}

/**
 * @brief Sends a *Soft Reset* command to reboot the HTU21D sensor.
 *
 * The 'Soft Reset' command is used for rebooting the HTU21D(F) sensor switching
 * the power off and on again. Upon reception of this command, the HTU21D(F)
 * sensor system reinitializes and starts operation according to the default
 * settings with the exception of the heater bit in the user register. This
 * command resets the user register to its default state, with the exception of
 * the Heater bit.
 *
 * The soft reset takes less than 15ms.
 *
 * > Per the datasheet, a soft reset is recommended at start.
 * @return Returns #HTU21D_ERR_OK if the soft reset was successful. If the soft
 * reset was not successful, it returns one of the following error codes:
 *   + #HTU21D_ERR_INVALID_ARG - Parameter error.
 *   + #HTU21D_ERR_FAIL - Command error, the HTU21D slave hasn't ACK the transfer.
 *   + #HTU21D_ERR_INVALID_STATE - I2C driver not installed or not in master mode.
 *   + #HTU21D_ERR_TIMEOUT - Operation timeout because the I2C bus is busy.
 */
int htu21d_soft_reset()
{
    esp_err_t ret;

    // send the command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return HTU21D_ERR_FAIL;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(
                                      cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, SOFT_RESET, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);

    switch (ret) {

    case ESP_ERR_INVALID_ARG:
        ESP_LOGE(TAG, "Soft reset failed, parameter error.");
        return HTU21D_ERR_INVALID_ARG;

    case ESP_FAIL:
        ESP_LOGE(TAG, "Sending Soft Reset command error, the HTU21D slave hasn't ACK the transfer.");
        return HTU21D_ERR_FAIL;

    case ESP_ERR_INVALID_STATE:
        ESP_LOGE(TAG, "Soft reset failed,  I2C driver not installed or not in master mode.");
        return HTU21D_ERR_INVALID_STATE;

    case ESP_ERR_TIMEOUT:
        ESP_LOGE(TAG, "Soft reset failed,  operation timeout because the I2C bus is busy.");
        return HTU21D_ERR_TIMEOUT;
    }

    vTaskDelay(pdMS_TO_TICKS(HTU21_RESET_TIME));

    ESP_LOGI(TAG, "HTU21D sensor soft reset was successful.");

    return HTU21D_ERR_OK;
}

uint8_t htu21d_read_user_register()
{
    esp_err_t ret;

    // send the command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return 0;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(
                                      cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, READ_USER_REG, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return 0;
    }

    // receive the answer
    uint8_t reg_value;
    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return 0;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(
                                      cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &reg_value, 0x01));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return 0;
    }

    return reg_value;
}

int htu21d_write_user_register(uint8_t value)
{
    esp_err_t ret;

    // send the command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return HTU21D_ERR_FAIL;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, WRITE_USER_REG, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, value, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);

    switch (ret) {

    case ESP_ERR_INVALID_ARG:
        return HTU21D_ERR_INVALID_ARG;

    case ESP_FAIL:
        return HTU21D_ERR_FAIL;

    case ESP_ERR_INVALID_STATE:
        return HTU21D_ERR_INVALID_STATE;

    case ESP_ERR_TIMEOUT:
        return HTU21D_ERR_TIMEOUT;
    }
    return HTU21D_ERR_OK;
}

uint16_t read_value(uint8_t command)
{
    esp_err_t ret;

    // send the command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return 0;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, command, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return 0;
    }

    // wait for the sensor (50ms)
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // receive the answer
    uint8_t msb, lsb, crc;
    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        ESP_LOGE(TAG, "Not enough dynamic memory");
        return 0;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &msb, 0x00));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &lsb, 0x00));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &crc, 0x01));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK_WITHOUT_ABORT(ret);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return 0;
    }

    uint16_t raw_value = ((uint16_t) msb << 8) | (uint16_t) lsb;
    if (!is_crc_valid(raw_value, crc)) {
        ESP_LOGE(TAG, "CRC is invalid.");
    }
    return raw_value & 0xFFFC;
}

// verify the CRC, algorithm in the datasheet (see comments below)
bool is_crc_valid(uint16_t value, uint8_t crc)
{
    // line the bits representing the input in a row (first data, then crc)
    uint32_t row = (uint32_t)value << 8;
    row |= crc;

    // polynomial = x^8 + x^5 + x^4 + 1
    // padded with zeroes corresponding to the bit length of the CRC
    uint32_t divisor = (uint32_t)0x988000;

    for (int i = 0 ; i < 16 ; i++) {

        // if the input bit above the leftmost divisor bit is 1,
        // the divisor is XORed into the input
        if (row & (uint32_t)1 << (23 - i)) {
            row ^= divisor;
        }

        // the divisor is then shifted one bit to the right
        divisor >>= 1;
    }

    // the remainder should equal zero if there are no detectable errors
    return (row == 0);
}

/**
 * @brief Converts Celsius to Fahrenheit.
 * @param celsius_degrees The temperature in degrees Celsius.
 * @return Returns a `float` in degrees Fahrenheit.
 */
float celsius_to_fahrenheit(float celsius_degrees)
{
    return (celsius_degrees * 9.0F / 5.0F) + 32.0F;
}

/**
 * @brief Computes the temperature compensated humidity.
 *
 * For temperatures other than 25°C, this function, which applies a temperature
 * coefficient compensation equation, can be used and will guarantee Relative
 * Humidity accuracy within ±2%, from 0°C to 80°C.
 *
 * @param[in] temperature Actual temperature measured (degC).
 * @param[in] relative_humidity Actual relative humidity measured (%RH).
 *
 * @return Returns the temperature compensated humidity (%RH).
 */
float htu21_compute_compensated_humidity(float temperature,
                                         float relative_humidity)
{
    return (relative_humidity +
            (25.0F - temperature) * HTU21_TEMPERATURE_COEFFICIENT);
}
