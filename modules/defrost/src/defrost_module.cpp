/**
 * @file defrost_module.cpp
 * @brief Defrost cycle controller — 3 types, 7-phase state machine (spec_v3 §3)
 *
 * Phase sequence:
 *   dFT=0 (natural):  IDLE → ACTIVE → DRIP → FAD → IDLE
 *   dFT=1 (heater):   IDLE → ACTIVE → DRIP → FAD → IDLE
 *   dFT=2 (hot gas):  IDLE → STABILIZE → VALVE_OPEN → ACTIVE → EQUALIZE → DRIP → FAD → IDLE
 *
 * Relay table per phase (managed via Equipment Manager requests):
 *   STABILIZE:  comp=0, heat=0, evap=0, cond=0, valve=0
 *   VALVE_OPEN: comp=0, heat=0, evap=0, cond=0, valve=1
 *   ACTIVE(0):  comp=0, heat=0, evap=0, cond=0, valve=0  (natural)
 *   ACTIVE(1):  comp=0, heat=1, evap=0, cond=0, valve=0  (heater)
 *   ACTIVE(2):  comp=1, heat=0, evap=0, cond=0, valve=1  (hot gas)
 *   EQUALIZE:   comp=0, heat=0, evap=0, cond=0, valve=0
 *   DRIP:       comp=0, heat=0, evap=0, cond=0, valve=0
 *   FAD:        comp=1, heat=0, evap=0, cond=1, valve=0
 */

#include "defrost_module.h"
#include "esp_log.h"

static const char* TAG = "Defrost";

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

DefrostModule::DefrostModule()
    : BaseModule("defrost", modesp::ModulePriority::NORMAL)
{}

// ═══════════════════════════════════════════════════════════════
// Helpers: читання з SharedState
// ═══════════════════════════════════════════════════════════════

float DefrostModule::read_float(const char* key, float def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* fp = etl::get_if<float>(&v.value());
    return fp ? *fp : def;
}

bool DefrostModule::read_bool(const char* key, bool def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* bp = etl::get_if<bool>(&v.value());
    return bp ? *bp : def;
}

int32_t DefrostModule::read_int(const char* key, int32_t def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* ip = etl::get_if<int32_t>(&v.value());
    return ip ? *ip : def;
}

// ═══════════════════════════════════════════════════════════════
// Sync settings з SharedState (WebUI/API може їх змінити)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::sync_settings() {
    defrost_type_  = read_int("defrost.type", defrost_type_);
    counter_mode_  = read_int("defrost.counter_mode", counter_mode_);
    initiation_    = read_int("defrost.initiation", initiation_);
    end_temp_      = read_float("defrost.end_temp", end_temp_);
    demand_temp_   = read_float("defrost.demand_temp", demand_temp_);
    fad_temp_      = read_float("defrost.fad_temp", fad_temp_);

    // Конвертуємо одиниці в мілісекунди
    interval_ms_     = static_cast<uint32_t>(read_int("defrost.interval", 8)) * 3600000u;
    max_duration_ms_ = static_cast<uint32_t>(read_int("defrost.max_duration", 30)) * 60000u;
    drip_time_ms_    = static_cast<uint32_t>(read_int("defrost.drip_time", 120)) * 1000u;
    fan_delay_ms_    = static_cast<uint32_t>(read_int("defrost.fan_delay", 120)) * 1000u;
    stabilize_ms_    = static_cast<uint32_t>(read_int("defrost.stabilize_time", 30)) * 1000u;
    valve_delay_ms_  = static_cast<uint32_t>(read_int("defrost.valve_delay", 3)) * 1000u;
    equalize_ms_     = static_cast<uint32_t>(read_int("defrost.equalize_time", 90)) * 1000u;

    // Відновлення persist лічильника при першому init
    if (!settings_loaded_) {
        settings_loaded_ = true;
        int32_t saved_timer = read_int("defrost.interval_timer", 0);
        interval_timer_ms_ = static_cast<uint32_t>(saved_timer) * 1000u;

        defrost_count_       = read_int("defrost.defrost_count", 0);
        consecutive_timeouts_ = read_int("defrost.consecutive_timeouts", 0);

        ESP_LOGI(TAG, "Restored: interval_timer=%ld s, count=%ld",
                 saved_timer, defrost_count_);
    }
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle: on_init
// ═══════════════════════════════════════════════════════════════

bool DefrostModule::on_init() {
    // PersistService вже відновив збережені значення з NVS → SharedState (Phase 1)
    sync_settings();

    // Початковий стан — все скинуто
    state_set("defrost.active", false);
    state_set("defrost.phase", "idle");
    state_set("defrost.state", "Очікування");
    state_set("defrost.phase_timer", static_cast<int32_t>(0));
    state_set("defrost.last_termination", "none");
    state_set("defrost.manual_start", false);
    clear_requests();

    // Публікуємо відновлені persist значення
    state_set("defrost.defrost_count", defrost_count_);
    state_set("defrost.consecutive_timeouts", consecutive_timeouts_);
    state_set("defrost.interval_timer",
              static_cast<int32_t>(interval_timer_ms_ / 1000));

    ESP_LOGI(TAG, "Initialized (type=%ld, interval=%lu h, initiation=%ld, timer=%lu s)",
             defrost_type_,
             interval_ms_ / 3600000u,
             initiation_,
             interval_timer_ms_ / 1000);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle: on_update — головний цикл
// ═══════════════════════════════════════════════════════════════

void DefrostModule::on_update(uint32_t dt_ms) {
    sync_settings();

    // Читаємо inputs з SharedState
    evap_temp_     = read_float("equipment.evap_temp");
    sensor2_ok_    = read_bool("equipment.sensor2_ok");
    compressor_on_ = read_bool("equipment.compressor");
    bool lockout   = read_bool("protection.lockout");

    // Protection lockout → аварійно переривати відтайку
    if (lockout && phase_ != Phase::IDLE) {
        ESP_LOGW(TAG, "Protection lockout — aborting defrost");
        finish_defrost();
        return;
    }

    // Диспетчер фаз
    switch (phase_) {
        case Phase::IDLE:
            update_idle(dt_ms);
            break;
        case Phase::STABILIZE:
            update_stabilize(dt_ms);
            break;
        case Phase::VALVE_OPEN:
            update_valve_open(dt_ms);
            break;
        case Phase::ACTIVE:
            update_active_phase(dt_ms);
            break;
        case Phase::EQUALIZE:
            update_equalize(dt_ms);
            break;
        case Phase::DRIP:
            update_drip(dt_ms);
            break;
        case Phase::FAD:
            update_fad(dt_ms);
            break;
    }

    publish_state();
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle: on_stop
// ═══════════════════════════════════════════════════════════════

void DefrostModule::on_stop() {
    state_set("defrost.active", false);
    state_set("defrost.phase", "idle");
    state_set("defrost.state", "Зупинено");
    clear_requests();
    ESP_LOGI(TAG, "Defrost stopped");
}

// ═══════════════════════════════════════════════════════════════
// IDLE — ініціація відтайки
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_idle(uint32_t dt_ms) {
    // Ручний запуск (пріоритет)
    if (read_bool("defrost.manual_start")) {
        state_set("defrost.manual_start", false);
        start_defrost("manual");
        return;
    }

    // Ініціація вимкнена
    if (initiation_ == 3) return;

    // Оновлюємо лічильник (залежно від режиму)
    if (counter_mode_ == 1) {
        // Реальний час — завжди тікає
        interval_timer_ms_ += dt_ms;
    } else {
        // counter_mode_ == 2: час роботи компресора
        if (compressor_on_) {
            interval_timer_ms_ += dt_ms;
        }
    }

    // Перевіряємо тригери ініціації
    bool timer_trigger  = (initiation_ == 0 || initiation_ == 2) &&
                           check_timer_trigger();
    bool demand_trigger = (initiation_ == 1 || initiation_ == 2) &&
                           check_demand_trigger();

    if (timer_trigger || demand_trigger) {
        // Оптимізація: якщо T_evap вище dSt → випарник чистий → пропускаємо
        if (sensor2_ok_ && evap_temp_ > end_temp_) {
            ESP_LOGI(TAG, "Defrost skipped — evap clean (%.1f > %.1f)",
                     evap_temp_, end_temp_);
            interval_timer_ms_ = 0;  // Скидаємо лічильник
            return;
        }
        start_defrost(timer_trigger ? "timer" : "demand");
    }
}

// ═══════════════════════════════════════════════════════════════
// Перевірка тригерів
// ═══════════════════════════════════════════════════════════════

bool DefrostModule::check_timer_trigger() const {
    return interval_timer_ms_ >= interval_ms_;
}

bool DefrostModule::check_demand_trigger() const {
    // Demand тільки якщо датчик випарника справний
    if (!sensor2_ok_) return false;
    return evap_temp_ < demand_temp_;
}

// ═══════════════════════════════════════════════════════════════
// Початок циклу відтайки
// ═══════════════════════════════════════════════════════════════

void DefrostModule::start_defrost(const char* reason) {
    ESP_LOGI(TAG, "Starting defrost (%s), type=%ld", reason, defrost_type_);
    interval_timer_ms_ = 0;  // Скидаємо лічильник інтервалу

    if (defrost_type_ == 2) {
        // Гарячий газ → починаємо зі стабілізації тиску (фаза 1)
        enter_phase(Phase::STABILIZE);
    } else {
        // Природна (0) або тен (1) → одразу активна відтайка (фаза 3)
        enter_phase(Phase::ACTIVE);
    }
}

// ═══════════════════════════════════════════════════════════════
// Перехід між фазами
// ═══════════════════════════════════════════════════════════════

void DefrostModule::enter_phase(Phase p) {
    phase_ = p;
    phase_timer_ms_ = 0;

    // Встановлюємо defrost.active для всіх фаз крім IDLE
    state_set("defrost.active", p != Phase::IDLE);
    state_set("defrost.phase", phase_name());

    // Встановлюємо запити до Equipment Manager відповідно до фази
    switch (p) {
        case Phase::IDLE:
            clear_requests();
            break;

        case Phase::STABILIZE:
            // dFT=2, фаза 1: все вимкнено, тиск вирівнюється
            set_requests(false, false, false, false, false);
            break;

        case Phase::VALVE_OPEN:
            // dFT=2, фаза 2: тільки клапан ГГ відкривається
            set_requests(false, false, false, false, true);
            break;

        case Phase::ACTIVE:
            if (defrost_type_ == 0) {
                // Природна: все вимкнено (компресор і тен OFF)
                set_requests(false, false, false, false, false);
            } else if (defrost_type_ == 1) {
                // Тен: тен ON, компресор OFF (EM гарантує інтерлок)
                set_requests(false, true, false, false, false);
            } else {
                // Гарячий газ: компресор ON + клапан ГГ ON
                set_requests(true, false, false, false, true);
            }
            break;

        case Phase::EQUALIZE:
            // dFT=2, фаза 4: КРИТИЧНА пауза — все вимкнено
            // Газ конденсується, тиск падає перед зупинкою
            set_requests(false, false, false, false, false);
            break;

        case Phase::DRIP:
            // Фаза 5: все вимкнено, крапельна вода стікає
            set_requests(false, false, false, false, false);
            break;

        case Phase::FAD:
            // Фаза 6: компресор ON, вент. конд. ON, вент. вип. OFF
            // Охолоджує випарник — залишки вологи примерзають до ламелей
            set_requests(true, false, false, true, false);
            break;
    }

    ESP_LOGI(TAG, "Phase → %s", phase_name());
}

// ═══════════════════════════════════════════════════════════════
// Назви фаз
// ═══════════════════════════════════════════════════════════════

const char* DefrostModule::phase_name() const {
    switch (phase_) {
        case Phase::IDLE:       return "idle";
        case Phase::STABILIZE:  return "stabilize";
        case Phase::VALVE_OPEN: return "valve_open";
        case Phase::ACTIVE:     return "active";
        case Phase::EQUALIZE:   return "equalize";
        case Phase::DRIP:       return "drip";
        case Phase::FAD:        return "fad";
    }
    return "idle";
}

const char* DefrostModule::phase_state_label() const {
    switch (phase_) {
        case Phase::IDLE:       return "Очікування";
        case Phase::STABILIZE:  return "Стабілізація";
        case Phase::VALVE_OPEN: return "Відкриття клапана";
        case Phase::ACTIVE:     return "Активна відтайка";
        case Phase::EQUALIZE:   return "Вирівнювання тиску";
        case Phase::DRIP:       return "Дренаж";
        case Phase::FAD:        return "Охолодження після відтайки";
    }
    return "Очікування";
}

// ═══════════════════════════════════════════════════════════════
// dFT=2: Фаза 1 — STABILIZE (вирівнювання тиску)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_stabilize(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    if (phase_timer_ms_ >= stabilize_ms_) {
        ESP_LOGI(TAG, "Stabilize complete (%lu ms)", stabilize_ms_);
        enter_phase(Phase::VALVE_OPEN);
    }
}

// ═══════════════════════════════════════════════════════════════
// dFT=2: Фаза 2 — VALVE_OPEN (клапан відкривається)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_valve_open(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    if (phase_timer_ms_ >= valve_delay_ms_) {
        ESP_LOGI(TAG, "Valve open complete (%lu ms)", valve_delay_ms_);
        enter_phase(Phase::ACTIVE);
    }
}

// ═══════════════════════════════════════════════════════════════
// Фаза 3 — ACTIVE (активна відтайка, для всіх dFT)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_active_phase(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    // Завершення по T_evap (основне, якщо датчик справний)
    if (sensor2_ok_ && evap_temp_ >= end_temp_) {
        ESP_LOGI(TAG, "Defrost end by temp: T_evap=%.1f >= dSt=%.1f",
                 evap_temp_, end_temp_);
        consecutive_timeouts_ = 0;
        state_set("defrost.last_termination", "temp");
        finish_active_phase("temp");
        return;
    }

    // Завершення по таймеру безпеки (dEt)
    if (phase_timer_ms_ >= max_duration_ms_) {
        consecutive_timeouts_++;
        ESP_LOGW(TAG, "Defrost safety timeout (%lu min), consecutive=%ld",
                 max_duration_ms_ / 60000, consecutive_timeouts_);
        if (consecutive_timeouts_ >= 3) {
            // 3 підряд по таймеру — можлива несправність тена або датчика
            ESP_LOGW(TAG, "3 consecutive timeouts — possible heater/sensor failure!");
        }
        state_set("defrost.last_termination", "timeout");
        state_set("defrost.consecutive_timeouts", consecutive_timeouts_);
        finish_active_phase("timeout");
        return;
    }
}

// ═══════════════════════════════════════════════════════════════
// dFT=2: Фаза 4 — EQUALIZE (вирівнювання після ГГ)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_equalize(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    if (phase_timer_ms_ >= equalize_ms_) {
        ESP_LOGI(TAG, "Equalize complete (%lu s)", equalize_ms_ / 1000);
        enter_phase(Phase::DRIP);
    }
}

// ═══════════════════════════════════════════════════════════════
// Фаза 5 — DRIP (дренаж)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_drip(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    if (phase_timer_ms_ >= drip_time_ms_) {
        ESP_LOGI(TAG, "Drip complete (%lu s)", drip_time_ms_ / 1000);
        enter_phase(Phase::FAD);
    }
}

// ═══════════════════════════════════════════════════════════════
// Фаза 6 — FAD (Fan After Defrost)
// ═══════════════════════════════════════════════════════════════

void DefrostModule::update_fad(uint32_t dt_ms) {
    phase_timer_ms_ += dt_ms;

    // Завершення по T_evap < FAT (якщо датчик справний)
    if (sensor2_ok_ && evap_temp_ < fad_temp_) {
        ESP_LOGI(TAG, "FAD complete by temp: T_evap=%.1f < FAT=%.1f",
                 evap_temp_, fad_temp_);
        finish_defrost();
        return;
    }

    // Завершення по таймеру FAd
    if (phase_timer_ms_ >= fan_delay_ms_) {
        ESP_LOGI(TAG, "FAD complete by timer (%lu s)", fan_delay_ms_ / 1000);
        finish_defrost();
        return;
    }
}

// ═══════════════════════════════════════════════════════════════
// Завершення активної фази → EQUALIZE або DRIP
// ═══════════════════════════════════════════════════════════════

void DefrostModule::finish_active_phase(const char* reason) {
    defrost_count_++;
    state_set("defrost.defrost_count", defrost_count_);
    ESP_LOGI(TAG, "Active defrost finished (%s), total count=%ld",
             reason, defrost_count_);

    if (defrost_type_ == 2) {
        // Гарячий газ → обов'язкове вирівнювання тиску (фаза 4)
        enter_phase(Phase::EQUALIZE);
    } else {
        // Природна/тен → одразу дренаж (фаза 5)
        enter_phase(Phase::DRIP);
    }
}

// ═══════════════════════════════════════════════════════════════
// Повне завершення відтайки — повернення в IDLE
// ═══════════════════════════════════════════════════════════════

void DefrostModule::finish_defrost() {
    ESP_LOGI(TAG, "Defrost cycle complete — returning to IDLE");
    enter_phase(Phase::IDLE);
}

// ═══════════════════════════════════════════════════════════════
// Requests до Equipment Manager
// ═══════════════════════════════════════════════════════════════

void DefrostModule::set_requests(bool comp, bool heater,
                                  bool evap_fan, bool cond_fan, bool hg_valve) {
    state_set("defrost.req.compressor", comp);
    state_set("defrost.req.heater",     heater);
    state_set("defrost.req.evap_fan",   evap_fan);
    state_set("defrost.req.cond_fan",   cond_fan);
    state_set("defrost.req.hg_valve",   hg_valve);
}

void DefrostModule::clear_requests() {
    set_requests(false, false, false, false, false);
}

// ═══════════════════════════════════════════════════════════════
// Публікація стану в SharedState
// ═══════════════════════════════════════════════════════════════

void DefrostModule::publish_state() {
    // Час в поточній фазі (в секундах для UI)
    state_set("defrost.phase_timer",
              static_cast<int32_t>(phase_timer_ms_ / 1000));

    // Лічильник до наступної відтайки (в секундах)
    // PersistService збереже з debounce 5s
    state_set("defrost.interval_timer",
              static_cast<int32_t>(interval_timer_ms_ / 1000));

    // Людино-читабельний стан
    state_set("defrost.state", phase_state_label());
}
