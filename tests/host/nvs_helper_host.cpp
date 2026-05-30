// In-memory NVS implementation for HOST tests (replaces ESP-IDF-backed nvs_helper.cpp).
// Storage is process-global; values round-trip within a test run. Sufficient for unit tests.
#include "modesp/services/nvs_helper.h"

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
std::string key_of(const char* ns, const char* key) {
    return std::string(ns) + "/" + std::string(key);
}
std::map<std::string, int32_t>& i32_store()   { static std::map<std::string, int32_t> m; return m; }
std::map<std::string, float>& f32_store()      { static std::map<std::string, float> m; return m; }
std::map<std::string, uint8_t>& bool_store()   { static std::map<std::string, uint8_t> m; return m; }
std::map<std::string, std::string>& str_store(){ static std::map<std::string, std::string> m; return m; }
std::map<std::string, std::vector<uint8_t>>& blob_store() {
    static std::map<std::string, std::vector<uint8_t>> m; return m;
}
}  // namespace

namespace modesp {
namespace nvs_helper {

bool init() { return true; }

bool read_i32(const char* ns, const char* key, int32_t& out) {
    auto& m = i32_store(); auto it = m.find(key_of(ns, key));
    if (it == m.end()) return false; out = it->second; return true;
}
bool write_i32(const char* ns, const char* key, int32_t value) {
    i32_store()[key_of(ns, key)] = value; return true;
}

bool read_float(const char* ns, const char* key, float& out) {
    auto& m = f32_store(); auto it = m.find(key_of(ns, key));
    if (it == m.end()) return false; out = it->second; return true;
}
bool write_float(const char* ns, const char* key, float value) {
    f32_store()[key_of(ns, key)] = value; return true;
}

bool read_bool(const char* ns, const char* key, bool& out) {
    auto& m = bool_store(); auto it = m.find(key_of(ns, key));
    if (it == m.end()) return false; out = (it->second != 0); return true;
}
bool write_bool(const char* ns, const char* key, bool value) {
    bool_store()[key_of(ns, key)] = value ? 1 : 0; return true;
}

bool read_str(const char* ns, const char* key, char* out, size_t max_len) {
    auto& m = str_store(); auto it = m.find(key_of(ns, key));
    if (it == m.end() || max_len == 0) return false;
    std::strncpy(out, it->second.c_str(), max_len - 1);
    out[max_len - 1] = '\0';
    return true;
}
bool write_str(const char* ns, const char* key, const char* value) {
    str_store()[key_of(ns, key)] = value ? value : ""; return true;
}

bool read_blob(const char* ns, const char* key, void* out, size_t max_len, size_t& out_len) {
    auto& m = blob_store(); auto it = m.find(key_of(ns, key));
    if (it == m.end()) return false;
    out_len = it->second.size();
    if (out_len > max_len) out_len = max_len;
    std::memcpy(out, it->second.data(), out_len);
    return true;
}
bool write_blob(const char* ns, const char* key, const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    blob_store()[key_of(ns, key)].assign(p, p + len); return true;
}

bool erase_key(const char* ns, const char* key) {
    const std::string k = key_of(ns, key);
    i32_store().erase(k); f32_store().erase(k); bool_store().erase(k);
    str_store().erase(k); blob_store().erase(k);
    return true;
}

}  // namespace nvs_helper
}  // namespace modesp
