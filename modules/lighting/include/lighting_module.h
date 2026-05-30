/**
 * @file lighting_module.h
 * @brief Lighting control — OFF/ON/Auto/Schedule + door trigger, auto-off, override
 *
 * Base modes (lighting.mode):
 *   0 = OFF      — світло завжди вимкнено
 *   1 = ON       — світло завжди ввімкнено
 *   2 = AUTO     — день=ON, ніч=OFF (thermostat.night_active)
 *   3 = SCHEDULE — за годинним вікном [sched_start, sched_end)
 *
 * Modifiers (layered over base mode):
 *   door_trigger  — відкриття дверей (equipment.door_open) вмикає світло
 *   auto_off_min  — таймаут авто-вимкнення для transient-ON (двері/override), 0=вимк
 *   override      — ручне перекриття 0=Авто / 1=ON / 2=OFF (авто-повернення)
 *
 * Resolution priority (highest → lowest):
 *   override(1/2) > door open > door-linger (after close) > base mode
 *
 * Equipment Layer integration:
 *   Lighting НЕ доступається до HAL напряму. Публікує lighting.req.light →
 *   Equipment читає й застосовує до реле. Світло незалежне від арбітражу.
 */

#pragma once

#include "modesp/base_module.h"

class LightingModule : public modesp::BaseModule {
public:
    LightingModule();

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_stop() override;

    /// Чи година `hour` всередині вікна [start, end). Обробляє перехід через
    /// північ (start > end). start == end → порожнє вікно (завжди false).
    /// Pure helper — без стану, для unit-тестів.
    static bool in_schedule(int hour, int start, int end);

private:
    bool compute_base();   ///< OFF/ON/AUTO/SCHEDULE → бажаний усталений стан
    int  current_hour();   ///< Поточна година (time()/localtime_r)

    // Settings (синхронізуються щооновлення)
    int32_t mode_         = 0;
    bool    door_trigger_ = false;
    int32_t auto_off_min_ = 0;
    int32_t sched_start_  = 6;
    int32_t sched_end_    = 22;
    int32_t override_     = 0;

    // Runtime
    bool     light_request_  = false;  // Поточний запит до Equipment
    bool     last_actual_    = false;  // Кеш для уникнення version bump
    uint32_t door_linger_ms_ = 0;      // Зворотний відлік після закриття дверей
    uint32_t override_ms_    = 0;      // Час з моменту активації override
};
