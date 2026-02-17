/**
 * @file config_service.h
 * @brief Configuration service — mounts LittleFS, parses board.json and bindings.json
 *
 * ConfigService runs at boot (CRITICAL priority, before HAL).
 * It reads JSON config files from a LittleFS partition and fills
 * BoardConfig + BindingTable structs used by HAL and DriverManager.
 *
 * All JSON parsing uses jsmn on a stack-allocated buffer — zero heap.
 */

#pragma once

#include "modesp/base_module.h"
#include "modesp/hal/hal_types.h"

namespace modesp {

class ConfigService : public BaseModule {
public:
    ConfigService() : BaseModule("config", ModulePriority::CRITICAL) {}

    bool on_init() override;

    const BoardConfig&  board_config()  const { return board_config_; }
    const BindingTable& binding_table() const { return binding_table_; }

private:
    BoardConfig  board_config_;
    BindingTable binding_table_;

    bool mount_littlefs();
    bool parse_board_json();
    bool parse_bindings_json();

    // JSON parsing limits (stack-allocated)
    // Keep small to avoid stack overflow on main task (default 3584 bytes)
    static constexpr size_t MAX_JSON_SIZE = 512;
    static constexpr size_t MAX_TOKENS    = 64;
};

} // namespace modesp
