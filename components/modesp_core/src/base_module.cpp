/**
 * @file base_module.cpp
 * @brief Реалізація BaseModule — делегування до ModuleManager/SharedState
 */

#include "modesp/base_module.h"
#include "modesp/module_manager.h"
#include "modesp/shared_state.h"

namespace modesp {

void BaseModule::publish(const etl::imessage& msg) {
    if (manager_) {
        manager_->publish(msg);
    }
}

bool BaseModule::state_set(const StateKey& key, const StateValue& value) {
    if (shared_state_) {
        return shared_state_->set(key, value);
    }
    return false;
}

bool BaseModule::state_set(const char* key, int32_t value) {
    return shared_state_ ? shared_state_->set(key, value) : false;
}

bool BaseModule::state_set(const char* key, float value) {
    return shared_state_ ? shared_state_->set(key, value) : false;
}

bool BaseModule::state_set(const char* key, bool value) {
    return shared_state_ ? shared_state_->set(key, value) : false;
}

bool BaseModule::state_set(const char* key, const char* value) {
    return shared_state_ ? shared_state_->set(key, value) : false;
}

etl::optional<StateValue> BaseModule::state_get(const StateKey& key) const {
    if (shared_state_) {
        return shared_state_->get(key);
    }
    return etl::nullopt;
}

etl::optional<StateValue> BaseModule::state_get(const char* key) const {
    return shared_state_ ? shared_state_->get(key) : etl::nullopt;
}

} // namespace modesp
