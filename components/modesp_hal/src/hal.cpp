/**
 * @file hal.cpp
 * @brief HAL implementation — GPIO init and resource management
 *
 * Initializes GPIO outputs as push-pull (safe OFF state)
 * and stores OneWire bus configs for later use by drivers.
 */

#include "modesp/hal/hal.h"
#include "esp_log.h"

static const char* TAG = "HAL";

namespace modesp {

bool HAL::init(const BoardConfig& config) {
    ESP_LOGI(TAG, "Initializing HAL for board '%s'", config.board_name.c_str());

    if (!init_gpio_outputs(config)) {
        ESP_LOGE(TAG, "GPIO output init failed");
        return false;
    }

    if (!init_onewire(config)) {
        ESP_LOGE(TAG, "OneWire init failed");
        return false;
    }

    ESP_LOGI(TAG, "HAL ready: %d gpio_out, %d ow",
             (int)gpio_output_count_, (int)onewire_count_);
    return true;
}

bool HAL::init_gpio_outputs(const BoardConfig& config) {
    gpio_output_count_ = 0;

    for (const auto& cfg : config.gpio_outputs) {
        if (gpio_output_count_ >= MAX_RELAYS) {
            ESP_LOGW(TAG, "GPIO output limit reached (%d)", (int)MAX_RELAYS);
            break;
        }

        // Configure GPIO as push-pull output
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << cfg.gpio);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GPIO %d config failed: %s", cfg.gpio, esp_err_to_name(err));
            return false;
        }

        // Safe state: output OFF
        int off_level = cfg.active_high ? 0 : 1;
        gpio_set_level(cfg.gpio, off_level);

        // Зберігаємо ресурс
        auto& res = gpio_outputs_[gpio_output_count_];
        res.id = cfg.id;
        res.gpio = cfg.gpio;
        res.active_high = cfg.active_high;
        res.initialized = true;
        gpio_output_count_++;

        ESP_LOGI(TAG, "  GPIO output '%s' on GPIO %d (active_%s)",
                 cfg.id.c_str(), cfg.gpio,
                 cfg.active_high ? "HIGH" : "LOW");
    }

    return true;
}

bool HAL::init_onewire(const BoardConfig& config) {
    onewire_count_ = 0;

    for (const auto& cfg : config.onewire_buses) {
        if (onewire_count_ >= MAX_ONEWIRE_BUSES) {
            ESP_LOGW(TAG, "OneWire bus limit reached (%d)", (int)MAX_ONEWIRE_BUSES);
            break;
        }

        // Зберігаємо конфіг — ініціалізація шини буде в драйвері
        auto& res = onewire_buses_[onewire_count_];
        res.id = cfg.id;
        res.gpio = cfg.gpio;
        res.initialized = true;
        onewire_count_++;

        ESP_LOGI(TAG, "  OneWire '%s' on GPIO %d", cfg.id.c_str(), cfg.gpio);
    }

    return true;
}

GpioOutputResource* HAL::find_gpio_output(etl::string_view id) {
    for (size_t i = 0; i < gpio_output_count_; i++) {
        if (gpio_outputs_[i].id.size() == id.size() &&
            etl::string_view(gpio_outputs_[i].id.c_str(), gpio_outputs_[i].id.size()) == id) {
            return &gpio_outputs_[i];
        }
    }
    return nullptr;
}

OneWireBusResource* HAL::find_onewire_bus(etl::string_view id) {
    for (size_t i = 0; i < onewire_count_; i++) {
        if (onewire_buses_[i].id.size() == id.size() &&
            etl::string_view(onewire_buses_[i].id.c_str(), onewire_buses_[i].id.size()) == id) {
            return &onewire_buses_[i];
        }
    }
    return nullptr;
}

} // namespace modesp
