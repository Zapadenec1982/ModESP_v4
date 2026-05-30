/**
 * @file test_lighting.cpp
 * @brief HOST unit tests for LightingModule.
 *
 * Покриває: базові режими (OFF/ON/AUTO), door trigger, auto-off linger,
 * manual override + авто-повернення, дзеркалення equipment.light,
 * та чистий помічник in_schedule(). Режим SCHEDULE з реальним годинником
 * тестується лише через in_schedule() (host не контролює wall-clock).
 */

// -- HOST BUILD: mock ESP-IDF before anything else --
#include "mocks/freertos_mock.h"
#include "mocks/esp_log_mock.h"
#include "mocks/esp_timer_mock.h"

#include "doctest.h"
#include "modesp/shared_state.h"
#include "modesp/module_manager.h"
#include "lighting_module.h"

static constexpr uint32_t SETUP_MS = 1u;
static constexpr uint32_t MIN_MS   = 60000u;

static bool lg_get_bool(modesp::SharedState& s, const char* key, bool def = false) {
    auto v = s.get(key);
    if (!v.has_value()) return def;
    const auto* bp = etl::get_if<bool>(&v.value());
    return bp ? *bp : def;
}

static int32_t lg_get_int(modesp::SharedState& s, const char* key, int32_t def = -999) {
    auto v = s.get(key);
    if (!v.has_value()) return def;
    const auto* ip = etl::get_if<int32_t>(&v.value());
    return ip ? *ip : def;
}

// Готує модуль у RUNNING зі заданим режимом.
struct LightFixture {
    modesp::SharedState state;
    modesp::ModuleManager mgr;
    LightingModule light;

    LightFixture() {
        mgr.register_module(light);
        mgr.init_all(state);
    }
    void set_mode(int m)        { state.set("lighting.mode", (int32_t)m); }
    void set_override(int o)    { state.set("lighting.override", (int32_t)o); }
    void set_auto_off(int min)  { state.set("lighting.auto_off_min", (int32_t)min); }
    void door(bool open)        { state.set("equipment.door_open", open); }
    void tick(uint32_t ms = SETUP_MS) { light.on_update(ms); }
    bool req()  { return lg_get_bool(state, "lighting.req.light"); }
};

// ── in_schedule (pure) ─────────────────────────────────────────

TEST_CASE("Lighting: in_schedule window [lighting]") {
    // Звичайне вікно [6, 22)
    CHECK(LightingModule::in_schedule(10, 6, 22) == true);
    CHECK(LightingModule::in_schedule(6,  6, 22) == true);   // межа start включна
    CHECK(LightingModule::in_schedule(22, 6, 22) == false);  // межа end виключна
    CHECK(LightingModule::in_schedule(5,  6, 22) == false);
    CHECK(LightingModule::in_schedule(23, 6, 22) == false);

    // Вікно через північ [22, 6)
    CHECK(LightingModule::in_schedule(23, 22, 6) == true);
    CHECK(LightingModule::in_schedule(3,  22, 6) == true);
    CHECK(LightingModule::in_schedule(22, 22, 6) == true);
    CHECK(LightingModule::in_schedule(6,  22, 6) == false);
    CHECK(LightingModule::in_schedule(12, 22, 6) == false);

    // Порожнє вікно
    CHECK(LightingModule::in_schedule(8, 8, 8) == false);
}

// ── Базові режими ──────────────────────────────────────────────

TEST_CASE_FIXTURE(LightFixture, "Lighting: mode OFF/ON [lighting]") {
    set_mode(0); tick();
    CHECK(req() == false);
    set_mode(1); tick();
    CHECK(req() == true);
    set_mode(0); tick();
    CHECK(req() == false);
}

TEST_CASE_FIXTURE(LightFixture, "Lighting: mode AUTO follows night_active [lighting]") {
    set_mode(2);
    state.set("thermostat.night_active", false);  // день
    tick();
    CHECK(req() == true);
    state.set("thermostat.night_active", true);    // ніч
    tick();
    CHECK(req() == false);
}

// ── Door trigger ───────────────────────────────────────────────

TEST_CASE_FIXTURE(LightFixture, "Lighting: door trigger, no timeout [lighting]") {
    set_mode(0);                       // базово OFF
    state.set("lighting.door_trigger", true);
    set_auto_off(0);                   // без таймауту

    door(true);  tick();
    CHECK_MESSAGE(req() == true, "Двері відкрито → світло ON");

    door(false); tick();
    CHECK_MESSAGE(req() == false, "Двері закрито, без linger → одразу OFF");
}

TEST_CASE_FIXTURE(LightFixture, "Lighting: door linger after close [lighting]") {
    set_mode(0);
    state.set("lighting.door_trigger", true);
    set_auto_off(1);                   // 1 хв linger

    door(true);  tick();
    REQUIRE(req() == true);

    door(false);
    tick(30u * 1000u);                 // 30 c після закриття — ще горить
    CHECK_MESSAGE(req() == true, "Протягом linger світло тримається");

    tick(31u * 1000u);                 // сумарно >60 c — гасне
    CHECK_MESSAGE(req() == false, "Після linger таймауту → OFF");
}

// ── Manual override ────────────────────────────────────────────

TEST_CASE_FIXTURE(LightFixture, "Lighting: override beats base mode [lighting]") {
    set_mode(0);                       // базово OFF
    set_override(1);                   // примусово ON
    tick();
    CHECK(req() == true);

    set_mode(1);                       // базово ON
    set_override(2);                   // примусово OFF
    tick();
    CHECK(req() == false);
}

TEST_CASE_FIXTURE(LightFixture, "Lighting: override auto-reverts after timeout [lighting]") {
    set_mode(0);                       // база OFF
    set_auto_off(1);                   // 1 хв
    set_override(1);                   // примусово ON
    tick();
    REQUIRE(req() == true);

    tick(MIN_MS + 1u);                 // понад таймаут
    CHECK_MESSAGE(lg_get_int(state, "lighting.override") == 0,
                  "override повертається в Авто");
    CHECK_MESSAGE(req() == false, "Після повернення — базовий режим (OFF)");
}

TEST_CASE_FIXTURE(LightFixture, "Lighting: override latches when timeout disabled [lighting]") {
    set_mode(0);
    set_auto_off(0);                   // без таймауту
    set_override(1);
    tick();
    tick(10u * MIN_MS);                // багато часу
    CHECK_MESSAGE(req() == true, "Без таймауту override тримається");
    CHECK(lg_get_int(state, "lighting.override") == 1);
}

// ── Дзеркалення фактичного стану ───────────────────────────────

TEST_CASE_FIXTURE(LightFixture, "Lighting: mirrors equipment.light → lighting.state [lighting]") {
    state.set("equipment.light", true);
    tick();
    CHECK(lg_get_bool(state, "lighting.state") == true);
    state.set("equipment.light", false);
    tick();
    CHECK(lg_get_bool(state, "lighting.state") == false);
}
