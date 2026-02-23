/**
 * @file protection_module.h
 * @brief Protection module — alarm monitoring and signaling (spec_v3 §2.7)
 *
 * Independent alarm monitors:
 *   1. High Temp (HAL) — delayed (dAd), blocked during defrost
 *   2. Low Temp (LAL)  — delayed (dAd), always active
 *   3. Sensor1 (ERR1)  — instant, air sensor failure
 *   4. Sensor2 (ERR2)  — instant, evap sensor failure (info only)
 *   5. Door open       — delayed (door_delay), info only
 *
 * Each monitor: NORMAL → PENDING (delay) → ALARM → NORMAL (auto-clear)
 *
 * Protection does NOT stop equipment in this phase.
 * protection.lockout is reserved (always false) for Phase 10+.
 *
 * Priority: HIGH(1) — runs BEFORE Thermostat(2), AFTER Equipment(0)
 *
 * SharedState keys read:
 *   equipment.air_temp      — float (°C)
 *   equipment.sensor1_ok    — bool
 *   equipment.sensor2_ok    — bool
 *   equipment.door_open     — bool
 *   defrost.active          — bool
 *
 * SharedState keys written:
 *   protection.lockout          — bool (always false)
 *   protection.alarm_active     — bool (any alarm active)
 *   protection.alarm_code       — string (highest priority alarm code)
 *   protection.high_temp_alarm  — bool
 *   protection.low_temp_alarm   — bool
 *   protection.sensor1_alarm    — bool
 *   protection.sensor2_alarm    — bool
 *   protection.door_alarm       — bool
 */

#pragma once

#include "modesp/base_module.h"

class ProtectionModule : public modesp::BaseModule {
public:
    ProtectionModule();

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_stop() override;

private:
    // Helpers
    void    sync_settings();

    // Структура для одного монітора аварій
    struct AlarmMonitor {
        bool active    = false;   // Аварія зараз
        bool pending   = false;   // В затримці
        uint32_t pending_ms = 0;  // Час в pending стані
    };

    AlarmMonitor high_temp_;
    AlarmMonitor low_temp_;
    AlarmMonitor sensor1_;   // ERR1 — instant (без затримки)
    AlarmMonitor sensor2_;   // ERR2 — instant
    AlarmMonitor door_;

    // Логіка моніторів
    void update_high_temp(float temp, bool sensor_ok, bool defrost_active, uint32_t dt_ms);
    void update_low_temp(float temp, bool sensor_ok, uint32_t dt_ms);
    void update_sensor_alarm(AlarmMonitor& m, bool sensor_ok, const char* label);
    void update_door_alarm(bool door_open, uint32_t dt_ms);
    void check_reset_command();
    void publish_alarms();

    // Налаштування (з SharedState, persist)
    float    high_limit_      = 12.0f;     // °C (HAL)
    float    low_limit_       = -35.0f;    // °C (LAL)
    uint32_t high_alarm_delay_ms_ = 1800000;  // 30 хв (dAd для HAL)
    uint32_t low_alarm_delay_ms_  = 1800000;  // 30 хв (dAd для LAL)
    uint32_t door_delay_ms_   = 300000;    // 5 хв
    bool     manual_reset_    = false;

    // Post-defrost suppression (HAL alarm)
    bool     was_defrost_active_       = false;
    bool     post_defrost_suppression_ = false;
    uint32_t post_defrost_timer_ms_    = 0;
    uint32_t post_defrost_delay_ms_    = 1800000;  // 30 хв default

    // Кешований код аварії
    const char* alarm_code_ = "none";
};
