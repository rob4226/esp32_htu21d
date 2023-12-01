# esp32_htu21d

An esp-idf component to interface with the HTU21D humidity/temperature sensor on
the I2C bus.

## How to Use

You can add it to your project's `main/idf_component.yml` to have ESP-IDF manage
it for you:

```yaml
dependencies:
  esp32_htu21d:
    git: https://github.com/rob4226/esp32_htu21d.git
    # Or using SSH:
    # git: git@github.com:rob4226/esp32_htu21d.git
```

Then you can include the header wherever you need it in your project:

```c
#include "htu21d.h"
```

### Example

```c
#define I2C_SDA_PIN 1
#define I2C_SCL_PIN 2

ESP_ERROR_CHECK(
    htu21d_init(I2C_NUM_0, I2C_SDA_PIN, I2C_SCL_PIN, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE));

while (1) {
  float temp = htu21d_read_temperature();
  float humidity = htu21d_read_humidity();

  printf("Temperature: %.02f°C  Humidity: %.02f%%\n", temp, humidity);

  vTaskDelay(pdMS_TO_TICKS(2000));
}
```

Also, see the example projects in the [examples](./examples) directory of this repo.

## HTU21D Sensor

The HTU21D sensor is a self-contained humidity and temperature sensor that is
fully calibrated during manufacturing. The sensor can operate from 1.5V to 3.6V,
has selectable resolution, low battery detection, and checksum capability. The
HTU21D has a low current stand-by mode for power-sensitive applications.

This sensor is able to measure the ambient temperature and relative humidity.

The HTU21D sensor requires a voltage supply between 1.5V and 3.6V.

### Measurement Resolutions

The default resolution is set to 12-bit relative humidity and 14-bit temperature
readings.

The following combinations of measurement resolutions can be selected:

| Bit 7 | Bit 0 | RH      | Temp    |
|-------|-------|---------|---------|
| 0     | 0     | 12 bits | 14 bits |
| 0     | 1     | 8 bits  | 12 bits |
| 1     | 0     | 10 bits | 13 bits |
| 1     | 1     | 11 bits | 11 bits |

## Development/Contributing

If you don't have the Python `pre-commit` package installed you can install it
with `pip install pre-commit`.

```shell
git clone https://github.com/rob4226/esp32_htu21d.git
# Setup hooks in newly cloned repo's .git folder for stuff like formatting code
# on commit, and checking for valid commit message format:
pre-commit install -t pre-commit -t commit-msg
```
