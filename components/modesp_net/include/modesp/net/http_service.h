/**
 * @file http_service.h
 * @brief HTTP server with REST API and static file serving
 *
 * Provides:
 *   GET  /api/state    — full SharedState as JSON
 *   GET  /api/board    — board.json contents
 *   GET  /api/ui       — ui.json (generated UI schema)
 *   GET  /api/bindings — bindings.json contents
 *   POST /api/bindings — save new bindings.json (needs restart)
 *   GET  /api/modules  — registered modules info
 *   POST /api/settings — update thermostat setpoint etc.
 *   POST /api/wifi     — save WiFi credentials
 *   GET  /api/wifi/scan — scan available WiFi networks
 *   GET  /api/wifi/ap   — current AP configuration
 *   POST /api/wifi/ap   — save AP configuration
 *   POST /api/restart  — restart ESP32
 *   GET  /[path]       — static files from LittleFS /data/www/
 */

#pragma once

#include "modesp/base_module.h"
#include "esp_http_server.h"

namespace modesp {

class SharedState;
class ConfigService;
class ModuleManager;
class WiFiService;
class PersistService;

class HttpService : public BaseModule {
public:
    HttpService() : BaseModule("http", ModulePriority::LOW) {}

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;

    // Dependencies injection
    void set_state(SharedState* state) { state_ = state; }
    void set_config(ConfigService* config) { config_ = config; }
    void set_modules(ModuleManager* modules) { modules_ = modules; }
    void set_wifi(WiFiService* wifi) { wifi_ = wifi; }
    void set_persist(PersistService* persist) { persist_ = persist; }

    // Server handle (needed by WsService)
    httpd_handle_t server() const { return server_; }

    // Register wildcard static file handler — call AFTER all other
    // URI handlers (API, WebSocket) are registered, because the /*
    // wildcard with httpd_uri_match_wildcard would shadow them.
    void register_static_handler();

private:
    httpd_handle_t server_ = nullptr;
    SharedState* state_ = nullptr;
    ConfigService* config_ = nullptr;
    ModuleManager* modules_ = nullptr;
    WiFiService* wifi_ = nullptr;
    PersistService* persist_ = nullptr;

    bool start_server();
    void register_api_handlers();

    // API handlers (static — httpd requires function pointers)
    static esp_err_t handle_get_state(httpd_req_t* req);
    static esp_err_t handle_get_board(httpd_req_t* req);
    static esp_err_t handle_get_ui(httpd_req_t* req);
    static esp_err_t handle_get_bindings(httpd_req_t* req);
    static esp_err_t handle_post_bindings(httpd_req_t* req);
    static esp_err_t handle_get_modules(httpd_req_t* req);
    static esp_err_t handle_post_settings(httpd_req_t* req);
    static esp_err_t handle_post_wifi(httpd_req_t* req);
    static esp_err_t handle_get_wifi_scan(httpd_req_t* req);
    static esp_err_t handle_get_wifi_ap(httpd_req_t* req);
    static esp_err_t handle_post_wifi_ap(httpd_req_t* req);
    static esp_err_t handle_post_restart(httpd_req_t* req);
    static esp_err_t handle_get_ota(httpd_req_t* req);
    static esp_err_t handle_post_ota(httpd_req_t* req);
    static esp_err_t handle_get_time(httpd_req_t* req);
    static esp_err_t handle_post_time(httpd_req_t* req);
    static esp_err_t handle_static(httpd_req_t* req);

    // CORS
    static esp_err_t handle_options(httpd_req_t* req);
    static void set_cors_headers(httpd_req_t* req);
};

} // namespace modesp
