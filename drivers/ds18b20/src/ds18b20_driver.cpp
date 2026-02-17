/**
 * @file ds18b20_driver.cpp
 * @brief DS18B20 driver implementation with OneWire bit-bang
 *
 * OneWire protocol implemented via GPIO bit-bang with critical
 * sections (portDISABLE_INTERRUPTS) for precise timing.
 *
 * Algorithm:
 *   1. Every read_interval_ms: send CONVERT_T command
 *   2. After 750ms: read scratchpad with CRC8
 *   3. Validate value (range + rate of change)
 *   4. Store latest valid reading for read() calls
 */

#include "ds18b20_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char* TAG = "DS18B20";

// ═══════════════════════════════════════════════════════════════
// DS18B20 ROM commands
// ═══════════════════════════════════════════════════════════════
static constexpr uint8_t CMD_SKIP_ROM      = 0xCC;
static constexpr uint8_t CMD_CONVERT_T     = 0x44;
static constexpr uint8_t CMD_READ_SCRATCH  = 0xBE;

// ═══════════════════════════════════════════════════════════════
// Configure (called by DriverManager before init)
// ═══════════════════════════════════════════════════════════════

void DS18B20Driver::configure(const char* role, gpio_num_t gpio, uint32_t read_interval_ms) {
    role_ = role;
    gpio_ = gpio;
    read_interval_ms_ = read_interval_ms;
    configured_ = true;
}

// ═══════════════════════════════════════════════════════════════
// ISensorDriver lifecycle
// ═══════════════════════════════════════════════════════════════

bool DS18B20Driver::init() {
    if (!configured_) {
        ESP_LOGE(TAG, "Driver not configured — call configure() first");
        return false;
    }

    // Configure GPIO as open-drain output (for OneWire)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << gpio_);
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO %d config failed: %s", gpio_, esp_err_to_name(err));
        return false;
    }
    gpio_set_level(gpio_, 1);  // Idle HIGH

    // Check for device presence on bus
    if (!onewire_reset()) {
        ESP_LOGW(TAG, "No device on GPIO %d — will retry in runtime", gpio_);
    } else {
        ESP_LOGI(TAG, "DS18B20 detected on GPIO %d", gpio_);
    }

    ESP_LOGI(TAG, "[%s] Initialized (GPIO=%d, interval=%lu ms)",
             role_.c_str(), gpio_, read_interval_ms_);
    return true;
}

void DS18B20Driver::update(uint32_t dt_ms) {
    uptime_ms_ += dt_ms;
    ms_since_read_ += dt_ms;

    // Phase 1: start conversion
    if (!conversion_started_ && ms_since_read_ >= read_interval_ms_) {
        if (start_conversion()) {
            conversion_started_ = true;
        } else {
            // Reset failed — sensor absent
            ms_since_read_ = 0;
            consecutive_errors_++;
            if (consecutive_errors_ >= MAX_CONSECUTIVE_ERRORS &&
                consecutive_errors_ % MAX_CONSECUTIVE_ERRORS == 0) {
                ESP_LOGE(TAG, "[%s] Sensor offline after %d errors",
                         role_.c_str(), consecutive_errors_);
            }
        }
        return;
    }

    // Phase 2: read scratchpad 750ms after conversion start
    if (conversion_started_ && ms_since_read_ >= read_interval_ms_ + 750) {
        conversion_started_ = false;
        ms_since_read_ = 0;

        float temp = 0.0f;
        if (retry([&]() { return read_temperature(temp); })) {
            if (validate_reading(temp)) {
                current_temp_ = temp;
                last_valid_temp_ = temp;
                last_valid_reading_ms_ = uptime_ms_;
                has_valid_reading_ = true;
                consecutive_errors_ = 0;

                ESP_LOGD(TAG, "[%s] %.2f°C", role_.c_str(), temp);
            } else {
                ESP_LOGW(TAG, "[%s] Validation failed: %.2f°C", role_.c_str(), temp);
                consecutive_errors_++;
            }
        } else {
            consecutive_errors_++;
            ESP_LOGW(TAG, "[%s] Read failed (errors=%d)", role_.c_str(), consecutive_errors_);
        }

        if (consecutive_errors_ == MAX_CONSECUTIVE_ERRORS) {
            ESP_LOGE(TAG, "[%s] %d consecutive errors", role_.c_str(), consecutive_errors_);
        }
    }
}

bool DS18B20Driver::read(float& value) {
    if (!has_valid_reading_) return false;
    value = current_temp_;
    return true;
}

bool DS18B20Driver::is_healthy() const {
    return consecutive_errors_ < MAX_CONSECUTIVE_ERRORS;
}

// ═══════════════════════════════════════════════════════════════
// OneWire low-level (bit-bang with critical sections)
//
// Таймінги оптимізовані для слабкого pull-up (4.7-5kΩ)
// та довгого кабелю (до 2м). Семпл зсунуто на ~15µs від
// початку тайм-слота для надійного зчитування.
// ═══════════════════════════════════════════════════════════════

bool DS18B20Driver::onewire_reset() {
    // Reset потребує 480µs LOW + 480µs recovery
    // Розбиваємо на дві фази щоб не тримати interrupts disabled 960µs
    gpio_set_level(gpio_, 0);
    ets_delay_us(480);

    portDISABLE_INTERRUPTS();
    gpio_set_level(gpio_, 1);
    ets_delay_us(70);
    int presence = gpio_get_level(gpio_);
    portENABLE_INTERRUPTS();

    // Дочекатись кінця presence pulse (загалом 480µs після release)
    ets_delay_us(410);

    return (presence == 0);
}

void DS18B20Driver::onewire_write_bit(uint8_t bit) {
    // Без власного critical section — викликається з write_byte під спільним
    if (bit & 1) {
        gpio_set_level(gpio_, 0);
        ets_delay_us(6);
        gpio_set_level(gpio_, 1);
        ets_delay_us(64);
    } else {
        gpio_set_level(gpio_, 0);
        ets_delay_us(60);
        gpio_set_level(gpio_, 1);
        ets_delay_us(10);
    }
}

uint8_t DS18B20Driver::onewire_read_bit() {
    // Без власного critical section — викликається з read_byte під спільним
    gpio_set_level(gpio_, 0);
    ets_delay_us(3);
    gpio_set_level(gpio_, 1);
    ets_delay_us(9);
    uint8_t bit = gpio_get_level(gpio_) ? 1 : 0;
    ets_delay_us(56);
    return bit;
}

void DS18B20Driver::onewire_write_byte(uint8_t data) {
    // Critical section на весь байт (~560µs)
    portDISABLE_INTERRUPTS();
    for (uint8_t i = 0; i < 8; i++) {
        onewire_write_bit(data & 0x01);
        data >>= 1;
    }
    portENABLE_INTERRUPTS();
}

uint8_t DS18B20Driver::onewire_read_byte() {
    // Critical section на весь байт (~560µs)
    uint8_t data = 0;
    portDISABLE_INTERRUPTS();
    for (uint8_t i = 0; i < 8; i++) {
        data |= (onewire_read_bit() << i);
    }
    portENABLE_INTERRUPTS();
    return data;
}

// ═══════════════════════════════════════════════════════════════
// DS18B20 operations
// ═══════════════════════════════════════════════════════════════

bool DS18B20Driver::start_conversion() {
    // OneWire тайміни захищені portDISABLE_INTERRUPTS в write_byte/read_byte.
    // WiFi PS маніпуляція тут не потрібна — вона спричиняла безкінечний цикл
    // "Set ps type: 2 → 1" в логах при кожному зчитуванні.
    if (!onewire_reset()) {
        return false;
    }
    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_CONVERT_T);
    return true;
}

bool DS18B20Driver::read_scratchpad(uint8_t* buf, size_t len) {
    // OneWire тайміни захищені portDISABLE_INTERRUPTS в write_byte/read_byte.
    // Цього достатньо для коректних зчитувань навіть з WiFi активним.
    if (!onewire_reset()) {
        return false;
    }
    onewire_write_byte(CMD_SKIP_ROM);
    onewire_write_byte(CMD_READ_SCRATCH);

    for (size_t i = 0; i < len; i++) {
        buf[i] = onewire_read_byte();
    }
    return true;
}

bool DS18B20Driver::read_temperature(float& temp_out) {
    uint8_t scratchpad[9];

    if (!read_scratchpad(scratchpad, 9)) {
        return false;
    }

    // CRC8 check (bytes 0-7, CRC in byte 8)
    uint8_t calc_crc = crc8(scratchpad, 8);
    if (calc_crc != scratchpad[8]) {
        ESP_LOGW(TAG, "[%s] CRC8 mismatch: got=0x%02X calc=0x%02X raw=[%02X %02X %02X %02X %02X %02X %02X %02X %02X]",
                 role_.c_str(), scratchpad[8], calc_crc,
                 scratchpad[0], scratchpad[1], scratchpad[2], scratchpad[3],
                 scratchpad[4], scratchpad[5], scratchpad[6], scratchpad[7], scratchpad[8]);
        return false;
    }

    // Check for default 85C value (conversion not complete)
    int16_t raw = (static_cast<int16_t>(scratchpad[1]) << 8) | scratchpad[0];
    float temp = raw / 16.0f;

    if (raw == 0x0550) {
        ESP_LOGD(TAG, "[%s] Power-on reset value (85C), skipping", role_.c_str());
        return false;
    }

    temp_out = temp;
    return true;
}

// ═══════════════════════════════════════════════════════════════
// CRC8 (Dallas/Maxim polynomial: x^8 + x^5 + x^4 + 1)
// ═══════════════════════════════════════════════════════════════

uint8_t DS18B20Driver::crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

// ═══════════════════════════════════════════════════════════════
// Retry pattern
// ═══════════════════════════════════════════════════════════════

template<typename F>
bool DS18B20Driver::retry(F operation, uint8_t max_attempts, uint32_t delay_ms) {
    for (uint8_t i = 0; i < max_attempts; i++) {
        if (operation()) return true;
        if (i < max_attempts - 1) {
            // Пауза + bus recovery reset перед наступною спробою
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            onewire_reset();
            ets_delay_us(100);
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
// Validation
// ═══════════════════════════════════════════════════════════════

bool DS18B20Driver::validate_reading(float value) {
    if (value < MIN_VALID_TEMP || value > MAX_VALID_TEMP) {
        ESP_LOGW(TAG, "[%s] Out of range: %.2f°C", role_.c_str(), value);
        return false;
    }

    if (has_valid_reading_) {
        uint32_t elapsed_ms = uptime_ms_ - last_valid_reading_ms_;
        if (elapsed_ms > 0) {
            float dt_sec = elapsed_ms / 1000.0f;
            float rate = fabsf(value - last_valid_temp_) / dt_sec;
            if (rate > MAX_RATE_PER_SEC) {
                ESP_LOGW(TAG, "[%s] Rate too high: %.2f°C/s (dt=%lu ms)",
                         role_.c_str(), rate, elapsed_ms);
                return false;
            }
        }
    }

    return true;
}
