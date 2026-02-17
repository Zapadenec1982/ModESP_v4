/**
 * @file thermostat_module.h
 * @brief Thermostat module with on/off hysteresis regulation
 *
 * Algorithm:
 *   temp > setpoint + hysteresis  ->  compressor ON  (cooling needed)
 *   temp < setpoint - hysteresis  ->  compressor OFF (target reached)
 *   between                       ->  no change      (dead zone)
 *
 * Compressor protection:
 *   After turning off, must wait MIN_OFF_TIME_MS before next start.
 *
 * Driver binding:
 *   Call bind_drivers(DriverManager&) after construction.
 *   The module reads temp via ISensorDriver::read() and
 *   controls the relay via IActuatorDriver::set().
 *
 * SharedState keys:
 *   thermostat.state      — "idle" | "cooling" | "safe_mode"
 *   thermostat.setpoint   — float (°C)
 *   thermostat.compressor — bool
 */

#pragma once

#include "modesp/base_module.h"
#include "modesp/hal/driver_interfaces.h"

namespace modesp {
class DriverManager;
}

class ThermostatModule : public modesp::BaseModule {
public:
    ThermostatModule();

    /// Bind sensor/actuator drivers from DriverManager.
    /// Must be called before init (after DriverManager::init).
    void bind_drivers(modesp::DriverManager& dm);

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_message(const etl::imessage& msg) override;
    void on_stop() override;

    // Regulation parameters
    float setpoint()   const { return setpoint_; }
    float hysteresis() const { return hysteresis_; }
    bool  is_cooling() const { return compressor_on_; }

private:
    // Regulation parameters
    float setpoint_    = -18.0f;  // °C (freezer default)
    float hysteresis_  = 1.5f;    // °C

    // State
    float    current_temp_    = 0.0f;
    bool     has_temp_        = false;
    bool     compressor_on_   = false;
    bool     safe_mode_       = false;
    uint32_t off_time_ms_     = 999999;  // Large value for first start
    uint32_t uptime_ms_       = 0;

    // Compressor protection
    static constexpr uint32_t MIN_OFF_TIME_MS = 180000;  // 3 minutes

    // Driver pointers (set by bind_drivers)
    modesp::ISensorDriver*   temp_sensor_ = nullptr;
    modesp::IActuatorDriver* compressor_  = nullptr;
};
