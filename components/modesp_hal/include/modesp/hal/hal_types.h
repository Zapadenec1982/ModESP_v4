/**
 * @file hal_types.h
 * @brief HAL system limits, config structs, and resource types
 *
 * All data structures used by the HAL layer:
 *   - BoardConfig:  parsed from board.json at boot
 *   - BindingTable: parsed from bindings.json at boot
 *   - Resource structs: initialized hardware handles
 *
 * Zero heap allocation — all containers are fixed-size ETL.
 */

#pragma once

#include "etl/string.h"
#include "etl/vector.h"
#include "etl/array.h"
#include "driver/gpio.h"

namespace modesp {

// ═══════════════════════════════════════════════════════════════
// System limits
// ═══════════════════════════════════════════════════════════════

static constexpr size_t MAX_RELAYS         = 8;
static constexpr size_t MAX_ONEWIRE_BUSES  = 4;
static constexpr size_t MAX_ADC_CHANNELS   = 8;
static constexpr size_t MAX_PWM_CHANNELS   = 8;
static constexpr size_t MAX_BINDINGS       = 24;
static constexpr size_t MAX_SENSORS        = 16;
static constexpr size_t MAX_ACTUATORS      = 16;

// ═══════════════════════════════════════════════════════════════
// String types for IDs and names
// ═══════════════════════════════════════════════════════════════

using HalId         = etl::string<16>;
using Role          = etl::string<16>;
using DriverType    = etl::string<16>;
using ModuleName    = etl::string<16>;
using SensorAddress = etl::string<24>;  // "28:FF:AA:BB:CC:DD:EE:01"

// ═══════════════════════════════════════════════════════════════
// Board config structs (parsed from board.json)
// ═══════════════════════════════════════════════════════════════

struct GpioOutputConfig {
    HalId id;
    gpio_num_t gpio;
    bool active_high;
};

struct OneWireBusConfig {
    HalId id;
    gpio_num_t gpio;
};

struct GpioInputConfig {
    HalId id;
    gpio_num_t gpio;
    bool pull_up;       // true = pull-up, false = pull-down
};

struct AdcChannelConfig {
    HalId id;
    gpio_num_t gpio;
    uint8_t atten;      // 0=0dB, 2=2.5dB, 6=6dB, 11=11dB (0-3.3V)
};

struct BoardConfig {
    etl::string<24> board_name;
    etl::string<8>  board_version;
    etl::vector<GpioOutputConfig, MAX_RELAYS>          gpio_outputs;
    etl::vector<OneWireBusConfig, MAX_ONEWIRE_BUSES>   onewire_buses;
    etl::vector<GpioInputConfig, MAX_ADC_CHANNELS>     gpio_inputs;
    etl::vector<AdcChannelConfig, MAX_ADC_CHANNELS>    adc_channels;
};

// ═══════════════════════════════════════════════════════════════
// Binding config (parsed from bindings.json)
// ═══════════════════════════════════════════════════════════════

struct Binding {
    HalId         hardware_id;
    Role          role;
    DriverType    driver_type;
    ModuleName    module_name;
    SensorAddress address;      // Опціональна ROM адреса (multi-sensor)
};

struct BindingTable {
    etl::vector<Binding, MAX_BINDINGS> bindings;
};

// ═══════════════════════════════════════════════════════════════
// HAL resources (initialized hardware)
// ═══════════════════════════════════════════════════════════════

struct GpioOutputResource {
    HalId id;
    gpio_num_t gpio;
    bool active_high;
    bool initialized = false;
};

struct OneWireBusResource {
    HalId id;
    gpio_num_t gpio;
    bool initialized = false;
};

struct GpioInputResource {
    HalId id;
    gpio_num_t gpio;
    bool pull_up;
    bool initialized = false;
};

struct AdcChannelResource {
    HalId id;
    gpio_num_t gpio;
    uint8_t atten;
    bool initialized = false;
};

} // namespace modesp
