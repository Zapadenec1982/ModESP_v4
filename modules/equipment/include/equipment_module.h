/**
 * @file equipment_module.h
 * @brief Equipment Manager — single owner of all HAL drivers
 *
 * Architecture:
 *   EM is the ONLY module that accesses ISensorDriver / IActuatorDriver.
 *   Business modules (Thermostat, Defrost, Protection) communicate
 *   through SharedState only:
 *     - EM publishes sensor readings: equipment.air_temp, equipment.sensor1_ok
 *     - EM publishes actuator states: equipment.compressor, equipment.heater
 *     - Business modules publish requests: thermostat.req.compressor, defrost.req.heater
 *     - EM reads requests, applies arbitration, drives outputs
 *
 * Arbitration priority: Protection LOCKOUT > Defrost active > Thermostat
 *
 * Interlocks (hardcoded, cannot be bypassed):
 *   - Heater and compressor NEVER simultaneously
 *   - Heater and hot gas valve NEVER simultaneously
 *   - Protection lockout = everything OFF
 *
 * SharedState keys published:
 *   equipment.air_temp      — float (°C), main chamber sensor
 *   equipment.evap_temp     — float (°C), evaporator sensor (optional)
 *   equipment.sensor1_ok    — bool, air sensor healthy
 *   equipment.sensor2_ok    — bool, evap sensor healthy
 *   equipment.compressor    — bool, actual relay state
 *   equipment.heater        — bool, actual relay state
 *   equipment.evap_fan      — bool, actual relay state
 *   equipment.cond_fan      — bool, actual relay state
 *   equipment.hg_valve      — bool, actual relay state
 *
 * SharedState keys read (requests from other modules):
 *   thermostat.req.compressor   — bool
 *   thermostat.req.evap_fan     — bool
 *   thermostat.req.cond_fan     — bool
 *   defrost.active              — bool
 *   defrost.req.compressor      — bool
 *   defrost.req.heater          — bool
 *   defrost.req.evap_fan        — bool
 *   defrost.req.cond_fan        — bool
 *   defrost.req.hg_valve        — bool
 *   protection.lockout          — bool
 */

#pragma once

#include "modesp/base_module.h"
#include "modesp/hal/driver_interfaces.h"

namespace modesp {
class DriverManager;
}

class EquipmentModule : public modesp::BaseModule {
public:
    EquipmentModule();

    /// Єдиний модуль з bind_drivers — володіє всіма drivers
    void bind_drivers(modesp::DriverManager& dm);

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_message(const etl::imessage& msg) override;
    void on_stop() override;

private:
    // === Сенсори ===
    modesp::ISensorDriver* sensor_air_  = nullptr;  // Обов'язковий
    modesp::ISensorDriver* sensor_evap_ = nullptr;  // Опціональний

    // === Актуатори ===
    modesp::IActuatorDriver* compressor_ = nullptr;  // Обов'язковий
    modesp::IActuatorDriver* heater_     = nullptr;  // Опціональний
    modesp::IActuatorDriver* evap_fan_   = nullptr;  // Опціональний
    modesp::IActuatorDriver* cond_fan_   = nullptr;  // Опціональний
    modesp::IActuatorDriver* hg_valve_   = nullptr;  // Опціональний

    // === Дискретні входи ===
    modesp::ISensorDriver* night_sensor_ = nullptr;  // Опціональний

    // === Внутрішня логіка ===
    void read_sensors();
    void read_requests();
    void apply_arbitration();
    void apply_outputs();
    void publish_state();

    // Кешовані значення сенсорів
    float air_temp_  = 0.0f;
    float evap_temp_ = 0.0f;

    // Requests від бізнес-модулів (читаються кожен цикл з SharedState)
    struct Requests {
        // Thermostat
        bool therm_compressor = false;
        bool therm_evap_fan   = false;
        bool therm_cond_fan   = false;

        // Defrost
        bool defrost_active   = false;
        bool def_compressor   = false;
        bool def_heater       = false;
        bool def_evap_fan     = false;
        bool def_cond_fan     = false;
        bool def_hg_valve     = false;

        // Protection
        bool protection_lockout = false;
    } req_;

    // Фінальний вихід (після арбітражу)
    struct Outputs {
        bool compressor = false;
        bool heater     = false;
        bool evap_fan   = false;
        bool cond_fan   = false;
        bool hg_valve   = false;
    } out_;

    // AUDIT-003: Compressor anti-short-cycle на рівні виходу (output-level).
    // Захищає компресор незалежно від джерела запиту (thermostat/defrost).
    // Доповнює, а не замінює таймери thermostat (ті працюють для state machine логіки).
    bool  comp_actual_        = false;   // Фактичний стан компресора
    uint32_t comp_since_ms_   = 999999;  // Час з останнього перемикання (мс)
    static constexpr uint32_t COMP_MIN_OFF_MS = 180000;  // 3 хв min OFF
    static constexpr uint32_t COMP_MIN_ON_MS  = 120000;  // 2 хв min ON
};
