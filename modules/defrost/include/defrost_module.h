/**
 * @file defrost_module.h
 * @brief Defrost cycle controller — 3 types, 7-phase state machine (spec_v3 §3)
 *
 * Defrost types (dFT):
 *   0 — Natural: compressor OFF, all OFF
 *   1 — Heater: heater ON, compressor OFF
 *   2 — Hot Gas: 7-phase sequence with valve and equalize
 *
 * State machine:
 *   IDLE → [dFT=2] STABILIZE → VALVE_OPEN → ACTIVE → EQUALIZE → DRIP → FAD → IDLE
 *   IDLE → [dFT=0/1]                         ACTIVE              → DRIP → FAD → IDLE
 *
 * Initiation (defrost.initiation):
 *   0 — Timer (dit hours)
 *   1 — Demand (T_evap < dSS)
 *   2 — Combined (first of timer OR demand)
 *   3 — Disabled
 *
 * Counter modes (defrost.counter_mode):
 *   1 — Real time (always counting)
 *   2 — Compressor run time (counts only when compressor ON)
 *
 * Priority: NORMAL(2) — after Protection(1), alongside Thermostat(2)
 *
 * SharedState keys read:
 *   equipment.air_temp      — float (logging)
 *   equipment.evap_temp     — float (termination + demand)
 *   equipment.sensor2_ok    — bool  (evap sensor health)
 *   equipment.compressor    — bool  (for dct=2 counter)
 *   protection.lockout      — bool  (abort defrost on lockout)
 *
 * SharedState keys written:
 *   defrost.active              — bool (main signal for EM/Thermostat/Protection)
 *   defrost.phase               — string ("idle"/"stabilize"/"valve_open"/"active"/"equalize"/"drip"/"fad")
 *   defrost.state               — string (human-readable UI state)
 *   defrost.phase_timer         — int32 (seconds in current phase)
 *   defrost.interval_timer      — int32 (seconds to next defrost, persist)
 *   defrost.defrost_count       — int32 (total completed defrosts, persist)
 *   defrost.last_termination    — string ("temp" or "timeout")
 *   defrost.consecutive_timeouts — int32 (consecutive safety-timer terminations)
 *   defrost.req.compressor      — bool
 *   defrost.req.heater          — bool
 *   defrost.req.evap_fan        — bool
 *   defrost.req.cond_fan        — bool
 *   defrost.req.hg_valve        — bool
 */

#pragma once

#include "modesp/base_module.h"

class DefrostModule : public modesp::BaseModule {
public:
    DefrostModule();

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_stop() override;

private:
    // Фази state machine (spec_v3 §3.4)
    enum class Phase {
        IDLE,           // Не в розморозці — чекаємо тригеру
        STABILIZE,      // dFT=2 тільки: вирівнювання тиску (фаза 1)
        VALVE_OPEN,     // dFT=2 тільки: клапан ГГ відкривається (фаза 2)
        ACTIVE,         // Активна відтайка (фаза 3) — для всіх dFT
        EQUALIZE,       // dFT=2 тільки: пауза після відтайки (фаза 4)
        DRIP,           // Дренаж (фаза 5)
        FAD,            // Fan After Defrost (фаза 6)
    };

    Phase phase_ = Phase::IDLE;

    // Хелпери читання з SharedState
    float   read_float(const char* key, float def = 0.0f);
    bool    read_bool(const char* key, bool def = false);
    int32_t read_int(const char* key, int32_t def = 0);
    void    sync_settings();

    // Фаза відтайки
    void enter_phase(Phase p);
    const char* phase_name() const;
    const char* phase_state_label() const;

    // Ініціація
    void update_idle(uint32_t dt_ms);
    bool check_timer_trigger() const;
    bool check_demand_trigger() const;
    void start_defrost(const char* reason);

    // Обробники фаз
    void update_stabilize(uint32_t dt_ms);
    void update_valve_open(uint32_t dt_ms);
    void update_active_phase(uint32_t dt_ms);
    void update_equalize(uint32_t dt_ms);
    void update_drip(uint32_t dt_ms);
    void update_fad(uint32_t dt_ms);

    // Завершення
    void finish_active_phase(const char* reason);
    void finish_defrost();

    // Запити до Equipment Manager
    void set_requests(bool comp, bool heater, bool evap_fan, bool cond_fan, bool hg_valve);
    void clear_requests();

    // Публікація стану в SharedState
    void publish_state();

    // === Налаштування (з SharedState, persist) ===
    int32_t  defrost_type_    = 0;          // dFT: 0=зупинка, 1=тен, 2=ГГ
    uint32_t interval_ms_     = 28800000;   // dit: 8h → мс
    int32_t  counter_mode_    = 1;          // dct: 1=реальний час, 2=час компресора
    int32_t  initiation_      = 0;          // 0=таймер, 1=demand, 2=combo, 3=вимкнено
    float    end_temp_        = 8.0f;       // dSt °C — завершення по T_evap
    uint32_t max_duration_ms_ = 1800000;    // dEt: 30 хв → мс
    float    demand_temp_     = -25.0f;     // dSS °C — поріг demand
    uint32_t drip_time_ms_    = 120000;     // dPt: 120 с → мс
    uint32_t fan_delay_ms_    = 120000;     // FAd: 120 с → мс
    float    fad_temp_        = -5.0f;      // FAT °C — завершення FAD
    uint32_t stabilize_ms_    = 30000;      // Фаза 1: 30 с
    uint32_t valve_delay_ms_  = 3000;       // Фаза 2: 3 с
    uint32_t equalize_ms_     = 90000;      // Фаза 4: 90 с

    // === Runtime стан ===
    uint32_t phase_timer_ms_    = 0;   // Час в поточній фазі
    uint32_t interval_timer_ms_ = 0;   // Лічильник до наступної відтайки (persist)
    int32_t  defrost_count_     = 0;   // Кількість відтайок (persist)
    int32_t  consecutive_timeouts_ = 0; // Послідовні завершення по таймеру

    // Кеш вхідних сигналів
    float evap_temp_     = 0.0f;
    bool  sensor2_ok_    = false;
    bool  compressor_on_ = false;

    // Прапор завантаження persist значень з SharedState при boot
    bool settings_loaded_ = false;
};
