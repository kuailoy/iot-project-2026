#include "DeviceReset.h"

#include <Arduino.h>
#include <WiFi.h>
#include <nvs_flash.h>

namespace DeviceReset {

void eraseAll() {
    Serial.println("Reset: erasing all settings");

    // Also forgets the network the Wi-Fi driver keeps on its own
    WiFi.disconnect(true, true);

    // Every namespace at once (Settings, habits, ...), then ready for use again
    esp_err_t result = nvs_flash_erase();
    if (result == ESP_OK) {
        result = nvs_flash_init();
    }
    if (result != ESP_OK) {
        Serial.printf("Reset: erasing failed (%d)\n", result);
    }
}

}
