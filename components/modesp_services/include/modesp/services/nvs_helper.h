/**
 * @file nvs_helper.h
 * @brief Simple NVS read/write helpers
 *
 * Thin wrappers around ESP-IDF NVS API.
 * Each function opens a handle, performs the operation, and closes it.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace modesp {
namespace nvs_helper {

/// Initialize NVS flash. Call once at boot before any NVS operations.
bool init();

/// Read a string value. Returns false if key not found or error.
bool read_str(const char* ns, const char* key, char* out, size_t max_len);

/// Write a string value. Returns false on error.
bool write_str(const char* ns, const char* key, const char* value);

/// Read a float value. Returns false if key not found or error.
bool read_float(const char* ns, const char* key, float& out);

/// Write a float value. Returns false on error.
bool write_float(const char* ns, const char* key, float value);

/// Read an int32_t value. Returns false if key not found or error.
bool read_i32(const char* ns, const char* key, int32_t& out);

/// Write an int32_t value. Returns false on error.
bool write_i32(const char* ns, const char* key, int32_t value);

/// Read a bool value (stored as uint8_t). Returns false if key not found or error.
bool read_bool(const char* ns, const char* key, bool& out);

/// Write a bool value (stored as uint8_t). Returns false on error.
bool write_bool(const char* ns, const char* key, bool value);

} // namespace nvs_helper
} // namespace modesp
