/**
 * @file hal.h
 * @brief Hardware Abstraction Layer — GPIO initialization and resource lookup
 *
 * HAL initializes physical hardware (GPIO pins, buses) from BoardConfig
 * and provides resource lookup by hardware ID. Drivers use HAL resources
 * to access their hardware without knowing pin numbers.
 *
 * All resources are statically allocated — zero heap.
 */

#pragma once

#include "modesp/hal/hal_types.h"
#include "etl/string_view.h"

namespace modesp {

class HAL {
public:
    /// Initialize all hardware resources from board configuration.
    /// Sets GPIOs to safe state (relays OFF).
    bool init(const BoardConfig& config);

    /// Find a GPIO output resource by hardware ID (e.g. "relay_1")
    GpioOutputResource*  find_gpio_output(etl::string_view id);

    /// Find a OneWire bus resource by hardware ID (e.g. "ow_1")
    OneWireBusResource*  find_onewire_bus(etl::string_view id);

    size_t gpio_output_count() const { return gpio_output_count_; }
    size_t onewire_count() const { return onewire_count_; }

private:
    etl::array<GpioOutputResource, MAX_RELAYS>        gpio_outputs_;
    etl::array<OneWireBusResource, MAX_ONEWIRE_BUSES> onewire_buses_;
    size_t gpio_output_count_ = 0;
    size_t onewire_count_     = 0;

    bool init_gpio_outputs(const BoardConfig& config);
    bool init_onewire(const BoardConfig& config);
};

} // namespace modesp
