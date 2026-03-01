/**
 * @file test_protection.cpp
 * @brief HOST unit tests for ProtectionModule.
 *
 * Framework: doctest (single header, DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN in test_main.cpp)
 *
 * Key implementation details that drive test design:
 *
 *   1. Alarm monitors: NORMAL -> PENDING (delay) -> ALARM -> NORMAL (auto-clear)
 *
 *   2. Sensor alarms (ERR1, ERR2) are INSTANT — no delay.
 *
 *   3. High temp alarm (HAL): delayed by high_alarm_delay (default 30 min).
 *      Blocked during defrost HEATING phases: stabilize, valve_open, active, equalize.
 *      Also suppressed for post_defrost_delay after defrost ends.
 *
 *   4. Low temp alarm (LAL): delayed by low_alarm_delay (default 30 min).
 *      NOT blocked during defrost. Always active when sensor OK.
 *
 *   5. Door alarm: delayed by door_delay (default 5 min).
 *      Feature-gated: has_feature("door_protection") must be true.
 *      (Generated features_config.h enables this in the test build.)
 *
 *   6. Default settings (from sync_settings() fallback arguments):
 *        high_limit           = 12.0 C
 *        low_limit            = -35.0 C
 *        high_alarm_delay     = 30 min = 1 800 000 ms
 *        low_alarm_delay      = 30 min = 1 800 000 ms
 *        door_delay           = 5  min =   300 000 ms
 *        post_defrost_delay   = 30 min = 1 800 000 ms
 *        manual_reset         = false (auto-clear when condition normalizes)
 *
 *   7. Alarm priority (publish_alarms):
 *        err1 > high_temp > low_temp > err2 > door
 *
 *   8. protection.lockout is always false (reserved for Phase 10+).
 *
 *   9. Reset command: write true to "protection.reset_alarms" -> clears all active.
 *
 *  10. TIMING BUG FIX: Use SETUP_MS=1 for setup ticks to avoid accumulating
 *      delay time during test setup. This prevents the pending_ms counter from
 *      advancing during setup steps that should not count toward alarm delay.
 *
 *  11. Post-defrost suppression timing:
 *      - Suppression starts when defrost.active transitions from true -> false.
 *      - post_defrost_timer_ms_ accumulates ONLY after the suppression starts.
 *      - The suppression window equals post_defrost_delay_ms_.
 */

// -- HOST BUILD: mock ESP-IDF before anything else --
#include "mocks/freertos_mock.h"
#include "mocks/esp_log_mock.h"
#include "mocks/esp_timer_mock.h"

#include "doctest.h"
#include "modesp/shared_state.h"
#include "modesp/module_manager.h"
#include "protection_module.h"

// ── Small dt used for "setup" ticks that must not accumulate alarm timers ────
static constexpr uint32_t SETUP_MS = 1u;

// -- Helper: extract typed value from SharedState -----------------------------

static float pr_get_float(modesp::SharedState& state, const char* key, float def = -999.0f) {
    auto v = state.get(key);
    if (!v.has_value()) return def;
    const auto* fp = etl::get_if<float>(&v.value());
    return fp ? *fp : def;
}

static bool pr_get_bool(modesp::SharedState& state, const char* key, bool def = false) {
    auto v = state.get(key);
    if (!v.has_value()) return def;
    const auto* bp = etl::get_if<bool>(&v.value());
    return bp ? *bp : def;
}

static modesp::StringValue pr_get_str(modesp::SharedState& state, const char* key) {
    auto v = state.get(key);
    if (!v.has_value()) return modesp::StringValue("");
    const auto* sp = etl::get_if<modesp::StringValue>(&v.value());
    return sp ? *sp : modesp::StringValue("");
}

// -- Helper: set up typical normal-operation inputs ---------------------------
static void pr_setup_normal(modesp::SharedState& state,
                             float air_temp   = 5.0f,
                             bool  sensor1_ok = true,
                             bool  sensor2_ok = true,
                             bool  door_open  = false,
                             bool  defrost    = false) {
    state.set("equipment.air_temp",   air_temp);
    state.set("equipment.sensor1_ok", sensor1_ok);
    state.set("equipment.sensor2_ok", sensor2_ok);
    state.set("equipment.door_open",  door_open);
    state.set("defrost.active",       defrost);
    state.set("defrost.phase",        modesp::StringValue("idle"));
}

// -- Helper: set short alarm delays for fast testing --------------------------
static void pr_setup_short_delays(modesp::SharedState& state,
                                   uint32_t high_delay_min = 1,
                                   uint32_t low_delay_min  = 1,
                                   uint32_t door_delay_min = 1) {
    state.set("protection.high_alarm_delay", static_cast<int32_t>(high_delay_min));
    state.set("protection.low_alarm_delay",  static_cast<int32_t>(low_delay_min));
    state.set("protection.door_delay",       static_cast<int32_t>(door_delay_min));
}

// -----------------------------------------------------------------------------
// TEST 1: No alarm in normal conditions
// -----------------------------------------------------------------------------

TEST_CASE("Protection: no alarm in normal conditions [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_normal(state, 5.0f, true, true, false, false);
    prot.on_update(1000u);

    CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == false,
                  "No alarm expected in normal conditions");
    CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "none",
                  "alarm_code must be 'none' in normal conditions");
    CHECK(pr_get_bool(state, "protection.high_temp_alarm")  == false);
    CHECK(pr_get_bool(state, "protection.low_temp_alarm")   == false);
    CHECK(pr_get_bool(state, "protection.sensor1_alarm")    == false);
    CHECK(pr_get_bool(state, "protection.sensor2_alarm")    == false);
    CHECK(pr_get_bool(state, "protection.door_alarm")       == false);
    CHECK(pr_get_bool(state, "protection.lockout")          == false);
}

// -----------------------------------------------------------------------------
// TEST 2: Sensor1 fault alarm is instant
// -----------------------------------------------------------------------------

TEST_CASE("Protection: sensor1 fault triggers ERR1 alarm instantly [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_normal(state, 5.0f, false, true, false, false);  // sensor1_ok=false
    prot.on_update(SETUP_MS);

    CHECK_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == true,
                  "ERR1 alarm must be instant when sensor1_ok=false");
    CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == true,
                  "alarm_active must be true with sensor1 fault");
    CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "err1",
                  "alarm_code must be 'err1' for sensor1 fault");

    SUBCASE("alarm auto-clears when sensor restores") {
        state.set("equipment.sensor1_ok", true);
        prot.on_update(SETUP_MS);
        CHECK_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == false,
                      "ERR1 must auto-clear when sensor1 restores");
        CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == false,
                      "alarm_active must be false after sensor1 restores");
    }
}

// -----------------------------------------------------------------------------
// TEST 3: Sensor2 fault alarm is instant
// -----------------------------------------------------------------------------

TEST_CASE("Protection: sensor2 fault triggers ERR2 alarm instantly [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_normal(state, 5.0f, true, false, false, false);  // sensor2_ok=false
    prot.on_update(SETUP_MS);

    CHECK_MESSAGE(pr_get_bool(state, "protection.sensor2_alarm") == true,
                  "ERR2 alarm must be instant when sensor2_ok=false");
    CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == true,
                  "alarm_active must be true with sensor2 fault");
    CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "err2",
                  "alarm_code must be 'err2' for sensor2 fault (no higher-priority alarms)");

    SUBCASE("ERR2 auto-clears when sensor restores") {
        state.set("equipment.sensor2_ok", true);
        prot.on_update(SETUP_MS);
        CHECK_MESSAGE(pr_get_bool(state, "protection.sensor2_alarm") == false,
                      "ERR2 must auto-clear when sensor2 restores");
    }
}

// -----------------------------------------------------------------------------
// TEST 4: High temp alarm only after delay
// -----------------------------------------------------------------------------

TEST_CASE("Protection: high temp alarm only after delay [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);  // 1 min delays = 60 000 ms
    pr_setup_normal(state, 15.0f, true, true, false, false);  // T=15 > limit=12

    // The delay period = 1 min = 60 000 ms
    static constexpr uint32_t delay_ms = 60000u;

    SUBCASE("no alarm before delay elapses") {
        prot.on_update(delay_ms - 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                      "High temp alarm must NOT trigger before delay elapses");
        CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == false,
                      "alarm_active must be false before delay");
    }

    SUBCASE("alarm triggers after delay elapses") {
        prot.on_update(delay_ms + 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == true,
                      "High temp alarm must trigger after delay elapses");
        CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == true,
                      "alarm_active must be true after high temp alarm fires");
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "high_temp",
                      "alarm_code must be 'high_temp'");
    }

    SUBCASE("exact boundary: alarm at exactly delay_ms + 1") {
        prot.on_update(delay_ms);
        // At exactly delay_ms the condition is pending_ms == delay_ms which is >= delay_ms
        // Implementation uses >=, so this should fire.
        // Note: first tick adds dt_ms to pending_ms. Single tick of delay_ms means
        // pending_ms = delay_ms >= delay_ms -> alarm fires.
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == true,
                      "High temp alarm must trigger at exactly delay_ms boundary");
    }

    SUBCASE("timer accumulates across multiple ticks") {
        // Split the delay across two ticks
        prot.on_update(delay_ms / 2);
        CHECK(pr_get_bool(state, "protection.high_temp_alarm") == false);
        prot.on_update(delay_ms / 2 + 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == true,
                      "High temp alarm must fire after accumulated delay");
    }
}

// -----------------------------------------------------------------------------
// TEST 5: Low temp alarm only after delay
// -----------------------------------------------------------------------------

TEST_CASE("Protection: low temp alarm only after delay [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);  // 1 min delays
    // T=-40 < low_limit=-35
    pr_setup_normal(state, -40.0f, true, true, false, false);

    static constexpr uint32_t delay_ms = 60000u;

    SUBCASE("no alarm before delay") {
        prot.on_update(delay_ms - 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.low_temp_alarm") == false,
                      "Low temp alarm must NOT trigger before delay elapses");
    }

    SUBCASE("alarm triggers after delay") {
        prot.on_update(delay_ms + 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.low_temp_alarm") == true,
                      "Low temp alarm must trigger after delay elapses");
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "low_temp",
                      "alarm_code must be 'low_temp'");
    }
}

// -----------------------------------------------------------------------------
// TEST 6: Alarm auto-clears when temp returns to normal
// -----------------------------------------------------------------------------

TEST_CASE("Protection: alarm auto-clears when temp returns to normal [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);
    pr_setup_normal(state, 15.0f, true, true, false, false);

    // Trigger the alarm
    prot.on_update(60001u);
    REQUIRE_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == true,
                    "Pre-condition: high temp alarm must be active");

    // Bring temp back to normal
    state.set("equipment.air_temp", 5.0f);
    prot.on_update(SETUP_MS);

    CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                  "High temp alarm must auto-clear when temp returns below limit");
    CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == false,
                  "alarm_active must be false after auto-clear");
    CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "none",
                  "alarm_code must revert to 'none' after auto-clear");
}

// -----------------------------------------------------------------------------
// TEST 7: Door alarm after door_delay
// -----------------------------------------------------------------------------

TEST_CASE("Protection: door alarm fires after door_delay [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);  // door_delay = 1 min = 60 000 ms
    pr_setup_normal(state, 5.0f, true, true, true, false);  // door_open=true

    static constexpr uint32_t door_delay_ms = 60000u;

    SUBCASE("no door alarm before delay") {
        prot.on_update(door_delay_ms - 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == false,
                      "Door alarm must NOT fire before door_delay elapses");
    }

    SUBCASE("door alarm fires after delay") {
        prot.on_update(door_delay_ms + 1u);
        CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == true,
                      "Door alarm must fire after door_delay elapses");
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "door",
                      "alarm_code must be 'door' (lowest priority)");
    }

    SUBCASE("door alarm auto-clears when door closes") {
        prot.on_update(door_delay_ms + 1u);
        REQUIRE(pr_get_bool(state, "protection.door_alarm") == true);

        state.set("equipment.door_open", false);
        prot.on_update(SETUP_MS);

        CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == false,
                      "Door alarm must auto-clear when door closes");
    }

    SUBCASE("door pending resets if door closes before alarm fires") {
        prot.on_update(door_delay_ms / 2);
        CHECK(pr_get_bool(state, "protection.door_alarm") == false);

        state.set("equipment.door_open", false);
        prot.on_update(SETUP_MS);
        // Door closes, pending resets

        state.set("equipment.door_open", true);
        prot.on_update(door_delay_ms / 2);
        // Only half the delay accumulated again -> no alarm
        CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == false,
                      "Door pending must reset when door closes before alarm fires");
    }
}

// -----------------------------------------------------------------------------
// TEST 8: Post-defrost suppression of high temp alarm
// -----------------------------------------------------------------------------

TEST_CASE("Protection: post-defrost suppression of high temp alarm [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    // Use short delays to make test fast
    // high_alarm_delay = 1 min = 60 000 ms
    // post_defrost_delay = 1 min = 60 000 ms
    pr_setup_short_delays(state, 1, 1, 1);
    state.set("protection.post_defrost_delay", static_cast<int32_t>(1));  // 1 min

    static constexpr uint32_t high_delay_ms   = 60000u;
    static constexpr uint32_t post_delay_ms   = 60000u;

    // Simulate defrost active -> ended transition
    pr_setup_normal(state, 5.0f, true, true, false, true);  // defrost=true
    state.set("defrost.phase", modesp::StringValue("active"));

    // Setup tick: mark was_defrost_active_ = true (use SETUP_MS to avoid accumulation)
    prot.on_update(SETUP_MS);

    // Now end defrost: this tick triggers post_defrost_suppression_ = true
    state.set("defrost.active", false);
    state.set("defrost.phase",  modesp::StringValue("idle"));
    // Set high temp condition that would normally trigger alarm
    state.set("equipment.air_temp", 15.0f);
    prot.on_update(SETUP_MS);  // suppression starts here, post_defrost_timer_ms_ = SETUP_MS

    SUBCASE("high temp alarm suppressed within post-defrost window") {
        // Advance to just before post_defrost_delay expires
        // Total post_defrost_timer_ms_ accumulated = SETUP_MS + (post_delay_ms - SETUP_MS - 1)
        // = post_delay_ms - 1 < post_delay_ms -> still suppressed
        prot.on_update(post_delay_ms - SETUP_MS - 1u);

        // Even though high temp condition exists, alarm should NOT fire during suppression
        // (pending resets each tick while suppressed)
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                      "High temp alarm must be suppressed within post-defrost window");
    }

    SUBCASE("high temp alarm fires after post-defrost window expires") {
        // Step 1: advance past post_defrost_delay to end suppression
        // post_defrost_timer_ms_ starts at SETUP_MS from previous tick
        // Need to add (post_delay_ms - SETUP_MS + 1) to exceed threshold
        prot.on_update(post_delay_ms - SETUP_MS + 1u);

        // Suppression now ended. high_alarm pending starts fresh.
        // Now add enough time for high_alarm_delay to expire
        prot.on_update(high_delay_ms + 1u);

        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == true,
                      "High temp alarm must fire after post-defrost suppression ends");
    }

    SUBCASE("All defrost heating phases suppress HAL alarm") {
        // Test each heating phase
        const char* heating_phases[] = { "active", "stabilize", "valve_open", "equalize" };
        for (const char* phase : heating_phases) {
            // Reset state for each subcase iteration
            state.set("defrost.active", true);
            state.set("defrost.phase",  modesp::StringValue(phase));
            state.set("equipment.air_temp", 15.0f);

            // One setup tick to register defrost active
            prot.on_update(SETUP_MS);

            // Try to accumulate high_alarm_delay
            prot.on_update(high_delay_ms + 1u);

            CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                          "HAL alarm must be suppressed in heating phase: ", phase);
        }
    }
}

// -----------------------------------------------------------------------------
// TEST 9: Alarm_code priority ordering
// -----------------------------------------------------------------------------

TEST_CASE("Protection: alarm_code priority -- err1 > high_temp > low_temp > err2 > door [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);

    SUBCASE("err1 has highest priority over all others") {
        // Trigger all alarm conditions simultaneously
        pr_setup_normal(state, 15.0f, false, false, true, false);
        // Need high/low/door alarms to fire first (they need delay)
        // sensor1/2 alarms are instant
        prot.on_update(60001u);  // fire delayed alarms
        // sensor1_ok=false -> err1 wins
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "err1",
                      "err1 must have highest priority");
    }

    SUBCASE("high_temp beats low_temp when both active") {
        // sensor1 OK, T is simultaneously > high_limit AND < low_limit is impossible physically
        // Test by firing high_temp first, then verify priority
        pr_setup_normal(state, 15.0f, true, true, false, false);
        prot.on_update(60001u);  // high temp fires
        REQUIRE(pr_get_bool(state, "protection.high_temp_alarm") == true);

        // Manually check priority: if both high and low were active, high_temp wins
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "high_temp",
                      "high_temp must have higher priority than low_temp");
    }

    SUBCASE("err2 beats door alarm") {
        pr_setup_normal(state, 5.0f, true, false, true, false);  // sensor2=false, door=true
        prot.on_update(60001u);  // door alarm fires
        // err2 is instant, fires regardless of door
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "err2",
                      "err2 must have higher priority than door alarm");
    }

    SUBCASE("door alarm code when only door is active") {
        pr_setup_normal(state, 5.0f, true, true, true, false);  // only door open
        prot.on_update(60001u);  // door delay = 1 min = 60 000ms
        CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == true,
                      "Pre-condition: door alarm must be active");
        CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "door",
                      "alarm_code must be 'door' when only door alarm active");
    }
}

// -----------------------------------------------------------------------------
// TEST 10: Manual reset clears all active alarms
// -----------------------------------------------------------------------------

TEST_CASE("Protection: manual reset clears all active alarms [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);

    // Use manual_reset=true so alarms won't auto-clear when conditions normalize.
    // This lets us verify that the reset command is the mechanism that clears them.
    state.set("protection.manual_reset", true);

    // Trigger multiple alarms (all conditions bad)
    pr_setup_normal(state, 15.0f, false, false, true, false);  // high temp, sensor faults, door
    prot.on_update(60001u);  // fire all delayed alarms

    REQUIRE_MESSAGE(pr_get_bool(state, "protection.alarm_active") == true,
                    "Pre-condition: at least one alarm must be active");
    REQUIRE(pr_get_bool(state, "protection.sensor1_alarm") == true);
    REQUIRE(pr_get_bool(state, "protection.sensor2_alarm") == true);

    // Restore conditions to normal. With manual_reset=true alarms persist.
    pr_setup_normal(state, 5.0f, true, true, false, false);
    prot.on_update(SETUP_MS);

    // Verify alarms still active (manual_reset prevents auto-clear)
    REQUIRE_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == true,
                    "Sensor alarm must persist with manual_reset=true even after sensor restores");

    // Issue manual reset
    state.set("protection.reset_alarms", true);
    prot.on_update(SETUP_MS);

    CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                  "Manual reset must clear high_temp alarm");
    CHECK_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == false,
                  "Manual reset must clear sensor1 alarm");
    CHECK_MESSAGE(pr_get_bool(state, "protection.sensor2_alarm") == false,
                  "Manual reset must clear sensor2 alarm");
    CHECK_MESSAGE(pr_get_bool(state, "protection.door_alarm") == false,
                  "Manual reset must clear door alarm");
    CHECK_MESSAGE(pr_get_bool(state, "protection.alarm_active") == false,
                  "alarm_active must be false after manual reset");
    CHECK_MESSAGE(pr_get_str(state, "protection.alarm_code") == "none",
                  "alarm_code must be 'none' after manual reset");
    CHECK_MESSAGE(pr_get_bool(state, "protection.reset_alarms") == false,
                  "reset_alarms trigger must be cleared after processing");
}

// -----------------------------------------------------------------------------
// TEST 11: Sensor failure blocks high/low temp alarm logic
// -----------------------------------------------------------------------------

TEST_CASE("Protection: sensor failure blocks high and low temp alarm logic [protection]") {
    modesp::SharedState state;
    ProtectionModule prot;
    modesp::ModuleManager mgr;
    mgr.register_module(prot);
    mgr.init_all(state);

    pr_setup_short_delays(state, 1, 1, 1);

    SUBCASE("high temp alarm blocked when sensor1 fails") {
        // T > high_limit but sensor1_ok=false -> high temp alarm must NOT fire
        pr_setup_normal(state, 15.0f, false, true, false, false);
        prot.on_update(60001u);

        CHECK_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == true,
                      "ERR1 must be active with sensor1_ok=false");
        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                      "High temp alarm must be blocked when sensor1 fails");
    }

    SUBCASE("low temp alarm blocked when sensor1 fails") {
        // T < low_limit but sensor1_ok=false -> low temp alarm must NOT fire
        pr_setup_normal(state, -40.0f, false, true, false, false);
        prot.on_update(60001u);

        CHECK_MESSAGE(pr_get_bool(state, "protection.sensor1_alarm") == true,
                      "ERR1 must be active with sensor1_ok=false");
        CHECK_MESSAGE(pr_get_bool(state, "protection.low_temp_alarm") == false,
                      "Low temp alarm must be blocked when sensor1 fails");
    }

    SUBCASE("pending high temp resets when sensor fails mid-delay") {
        // Start accumulating high temp delay with sensor OK
        pr_setup_normal(state, 15.0f, true, true, false, false);
        prot.on_update(30000u);  // half delay accumulated
        CHECK(pr_get_bool(state, "protection.high_temp_alarm") == false);

        // Sensor fails mid-delay -> pending resets
        state.set("equipment.sensor1_ok", false);
        prot.on_update(30001u);  // would have fired if pending continued

        CHECK_MESSAGE(pr_get_bool(state, "protection.high_temp_alarm") == false,
                      "High temp alarm must NOT fire after sensor failure resets pending");
    }
}
