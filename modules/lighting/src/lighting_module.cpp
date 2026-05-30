/**
 * @file lighting_module.cpp
 * @brief Lighting control — base mode + door trigger / auto-off / manual override
 */

#include "lighting_module.h"
#include "esp_log.h"

#include <ctime>

static const char* TAG = "Lighting";

LightingModule::LightingModule()
    : BaseModule("lighting", modesp::ModulePriority::NORMAL)
{}

bool LightingModule::in_schedule(int hour, int start, int end) {
    if (start == end) return false;              // порожнє вікно
    if (start < end)  return hour >= start && hour < end;
    return hour >= start || hour < end;          // вікно через північ
}

int LightingModule::current_hour() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_hour;
}

bool LightingModule::compute_base() {
    switch (mode_) {
        case 1:  // ON
            return true;
        case 2:  // AUTO — день=ON, ніч=OFF
            return !read_bool("thermostat.night_active");
        case 3:  // SCHEDULE
            return in_schedule(current_hour(), sched_start_, sched_end_);
        case 0:  // OFF
        default:
            return false;
    }
}

bool LightingModule::on_init() {
    // PersistService вже відновив persisted-налаштування з NVS
    mode_         = read_int("lighting.mode", 0);
    door_trigger_ = read_bool("lighting.door_trigger");
    auto_off_min_ = read_int("lighting.auto_off_min", 0);
    sched_start_  = read_int("lighting.sched_start", 6);
    sched_end_    = read_int("lighting.sched_end", 22);

    // override не персистується — завжди стартує з "Авто"
    override_ = 0;
    state_set("lighting.override", static_cast<int32_t>(0));
    state_set("lighting.mode", mode_);
    state_set("lighting.state", false);
    state_set("lighting.req.light", false);

    door_linger_ms_ = 0;
    override_ms_    = 0;

    ESP_LOGI(TAG, "Initialized (mode=%ld, door_trigger=%d, auto_off=%ldmin)",
             (long)mode_, door_trigger_, (long)auto_off_min_);
    return true;
}

void LightingModule::on_update(uint32_t dt_ms) {
    // Sync settings (WebUI/MQTT можуть змінити)
    mode_         = read_int("lighting.mode", 0);
    door_trigger_ = read_bool("lighting.door_trigger");
    auto_off_min_ = read_int("lighting.auto_off_min", 0);
    sched_start_  = read_int("lighting.sched_start", 6);
    sched_end_    = read_int("lighting.sched_end", 22);
    override_     = read_int("lighting.override", 0);

    const uint32_t auto_off_ms = (auto_off_min_ > 0)
        ? static_cast<uint32_t>(auto_off_min_) * 60000u : 0u;

    const bool door_open = door_trigger_ && read_bool("equipment.door_open");

    // Door-linger: поки двері відкриті — тримаємо повний таймаут;
    // після закриття — зворотний відлік (щоб світло не гасло одразу).
    if (door_open) {
        door_linger_ms_ = auto_off_ms;           // оновлюється поки відкрито
    } else if (door_linger_ms_ > 0) {
        door_linger_ms_ = (door_linger_ms_ > dt_ms) ? door_linger_ms_ - dt_ms : 0;
    }

    // Manual override авто-повертається в "Авто" після таймауту
    // (лише якщо таймаут увімкнено; інакше тримається до зміни користувачем).
    if (override_ != 0) {
        if (auto_off_ms > 0) {
            override_ms_ += dt_ms;
            if (override_ms_ >= auto_off_ms) {
                override_    = 0;
                override_ms_ = 0;
                state_set("lighting.override", static_cast<int32_t>(0));
                ESP_LOGI(TAG, "Override expired → Авто");
            }
        }
    } else {
        override_ms_ = 0;
    }

    // Резолюція пріоритету: override > двері відкриті > door-linger > базовий режим
    bool request;
    if (override_ == 1)            request = true;
    else if (override_ == 2)       request = false;
    else if (door_open)           request = true;
    else if (door_linger_ms_ > 0) request = true;
    else                          request = compute_base();

    // Публікуємо тільки при зміні
    if (request != light_request_) {
        light_request_ = request;
        state_set("lighting.req.light", request);
        ESP_LOGI(TAG, "Light → %s (mode=%ld, ov=%ld, door=%d)",
                 request ? "ON" : "OFF", (long)mode_, (long)override_, door_open);
    }

    // Дзеркалимо фактичний стан від Equipment
    const bool actual = read_bool("equipment.light");
    if (actual != last_actual_) {
        last_actual_ = actual;
        state_set("lighting.state", actual);
    }
}

void LightingModule::on_stop() {
    state_set("lighting.req.light", false);
    ESP_LOGI(TAG, "Lighting stopped");
}
