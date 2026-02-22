/**
 * @file hal.cpp
 * @brief HAL implementation — GPIO init and resource management
 *
 * Initializes GPIO outputs as push-pull (safe OFF state),
 * OneWire bus configs, GPIO inputs, and ADC channels.
 */

#include "modesp/hal/hal.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

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

    if (!init_gpio_inputs(config)) {
        ESP_LOGE(TAG, "GPIO input init failed");
        return false;
    }

    if (!init_adc(config)) {
        ESP_LOGE(TAG, "ADC init failed");
        return false;
    }

    ESP_LOGI(TAG, "HAL ready: %d gpio_out, %d ow, %d gpio_in, %d adc",
             (int)gpio_output_count_, (int)onewire_count_,
             (int)gpio_input_count_, (int)adc_count_);
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

bool HAL::init_gpio_inputs(const BoardConfig& config) {
    gpio_input_count_ = 0;

    for (const auto& cfg : config.gpio_inputs) {
        if (gpio_input_count_ >= MAX_ADC_CHANNELS) {
            ESP_LOGW(TAG, "GPIO input limit reached (%d)", (int)MAX_ADC_CHANNELS);
            break;
        }

        // GPIO 34-39 — input-only, без внутрішнього pull-up/pull-down
        bool is_input_only = (cfg.gpio >= GPIO_NUM_34 && cfg.gpio <= GPIO_NUM_39);

        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << cfg.gpio);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_DISABLE;

        if (is_input_only) {
            // Зовнішній pull-up/pull-down потрібен на PCB
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        } else {
            io_conf.pull_up_en = cfg.pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = cfg.pull_up ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
        }

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GPIO input %d config failed: %s", cfg.gpio, esp_err_to_name(err));
            return false;
        }

        if (is_input_only && cfg.pull_up) {
            ESP_LOGW(TAG, "  GPIO %d is input-only — external pull-up required", cfg.gpio);
        }

        auto& res = gpio_inputs_[gpio_input_count_];
        res.id = cfg.id;
        res.gpio = cfg.gpio;
        res.pull_up = cfg.pull_up;
        res.initialized = true;
        gpio_input_count_++;

        ESP_LOGI(TAG, "  GPIO input '%s' on GPIO %d (pull_%s)",
                 cfg.id.c_str(), cfg.gpio,
                 cfg.pull_up ? "UP" : "DOWN");
    }

    return true;
}

bool HAL::init_adc(const BoardConfig& config) {
    adc_count_ = 0;

    if (config.adc_channels.empty()) {
        return true;  // Немає ADC каналів — нормальна ситуація
    }

    for (const auto& cfg : config.adc_channels) {
        if (adc_count_ >= MAX_ADC_CHANNELS) {
            ESP_LOGW(TAG, "ADC channel limit reached (%d)", (int)MAX_ADC_CHANNELS);
            break;
        }

        // Зберігаємо конфіг — фактична ініціалізація ADC буде в драйвері NTC,
        // оскільки ESP-IDF ADC oneshot потребує handle який належить драйверу
        auto& res = adc_channels_[adc_count_];
        res.id = cfg.id;
        res.gpio = cfg.gpio;
        res.atten = cfg.atten;
        res.initialized = true;
        adc_count_++;

        ESP_LOGI(TAG, "  ADC '%s' on GPIO %d (atten=%d)",
                 cfg.id.c_str(), cfg.gpio, cfg.atten);
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

GpioInputResource* HAL::find_gpio_input(etl::string_view id) {
    for (size_t i = 0; i < gpio_input_count_; i++) {
        if (gpio_inputs_[i].id.size() == id.size() &&
            etl::string_view(gpio_inputs_[i].id.c_str(), gpio_inputs_[i].id.size()) == id) {
            return &gpio_inputs_[i];
        }
    }
    return nullptr;
}

AdcChannelResource* HAL::find_adc_channel(etl::string_view id) {
    for (size_t i = 0; i < adc_count_; i++) {
        if (adc_channels_[i].id.size() == id.size() &&
            etl::string_view(adc_channels_[i].id.c_str(), adc_channels_[i].id.size()) == id) {
            return &adc_channels_[i];
        }
    }
    return nullptr;
}

} // namespace modesp
