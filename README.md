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
