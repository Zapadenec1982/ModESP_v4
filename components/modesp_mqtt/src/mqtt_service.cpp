/**
 * @file mqtt_service.cpp
 * @brief MQTT client — publish state, subscribe commands, HTTP config API
 */

#include "modesp/net/mqtt_service.h"
#include "modesp/shared_state.h"
#include "modesp/services/nvs_helper.h"
#include "modesp/types.h"

#include "mqtt_topics.h"
#include "state_meta.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crt_bundle.h"

#define JSMN_STATIC
#include "jsmn.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char* TAG = "MQTT";

namespace modesp {

// ── NVS config ──────────────────────────────────────────────────

void MqttService::load_config() {
    nvs_helper::read_str("mqtt", "broker", broker_, sizeof(broker_));
    nvs_helper::read_str("mqtt", "user", user_, sizeof(user_));
    nvs_helper::read_str("mqtt", "pass", pass_, sizeof(pass_));
    nvs_helper::read_str("mqtt", "prefix", prefix_, sizeof(prefix_));
    nvs_helper::read_bool("mqtt", "enabled", enabled_);

    int32_t p = 1883;
    if (nvs_helper::read_i32("mqtt", "port", p)) {
        port_ = static_cast<uint16_t>(p);
    }
}

bool MqttService::save_config(const char* broker, uint16_t port,
                               const char* user, const char* pass,
                               const char* prefix, bool enabled) {
    bool ok = true;
    if (broker)  ok &= nvs_helper::write_str("mqtt", "broker", broker);
    if (user)    ok &= nvs_helper::write_str("mqtt", "user", user);
    if (pass)    ok &= nvs_helper::write_str("mqtt", "pass", pass);
    if (prefix)  ok &= nvs_helper::write_str("mqtt", "prefix", prefix);
    ok &= nvs_helper::write_i32("mqtt", "port", static_cast<int32_t>(port));
    ok &= nvs_helper::write_bool("mqtt", "enabled", enabled);

    if (ok) {
        if (broker) {
            strncpy(broker_, broker, sizeof(broker_) - 1);
            broker_[sizeof(broker_) - 1] = '\0';
        }
        port_ = port;
        if (user) {
            strncpy(user_, user, sizeof(user_) - 1);
            user_[sizeof(user_) - 1] = '\0';
        }
        if (pass) {
            strncpy(pass_, pass, sizeof(pass_) - 1);
            pass_[sizeof(pass_) - 1] = '\0';
        }
        if (prefix) {
            strncpy(prefix_, prefix, sizeof(prefix_) - 1);
            prefix_[sizeof(prefix_) - 1] = '\0';
        }
        enabled_ = enabled;
        ESP_LOGI(TAG, "Config saved (broker=%s, port=%d, enabled=%d)",
                 broker_, port_, enabled_);
    }
    return ok;
}

void MqttService::build_default_prefix() {
    if (prefix_[0] != '\0') return;  // Вже задано

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(prefix_, sizeof(prefix_), "modesp/%02X%02X%02X",
             mac[3], mac[4], mac[5]);
}

// ── Lifecycle ───────────────────────────────────────────────────

bool MqttService::on_init() {
    ESP_LOGI(TAG, "Initializing MQTT service...");

    load_config();
    build_default_prefix();

    state_set("mqtt.connected", false);
    state_set("mqtt.broker", broker_[0] ? broker_ : "");

    if (!enabled_) {
        state_set("mqtt.status", "disabled");
        ESP_LOGI(TAG, "MQTT disabled (not configured)");
        return true;
    }

    if (broker_[0] == '\0') {
        state_set("mqtt.status", "no_broker");
        ESP_LOGW(TAG, "MQTT enabled but no broker configured");
        return true;
    }

    // Deferred start: не підключаємось одразу — чекаємо WiFi в on_update()
    state_set("mqtt.status", "waiting_wifi");
    ESP_LOGI(TAG, "MQTT ready, waiting for WiFi...");
    return true;
}

void MqttService::on_update(uint32_t dt_ms) {
    // Deferred start: чекаємо WiFi перед першим підключенням
    if (!client_ && enabled_ && broker_[0] != '\0' && state_) {
        auto wifi_val = state_->get("wifi.connected");
        if (wifi_val.has_value()
            && etl::holds_alternative<bool>(wifi_val.value())
            && etl::get<bool>(wifi_val.value())) {
            ESP_LOGI(TAG, "WiFi connected, starting MQTT client...");
            state_set("mqtt.status", "connecting");
            start_client();
        }
        return;
    }

    if (!enabled_ || !connected_) return;

    publish_timer_ += dt_ms;
    if (publish_timer_ < PUBLISH_INTERVAL_MS) return;
    publish_timer_ = 0;

    // Публікуємо тільки якщо state змінився
    if (state_ && state_->version() != last_version_) {
        publish_state();
    }
}

void MqttService::on_stop() {
    stop_client();
}

// ── MQTT client ─────────────────────────────────────────────────

bool MqttService::start_client() {
    if (client_) {
        stop_client();
    }

    // Формуємо URI: broker вже містить "mqtt://host" або просто "host"
    // Порт 8883 = стандарт MQTT over TLS → автоматично mqtts://
    char uri[160];
    if (strncmp(broker_, "mqtt://", 7) == 0 ||
        strncmp(broker_, "mqtts://", 8) == 0) {
        snprintf(uri, sizeof(uri), "%s:%d", broker_, port_);
    } else if (port_ == 8883) {
        snprintf(uri, sizeof(uri), "mqtts://%s:%d", broker_, port_);
    } else {
        snprintf(uri, sizeof(uri), "mqtt://%s:%d", broker_, port_);
    }

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = uri;
    mqtt_cfg.credentials.username = user_[0] ? user_ : nullptr;
    mqtt_cfg.credentials.authentication.password = pass_[0] ? pass_ : nullptr;

    // TLS: підключити вбудований CA bundle для mqtts://
    if (strncmp(uri, "mqtts://", 8) == 0) {
        mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        ESP_LOGI(TAG, "TLS enabled (built-in CA bundle)");
    }

    // LWT (Last Will and Testament) — "offline" при розриві
    char lwt_topic[80];
    snprintf(lwt_topic, sizeof(lwt_topic), "%s/status", prefix_);
    mqtt_cfg.session.last_will.topic = lwt_topic;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.msg_len = 7;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    client_ = esp_mqtt_client_init(&mqtt_cfg);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        state_set("mqtt.status", "error");
        return false;
    }

    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY,
                                    mqtt_event_handler, this);

    esp_err_t err = esp_mqtt_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        state_set("mqtt.status", "error");
        return false;
    }

    ESP_LOGI(TAG, "MQTT client started → %s (prefix=%s)", uri, prefix_);
    return true;
}

void MqttService::stop_client() {
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    connected_ = false;
    state_set("mqtt.connected", false);
}

void MqttService::reconnect() {
    stop_client();
    if (enabled_ && broker_[0] != '\0') {
        state_set("mqtt.status", "connecting");
        start_client();
    } else if (!enabled_) {
        state_set("mqtt.status", "disabled");
    }
}

// ── MQTT event handler (static callback) ────────────────────────

void MqttService::mqtt_event_handler(void* args, esp_event_base_t base,
                                      int32_t event_id, void* event_data) {
    auto* self = static_cast<MqttService*>(args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            self->connected_ = true;
            self->last_version_ = 0;  // Форсуємо повну публікацію
            self->state_set("mqtt.connected", true);
            self->state_set("mqtt.status", "connected");
            ESP_LOGI(TAG, "Connected to broker");

            // Publish "online" status (retain)
            char topic[80];
            snprintf(topic, sizeof(topic), "%s/status", self->prefix_);
            esp_mqtt_client_publish(self->client_, topic, "online", 6, 1, 1);

            // Subscribe to command topics
            for (size_t i = 0; i < gen::MQTT_SUBSCRIBE_COUNT; i++) {
                if (!self->connected_) break;  // З'єднання втрачено під час підписки
                char sub_topic[80];
                snprintf(sub_topic, sizeof(sub_topic), "%s/cmd/%s",
                         self->prefix_, gen::MQTT_SUBSCRIBE[i]);
                esp_mqtt_client_subscribe(self->client_, sub_topic, 0);
            }
            ESP_LOGI(TAG, "Subscribed to %d command topics", (int)gen::MQTT_SUBSCRIBE_COUNT);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            self->connected_ = false;
            self->state_set("mqtt.connected", false);
            self->state_set("mqtt.status", "disconnected");
            ESP_LOGW(TAG, "Disconnected from broker");
            break;

        case MQTT_EVENT_DATA:
            self->handle_incoming(event->topic, event->topic_len,
                                   event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error (type=%d)", event->error_handle->error_type);
            self->state_set("mqtt.status", "error");
            break;

        default:
            break;
    }
}

// ── Publish state ───────────────────────────────────────────────

int MqttService::format_value(const StateValue& val, char* buf, size_t buf_size) {
    if (etl::holds_alternative<float>(val)) {
        return snprintf(buf, buf_size, "%.2f",
                        static_cast<double>(etl::get<float>(val)));
    } else if (etl::holds_alternative<int32_t>(val)) {
        return snprintf(buf, buf_size, "%ld",
                        (long)etl::get<int32_t>(val));
    } else if (etl::holds_alternative<bool>(val)) {
        return snprintf(buf, buf_size, "%s",
                        etl::get<bool>(val) ? "true" : "false");
    } else if (etl::holds_alternative<StringValue>(val)) {
        return snprintf(buf, buf_size, "%s",
                        etl::get<StringValue>(val).c_str());
    }
    return 0;
}

void MqttService::publish_state() {
    if (!client_ || !state_) return;

    for (size_t i = 0; i < gen::MQTT_PUBLISH_COUNT && i < MAX_PUBLISH_KEYS; i++) {
        auto val = state_->get(gen::MQTT_PUBLISH[i]);
        if (!val.has_value()) continue;

        char payload[32];
        int len = format_value(val.value(), payload, sizeof(payload));
        if (len <= 0) continue;

        // Delta-publish: порівнюємо з попереднім значенням
        if (strncmp(payload, last_payloads_[i], sizeof(last_payloads_[i])) == 0) {
            continue;  // Значення не змінилось — не публікуємо
        }

        char topic[80];
        snprintf(topic, sizeof(topic), "%s/state/%s",
                 prefix_, gen::MQTT_PUBLISH[i]);

        esp_mqtt_client_publish(client_, topic, payload, len, 0, 0);

        // Зберігаємо опубліковане значення в кеш
        strncpy(last_payloads_[i], payload, sizeof(last_payloads_[i]) - 1);
        last_payloads_[i][sizeof(last_payloads_[i]) - 1] = '\0';
    }

    last_version_ = state_->version();
}

// ── Handle incoming commands ────────────────────────────────────

void MqttService::handle_incoming(const char* topic, int topic_len,
                                    const char* data, int data_len) {
    // Вхідний topic: "{prefix}/cmd/{key}"
    // Шукаємо "/cmd/" в topic
    char topic_buf[80];
    int copy_len = (topic_len < (int)sizeof(topic_buf) - 1)
                   ? topic_len : (int)sizeof(topic_buf) - 1;
    memcpy(topic_buf, topic, copy_len);
    topic_buf[copy_len] = '\0';

    // Знайти "/cmd/" в topic
    const char* cmd_marker = strstr(topic_buf, "/cmd/");
    if (!cmd_marker) {
        ESP_LOGW(TAG, "Unexpected topic: %s", topic_buf);
        return;
    }

    const char* key = cmd_marker + 5;  // skip "/cmd/"

    // Валідація: ключ має бути в STATE_META як writable
    const auto* meta = gen::find_state_meta(key);
    if (!meta) {
        ESP_LOGW(TAG, "Unknown key in MQTT cmd: %s", key);
        return;
    }
    if (!meta->writable) {
        ESP_LOGW(TAG, "Key not writable: %s", key);
        return;
    }

    // Копіюємо payload в null-terminated буфер
    char val_buf[32];
    int val_len = (data_len < (int)sizeof(val_buf) - 1)
                  ? data_len : (int)sizeof(val_buf) - 1;
    memcpy(val_buf, data, val_len);
    val_buf[val_len] = '\0';

    ESP_LOGI(TAG, "CMD: %s = %s", key, val_buf);

    // Парсимо значення за типом з meta
    if (strcmp(meta->type, "float") == 0) {
        char* end = nullptr;
        float f = strtof(val_buf, &end);
        if (end == val_buf) {
            ESP_LOGW(TAG, "Invalid float: %s", val_buf);
            return;
        }
        // Валідація min/max
        if (f < meta->min_val) f = meta->min_val;
        if (f > meta->max_val) f = meta->max_val;
        state_set(key, f);
    } else if (strcmp(meta->type, "int") == 0) {
        char* end = nullptr;
        long v = strtol(val_buf, &end, 10);
        if (end == val_buf) {
            ESP_LOGW(TAG, "Invalid int: %s", val_buf);
            return;
        }
        if (v < static_cast<long>(meta->min_val)) v = static_cast<long>(meta->min_val);
        if (v > static_cast<long>(meta->max_val)) v = static_cast<long>(meta->max_val);
        state_set(key, static_cast<int32_t>(v));
    } else if (strcmp(meta->type, "bool") == 0) {
        bool b = (strcmp(val_buf, "true") == 0 || strcmp(val_buf, "1") == 0);
        state_set(key, b);
    }
}

// ── HTTP API ────────────────────────────────────────────────────

void MqttService::set_cors_headers(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

esp_err_t MqttService::handle_options(httpd_req_t* req) {
    set_cors_headers(req);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

void MqttService::set_http_server(httpd_handle_t server) {
    if (server_ == server && http_registered_) return;
    server_ = server;
    if (server_) {
        register_http_handlers();
    }
}

void MqttService::register_http_handlers() {
    if (!server_ || http_registered_) return;

    httpd_uri_t get_mqtt = {};
    get_mqtt.uri      = "/api/mqtt";
    get_mqtt.method   = HTTP_GET;
    get_mqtt.handler  = handle_get_mqtt;
    get_mqtt.user_ctx = this;
    httpd_register_uri_handler(server_, &get_mqtt);

    httpd_uri_t post_mqtt = {};
    post_mqtt.uri      = "/api/mqtt";
    post_mqtt.method   = HTTP_POST;
    post_mqtt.handler  = handle_post_mqtt;
    post_mqtt.user_ctx = this;
    httpd_register_uri_handler(server_, &post_mqtt);

    // OPTIONS для CORS preflight
    httpd_uri_t options = {};
    options.uri      = "/api/mqtt";
    options.method   = HTTP_OPTIONS;
    options.handler  = handle_options;
    options.user_ctx = this;
    httpd_register_uri_handler(server_, &options);

    http_registered_ = true;
    ESP_LOGI(TAG, "HTTP handlers registered (GET/POST /api/mqtt)");
}

esp_err_t MqttService::handle_get_mqtt(httpd_req_t* req) {
    auto* self = static_cast<MqttService*>(req->user_ctx);

    // Визначаємо status рядок
    const char* status = "disabled";
    if (self->enabled_) {
        if (self->connected_) status = "connected";
        else if (self->client_) status = "connecting";
        else if (self->broker_[0] == '\0') status = "no_broker";
        else status = "disconnected";
    }

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"enabled\":%s,"
        "\"connected\":%s,"
        "\"broker\":\"%s\","
        "\"port\":%d,"
        "\"user\":\"%s\","
        "\"prefix\":\"%s\","
        "\"status\":\"%s\"}",
        self->enabled_ ? "true" : "false",
        self->connected_ ? "true" : "false",
        self->broker_,
        self->port_,
        self->user_,
        self->prefix_,
        status);

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

esp_err_t MqttService::handle_post_mqtt(httpd_req_t* req) {
    auto* self = static_cast<MqttService*>(req->user_ctx);

    char buf[384];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    // Parse JSON з jsmn
    jsmn_parser parser;
    jsmntok_t tokens[20];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, buf, len, tokens, 20);
    if (r < 1 || tokens[0].type != JSMN_OBJECT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Зчитуємо поточні значення як fallback
    char new_broker[128];
    strncpy(new_broker, self->broker_, sizeof(new_broker));
    uint16_t new_port = self->port_;
    char new_user[64];
    strncpy(new_user, self->user_, sizeof(new_user));
    char new_pass[64];
    strncpy(new_pass, self->pass_, sizeof(new_pass));
    char new_prefix[48];
    strncpy(new_prefix, self->prefix_, sizeof(new_prefix));
    bool new_enabled = self->enabled_;

    for (int i = 1; i < r - 1; i += 2) {
        if (tokens[i].type != JSMN_STRING) continue;

        buf[tokens[i].end] = '\0';
        buf[tokens[i + 1].end] = '\0';

        const char* key = buf + tokens[i].start;
        const char* val = buf + tokens[i + 1].start;

        // Приймаємо як "broker" так і "mqtt.broker" (WebUI toggle/save)
        const char* k = key;
        if (strncmp(k, "mqtt.", 5) == 0) k += 5;  // Відрізаємо префікс

        if (strcmp(k, "broker") == 0) {
            strncpy(new_broker, val, sizeof(new_broker) - 1);
            new_broker[sizeof(new_broker) - 1] = '\0';
        } else if (strcmp(k, "port") == 0) {
            new_port = static_cast<uint16_t>(atoi(val));
        } else if (strcmp(k, "user") == 0) {
            strncpy(new_user, val, sizeof(new_user) - 1);
            new_user[sizeof(new_user) - 1] = '\0';
        } else if (strcmp(k, "password") == 0) {
            strncpy(new_pass, val, sizeof(new_pass) - 1);
            new_pass[sizeof(new_pass) - 1] = '\0';
        } else if (strcmp(k, "prefix") == 0) {
            strncpy(new_prefix, val, sizeof(new_prefix) - 1);
            new_prefix[sizeof(new_prefix) - 1] = '\0';
        } else if (strcmp(k, "enabled") == 0) {
            new_enabled = (val[0] == 't' || val[0] == '1');
        }
    }

    bool ok = self->save_config(new_broker, new_port, new_user,
                                 new_pass, new_prefix, new_enabled);

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    if (ok) {
        // Перезапустити клієнт з новими налаштуваннями
        self->reconnect();
        httpd_resp_sendstr(req, "{\"ok\":true,\"msg\":\"Config saved, reconnecting\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save");
    }
    return ESP_OK;
}

} // namespace modesp
