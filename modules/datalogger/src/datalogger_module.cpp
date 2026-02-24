/**
 * @brief DataLogger — multi-channel temperature & event logging to LittleFS.
 *
 * Samples up to 3 temperature channels every N seconds (air + optional evap/cond).
 * Logs equipment events on state change (edge-detect).
 * Append-only files with rotate. Streaming chunked JSON v2 via HTTP.
 *
 * JSON v2 format:
 *   {"channels":["air","evap","cond"],
 *    "temp":[[ts,a,e,c],...],
 *    "events":[[ts,type],...]}
 * where e/c = null when TEMP_NO_DATA.
 */

#include "datalogger_module.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <ctime>

static const char* TAG = "DataLogger";

DataLoggerModule::DataLoggerModule()
    : BaseModule("datalogger", modesp::ModulePriority::LOW)
{
}

// ── Час: UNIX epoch якщо SNTP синхронізовано, інакше uptime ──

uint32_t DataLoggerModule::current_timestamp() const {
    time_t now = time(nullptr);
    if (now > 1700000000) return static_cast<uint32_t>(now);  // SNTP OK
    // Fallback: uptime в секундах
    return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
}

// ── Допоміжна: int16 → JSON число або "null" ──

int DataLoggerModule::append_temp_val(char* buf, size_t sz, int16_t val) {
    if (val == TEMP_NO_DATA) {
        return snprintf(buf, sz, "null");
    }
    return snprintf(buf, sz, "%d", static_cast<int>(val));
}

// ── Міграція старого 8-байтного формату ──

void DataLoggerModule::migrate_old_format() {
    // Перевіряємо чи temp.bin має старий формат (розмір не кратний 12)
    struct stat st;
    bool need_migrate = false;

    if (stat(TEMP_FILE, &st) == 0 && st.st_size > 0) {
        if (st.st_size % sizeof(TempRecord) != 0) {
            need_migrate = true;
        }
    }
    if (!need_migrate && stat(TEMP_OLD_FILE, &st) == 0 && st.st_size > 0) {
        if (st.st_size % sizeof(TempRecord) != 0) {
            need_migrate = true;
        }
    }

    if (need_migrate) {
        ESP_LOGW(TAG, "Старий формат temp файлів — видаляємо для міграції");
        remove(TEMP_FILE);
        remove(TEMP_OLD_FILE);
    }
}

// ── Init ──

bool DataLoggerModule::on_init() {
    // Створити директорію логів
    mkdir(LOG_DIR, 0775);

    // Міграція старого формату (8 bytes → 12 bytes)
    migrate_old_format();

    sync_settings();

    // Порахувати існуючі записи після ребуту
    struct stat st;
    temp_count_ = 0;
    if (stat(TEMP_FILE, &st) == 0)
        temp_count_ += st.st_size / sizeof(TempRecord);
    if (stat(TEMP_OLD_FILE, &st) == 0)
        temp_count_ += st.st_size / sizeof(TempRecord);

    event_count_ = 0;
    if (stat(EVENT_FILE, &st) == 0)
        event_count_ += st.st_size / sizeof(EventRecord);
    if (stat(EVENT_OLD_FILE, &st) == 0)
        event_count_ += st.st_size / sizeof(EventRecord);

    update_flash_used();

    // Ініціалізувати SharedState
    state_set("datalogger.records_count", static_cast<int32_t>(temp_count_));
    state_set("datalogger.events_count", static_cast<int32_t>(event_count_));
    state_set("datalogger.flash_used", static_cast<int32_t>(flash_used_kb_));

    // POWER_ON маркер
    log_event(EVENT_POWER_ON);

    // Прочитати початковий стан для edge-detect
    prev_compressor_     = read_bool("equipment.compressor", false);
    prev_defrost_active_ = read_bool("defrost.active", false);
    prev_door_open_      = read_bool("equipment.door_open", false);
    prev_alarm_high_     = read_bool("protection.high_temp_alarm", false);
    prev_alarm_low_      = read_bool("protection.low_temp_alarm", false);

    ESP_LOGI(TAG, "Ініціалізовано: %lu temp, %lu events, %lu KB flash (evap=%d, cond=%d)",
             (unsigned long)temp_count_, (unsigned long)event_count_,
             (unsigned long)flash_used_kb_, log_evap_, log_cond_);
    return true;
}

// ── Sync settings ──

void DataLoggerModule::sync_settings() {
    int32_t interval = read_int("datalogger.sample_interval", 60);
    sample_interval_ms_ = interval * 1000;
    retention_hours_ = read_int("datalogger.retention_hours", 48);
    log_evap_ = read_bool("datalogger.log_evap", false);
    log_cond_ = read_bool("datalogger.log_cond", false);
}

// ── Main loop ──

void DataLoggerModule::on_update(uint32_t dt_ms) {
    if (!read_bool("datalogger.enabled", true)) return;

    sync_settings();

    // 1. Семплювання температури (3 канали)
    sample_timer_ms_ += dt_ms;
    if (sample_timer_ms_ >= static_cast<uint32_t>(sample_interval_ms_)) {
        sample_timer_ms_ = 0;

        TempRecord rec;
        rec.timestamp = current_timestamp();

        // Air — завжди логується
        float air = read_float("equipment.air_temp", 0.0f);
        rec.air_x10 = static_cast<int16_t>(air * 10.0f);

        // Evap — тільки якщо enabled І датчик присутній
        if (log_evap_ && read_bool("equipment.has_evap_temp", false)) {
            float evap = read_float("equipment.evap_temp", 0.0f);
            rec.evap_x10 = static_cast<int16_t>(evap * 10.0f);
        } else {
            rec.evap_x10 = TEMP_NO_DATA;
        }

        // Cond — тільки якщо enabled І датчик присутній
        if (log_cond_ && read_bool("equipment.has_cond_temp", false)) {
            float cond = read_float("equipment.cond_temp", 0.0f);
            rec.cond_x10 = static_cast<int16_t>(cond * 10.0f);
        } else {
            rec.cond_x10 = TEMP_NO_DATA;
        }

        rec._reserved = TEMP_NO_DATA;

        if (!temp_buf_.full()) {
            temp_buf_.push_back(rec);
            // Оновлюємо кількість записів (flushed + RAM) одразу для UI
            state_set("datalogger.records_count",
                      static_cast<int32_t>(temp_count_ + temp_buf_.size()));
        }
    }

    // 2. Polling подій (edge-detect, дешево — тільки read_bool)
    size_t events_before = event_buf_.size();
    poll_events();
    if (event_buf_.size() != events_before) {
        state_set("datalogger.events_count",
                  static_cast<int32_t>(event_count_ + event_buf_.size()));
    }

    // 3. Flush на LittleFS кожні 10 хвилин
    flush_timer_ms_ += dt_ms;
    if (flush_timer_ms_ >= FLUSH_INTERVAL_MS) {
        flush_timer_ms_ = 0;
        flush_to_flash();
    }
}

// ── Edge-detect подій ──

void DataLoggerModule::poll_events() {
    bool comp = read_bool("equipment.compressor", false);
    if (comp != prev_compressor_) {
        log_event(comp ? EVENT_COMPRESSOR_ON : EVENT_COMPRESSOR_OFF);
        prev_compressor_ = comp;
    }

    bool defrost = read_bool("defrost.active", false);
    if (defrost != prev_defrost_active_) {
        log_event(defrost ? EVENT_DEFROST_START : EVENT_DEFROST_END);
        prev_defrost_active_ = defrost;
    }

    bool door = read_bool("equipment.door_open", false);
    if (door != prev_door_open_) {
        log_event(door ? EVENT_DOOR_OPEN : EVENT_DOOR_CLOSE);
        prev_door_open_ = door;
    }

    bool alarm_high = read_bool("protection.high_temp_alarm", false);
    if (alarm_high && !prev_alarm_high_) {
        log_event(EVENT_ALARM_HIGH);
    }
    prev_alarm_high_ = alarm_high;

    bool alarm_low = read_bool("protection.low_temp_alarm", false);
    if (alarm_low && !prev_alarm_low_) {
        log_event(EVENT_ALARM_LOW);
    }
    // Скидання аварії → EVENT_ALARM_CLEAR
    if ((prev_alarm_high_ && !alarm_high) || (prev_alarm_low_ && !alarm_low)) {
        log_event(EVENT_ALARM_CLEAR);
    }
    prev_alarm_low_ = alarm_low;
}

// ── Запис події в RAM буфер ──

void DataLoggerModule::log_event(EventType type) {
    EventRecord rec;
    rec.timestamp = current_timestamp();
    rec.event_type = static_cast<uint8_t>(type);
    rec._pad[0] = 0;
    rec._pad[1] = 0;
    rec._pad[2] = 0;

    if (!event_buf_.full()) {
        event_buf_.push_back(rec);
    } else {
        ESP_LOGW(TAG, "Event buffer full, dropping event %d", type);
    }
}

// ── Flush RAM → LittleFS ──

bool DataLoggerModule::flush_to_flash() {
    if (temp_buf_.empty() && event_buf_.empty()) return true;

    // Flush температури
    if (!temp_buf_.empty()) {
        FILE* f = fopen(TEMP_FILE, "ab");
        if (f) {
            size_t written = fwrite(temp_buf_.data(), sizeof(TempRecord),
                                    temp_buf_.size(), f);
            fclose(f);
            temp_count_ += written;
            temp_buf_.clear();
            state_set("datalogger.records_count", static_cast<int32_t>(temp_count_));
        } else {
            ESP_LOGE(TAG, "Не вдалося відкрити %s", TEMP_FILE);
        }

        size_t max_size = static_cast<size_t>(retention_hours_) * 60 * sizeof(TempRecord);
        rotate_if_needed(TEMP_FILE, max_size);
    }

    // Flush подій
    if (!event_buf_.empty()) {
        FILE* f = fopen(EVENT_FILE, "ab");
        if (f) {
            size_t written = fwrite(event_buf_.data(), sizeof(EventRecord),
                                    event_buf_.size(), f);
            fclose(f);
            event_count_ += written;
            event_buf_.clear();
            state_set("datalogger.events_count", static_cast<int32_t>(event_count_));
        } else {
            ESP_LOGE(TAG, "Не вдалося відкрити %s", EVENT_FILE);
        }

        rotate_if_needed(EVENT_FILE, EVENT_MAX_SIZE);
    }

    update_flash_used();
    state_set("datalogger.flash_used", static_cast<int32_t>(flash_used_kb_));

    ESP_LOGD(TAG, "Flush: %lu temp, %lu events",
             (unsigned long)temp_count_, (unsigned long)event_count_);
    return true;
}

// ── Ротація файлу ──

void DataLoggerModule::rotate_if_needed(const char* path, size_t max_size) {
    struct stat st;
    if (stat(path, &st) != 0) return;
    if (static_cast<size_t>(st.st_size) <= max_size) return;

    // Визначити .old шлях
    const char* old_path = nullptr;
    if (strcmp(path, TEMP_FILE) == 0) {
        old_path = TEMP_OLD_FILE;
    } else if (strcmp(path, EVENT_FILE) == 0) {
        old_path = EVENT_OLD_FILE;
    }
    if (!old_path) return;

    remove(old_path);
    rename(path, old_path);
    ESP_LOGI(TAG, "Ротація %s (%lu bytes)", path, (unsigned long)st.st_size);
}

// ── Підрахунок flash ──

void DataLoggerModule::update_flash_used() {
    struct stat st;
    size_t total = 0;
    if (stat(TEMP_FILE, &st) == 0) total += st.st_size;
    if (stat(TEMP_OLD_FILE, &st) == 0) total += st.st_size;
    if (stat(EVENT_FILE, &st) == 0) total += st.st_size;
    if (stat(EVENT_OLD_FILE, &st) == 0) total += st.st_size;
    flash_used_kb_ = static_cast<uint32_t>((total + 512) / 1024);
}

// ── Streaming chunked JSON v2 для GET /api/log ──
//
// Формат: {"channels":["air","evap","cond"],
//          "temp":[[ts,air,evap,cond],...],
//          "events":[[ts,type],...]}
// Значення evap/cond = null якщо TEMP_NO_DATA.

esp_err_t DataLoggerModule::serialize_log_chunked(httpd_req_t* req, int hours) const {
    char buf[256];
    int len;

    uint32_t cutoff = 0;
    if (hours > 0) {
        uint32_t now = current_timestamp();
        if (now > static_cast<uint32_t>(hours * 3600)) {
            cutoff = now - static_cast<uint32_t>(hours * 3600);
        }
    }

    // 1. Header з channels + початок temp масиву
    len = snprintf(buf, sizeof(buf),
        "{\"channels\":[\"air\",\"evap\",\"cond\"],\"temp\":[");
    httpd_resp_send_chunk(req, buf, len);

    // 2. Читаємо temp.old + temp.bin
    const char* temp_files[] = {TEMP_OLD_FILE, TEMP_FILE};
    bool first = true;

    for (int fi = 0; fi < 2; fi++) {
        FILE* f = fopen(temp_files[fi], "rb");
        if (!f) continue;

        TempRecord rec;
        while (fread(&rec, sizeof(rec), 1, f) == 1) {
            if (rec.timestamp < cutoff) continue;

            // [ts, air, evap_or_null, cond_or_null]
            char tmp[128];
            int pos = 0;
            pos += snprintf(tmp + pos, sizeof(tmp) - pos, "%s[%lu,%d,",
                           first ? "" : ",",
                           (unsigned long)rec.timestamp,
                           static_cast<int>(rec.air_x10));
            pos += append_temp_val(tmp + pos, sizeof(tmp) - pos, rec.evap_x10);
            pos += snprintf(tmp + pos, sizeof(tmp) - pos, ",");
            pos += append_temp_val(tmp + pos, sizeof(tmp) - pos, rec.cond_x10);
            pos += snprintf(tmp + pos, sizeof(tmp) - pos, "]");

            if (httpd_resp_send_chunk(req, tmp, pos) != ESP_OK) {
                fclose(f);
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            first = false;
        }
        fclose(f);
    }

    // 3. Також RAM буфер (ще не flush-нутий)
    for (size_t i = 0; i < temp_buf_.size(); i++) {
        const auto& rec = temp_buf_[i];
        if (rec.timestamp < cutoff) continue;

        char tmp[128];
        int pos = 0;
        pos += snprintf(tmp + pos, sizeof(tmp) - pos, "%s[%lu,%d,",
                       first ? "" : ",",
                       (unsigned long)rec.timestamp,
                       static_cast<int>(rec.air_x10));
        pos += append_temp_val(tmp + pos, sizeof(tmp) - pos, rec.evap_x10);
        pos += snprintf(tmp + pos, sizeof(tmp) - pos, ",");
        pos += append_temp_val(tmp + pos, sizeof(tmp) - pos, rec.cond_x10);
        pos += snprintf(tmp + pos, sizeof(tmp) - pos, "]");

        httpd_resp_send_chunk(req, tmp, pos);
        first = false;
    }

    // 4. Events section
    len = snprintf(buf, sizeof(buf), "],\"events\":[");
    httpd_resp_send_chunk(req, buf, len);

    const char* event_files[] = {EVENT_OLD_FILE, EVENT_FILE};
    first = true;

    for (int fi = 0; fi < 2; fi++) {
        FILE* f = fopen(event_files[fi], "rb");
        if (!f) continue;

        EventRecord rec;
        while (fread(&rec, sizeof(rec), 1, f) == 1) {
            if (rec.timestamp < cutoff) continue;
            len = snprintf(buf, sizeof(buf), "%s[%lu,%d]",
                          first ? "" : ",",
                          (unsigned long)rec.timestamp,
                          (int)rec.event_type);
            if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                fclose(f);
                httpd_resp_send_chunk(req, nullptr, 0);
                return ESP_FAIL;
            }
            first = false;
        }
        fclose(f);
    }

    // Також RAM events
    for (size_t i = 0; i < event_buf_.size(); i++) {
        const auto& rec = event_buf_[i];
        if (rec.timestamp < cutoff) continue;
        len = snprintf(buf, sizeof(buf), "%s[%lu,%d]",
                      first ? "" : ",",
                      (unsigned long)rec.timestamp,
                      (int)rec.event_type);
        httpd_resp_send_chunk(req, buf, len);
        first = false;
    }

    // 5. Footer
    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, nullptr, 0);  // end chunked
    return ESP_OK;
}

// ── Summary для /api/log/summary ──

bool DataLoggerModule::serialize_summary(char* buf, size_t buf_size) const {
    // Включаємо RAM буфер в лічильники
    uint32_t total_temp = temp_count_ + static_cast<uint32_t>(temp_buf_.size());
    uint32_t total_events = event_count_ + static_cast<uint32_t>(event_buf_.size());
    int len = snprintf(buf, buf_size,
        "{\"hours\":%ld,\"temp_count\":%lu,\"event_count\":%lu,\"flash_kb\":%lu}",
        (long)retention_hours_,
        (unsigned long)total_temp,
        (unsigned long)total_events,
        (unsigned long)flash_used_kb_);
    return len > 0 && static_cast<size_t>(len) < buf_size;
}

// ── Stop ──

void DataLoggerModule::on_stop() {
    // Flush залишки перед зупинкою
    flush_to_flash();
    ESP_LOGI(TAG, "Зупинено, фінальний flush виконано");
}
