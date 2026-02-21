# ModESP v4 — BUGFIXES

> Independently verified code review by Claude Code
> Date: 2026-02-19
> Codebase revision: Phase 9.4 (4 modules, 69 state keys)
> Cross-referenced with: `docs/business_logic_review.md`, `docs/architecture_review.md`

---

## Severity Levels

| Level | Meaning |
|-------|---------|
| **SEV-1** | Data corruption, hardware damage, silent failure |
| **SEV-2** | Incorrect behavior, spec violation, wrong output |
| **SEV-3** | Code quality, maintainability, future risk |
| **SEV-4** | Minor, cosmetic, optimization |

---

## SEV-1 — Critical

### BUG-001: StateKey overflow — silent data truncation for keys > 24 chars

**Files:** All modules, `components/modesp_core/include/modesp/types.h:48`
**Root cause:** `StateKey = etl::string<24>` (max 24 characters including null terminator = 23 usable chars). Multiple keys exceed this limit:

| Key | Length | Truncated to |
|-----|--------|--------------|
| `thermostat.req.compressor` | 25 | `thermostat.req.compress` |
| `thermostat.cond_fan_delay` | 25 | `thermostat.cond_fan_dela` |
| `thermostat.safety_run_off` | 25 | `thermostat.safety_run_of` |
| `protection.high_temp_alarm` | 26 | `protection.high_temp_ala` |
| `protection.low_temp_alarm` | 25 | `protection.low_temp_alar` |
| `protection.sensor1_alarm` | 24 | OK (fits exactly) |
| `protection.sensor2_alarm` | 24 | OK (fits exactly) |
| `defrost.consecutive_timeouts` | 28 | `defrost.consecutive_time` |
| `defrost.last_termination` | 24 | OK (fits exactly) |

**Impact:** `etl::string<24>` silently truncates. Module A writes key `"protection.high_temp_alarm"` → stored as `"protection.high_temp_al"`. Module B reads `"protection.high_temp_alarm"` → also truncated to `"protection.high_temp_al"` → **collision works by accident**. But `"protection.high_temp_alarm"` and `"protection.high_temp_alar"` (hypothetical) would collide silently — both map to the same truncated key.

The real danger: `defrost.consecutive_timeouts` (28 chars) → `"defrost.consecutive_tim"`. Any key starting with `"defrost.consecutive_tim"` collides.

**Verification:** `echo -n "thermostat.req.compressor" | wc -c` → 25 (exceeds 24)

**Fix:**
```cpp
// types.h — збільшити MODESP_MAX_KEY_LENGTH
#define MODESP_MAX_KEY_LENGTH 32  // was 24
```
Або перейменувати довгі ключі (breaking change для WebUI/MQTT).

**Priority:** IMMEDIATE — affects data integrity of entire system

---

### BUG-002: Defrost type change during active HG cycle → hydraulic shock

**File:** `modules/defrost/src/defrost_module.cpp:412-418`
**Root cause:** `finish_active_phase()` reads `defrost_type_` which is synced from SharedState every cycle. If operator changes `defrost.type` from 2 (hot gas) to 0 (natural) via WebUI during ACTIVE phase, `finish_active_phase()` skips EQUALIZE and goes directly to DRIP.

**Impact:** Hot gas defrost requires EQUALIZE phase (pressure equalization before compressor restart). Skipping it → compressor starts against high pressure → **hydraulic shock → compressor damage**.

**Fix:**
```cpp
// defrost_module.h — add member:
int32_t active_type_ = 0;  // Тип зафіксований при старті відтайки

// defrost_module.cpp — start_defrost():
void DefrostModule::start_defrost(const char* reason) {
    active_type_ = defrost_type_;  // Зафіксувати
    // ... use active_type_ everywhere in phase transitions
}
```

**Priority:** IMMEDIATE — physical equipment damage risk

---

### BUG-003: Protection module is purely decorative — lockout never activates

**Files:** `modules/protection/src/protection_module.cpp:278`, `modules/thermostat/src/thermostat_module.cpp:222`
**Root cause:** `protection.lockout` is hardcoded to `false` (line 278: `state_set("protection.lockout", false)`). Thermostat only checks `protection.lockout` to decide whether to stop the compressor (line 222).

**Impact:** Alarms (high_temp, low_temp, ERR1) are published to SharedState but **have zero effect on system behavior**. Compressor continues running during high temp alarm (compressor doesn't cool → runs pointlessly → wears out). ERR1 sensor failure doesn't trigger lockout — only Safety Run (via separate sensor1_ok check).

**Fix:** Implement alarm → action matrix:
```cpp
// protection_module.cpp — publish_alarms():
bool lockout = sensor1_.active || high_temp_.active || low_temp_.active;
state_set("protection.lockout", lockout);
```

**Priority:** HIGH — defeats purpose of protection module

---

### BUG-004: Cond fan instant OFF at defrost start — no COd delay

**File:** `modules/thermostat/src/thermostat_module.cpp:230-236`, `modules/equipment/src/equipment_module.cpp:194-207`
**Root cause:** When defrost starts, thermostat does `request_cond_fan(false)` immediately (line 234). Equipment Manager applies defrost requests which also have `cond_fan=false`. The condenser fan delay (`COd`) implemented in thermostat's `update_cond_fan()` is never executed because thermostat returns early when `defrost_active_`.

**Impact:** Condenser fan switches OFF instantly instead of delayed OFF per spec §3.1.1. For systems with large condensers, instant compressor+cond_fan OFF can cause pressure spikes.

**Fix:** Execute cond_fan delay logic before early return in defrost handler, or have Equipment Manager implement COd delay.

**Priority:** HIGH — pressure management issue

---

## SEV-2 — Important

### BUG-005: Thermostat state machine not reset during defrost

**File:** `modules/thermostat/src/thermostat_module.cpp:230-236`
**Root cause:** When defrost activates, thermostat sets requests to false and returns. But `state_` remains COOLING. `comp_on_time_ms_` keeps incrementing via `dt_ms` (line 216) even though compressor is actually OFF (Equipment Manager applies defrost requests).

**Impact:** When defrost ends, `comp_on_time_ms_` is inflated by defrost duration (could be 30+ minutes). `comp_off_time_ms_` is incorrect. Min_on/min_off protection timers are unreliable after defrost.

**Fix:**
```cpp
if (defrost_active_ && !defrost_fad_) {
    if (state_ == State::COOLING) enter_state(State::IDLE);  // Reset SM
    request_compressor(false);
    // ...
}
```

---

### BUG-006: enter_phase() no re-entry guard — timer reset on duplicate call

**File:** `modules/defrost/src/defrost_module.cpp:129-131`
**Root cause:** `enter_phase()` always resets `phase_timer_ms_ = 0` even if already in that phase. No guard `if (phase_ == p) return;`.

**Impact:** If logic bug causes `enter_phase(Phase::ACTIVE)` while already in ACTIVE → safety timer resets → defrost never terminates by timeout → heater runs indefinitely.

**Fix:** Add guard at top of `enter_phase()`:
```cpp
void DefrostModule::enter_phase(Phase p) {
    if (phase_ == p) return;
    // ...
}
```

---

### BUG-007: High temp alarm blocked in ALL defrost phases (spec says only DFR/DRP)

**File:** `modules/protection/src/protection_module.cpp:127-132`
**Root cause:** Protection checks `defrost_active` (true for all non-IDLE phases). Spec §1.3 says high temp alarm should only be blocked during DFR (ACTIVE) and DRP (DRIP).

**Impact:** During FAD phase (compressor ON, cooling), high temp alarm is suppressed. If system doesn't cool during FAD, alarm won't trigger.

**Fix:** Pass defrost phase string or add explicit `defrost.block_high_alarm` bool key set only during ACTIVE and DRIP phases.

---

### BUG-008: Demand defrost has no minimum interval protection

**File:** `modules/defrost/src/defrost_module.cpp:331-334`
**Root cause:** `check_demand_trigger()` only checks `evap_temp_ < demand_temp_`. No minimum time since last defrost.

**Impact:** If evap sensor is faulty or evaporator re-ices quickly, defrost can start immediately after previous one ends → continuous defrost cycling → no cooling → product damage.

**Fix:** Add minimum interval check before demand trigger:
```cpp
bool DefrostModule::check_demand_trigger() {
    if (!sensor2_ok_) return false;
    if (interval_timer_ms_ < MIN_DEMAND_INTERVAL_MS) return false;  // e.g. 30 min
    return evap_temp_ < demand_temp_;
}
```

---

### BUG-009: Thermostat tracks request time, not actual relay time

**File:** `modules/thermostat/src/thermostat_module.cpp:215-219`, `:78-89`
**Root cause:** `comp_on_time_ms_` increments when `compressor_on_` (thermostat's request flag) is true. But relay may reject the request (`RelayDriver::set()` returns false if `min_switch_ms` not elapsed). Thermostat doesn't read `equipment.compressor` (actual state).

**Impact:** `min_on_time` and `min_off_time` protection timers based on request time, not actual time. Compressor may switch before actual minimum interval.

**Fix:** Read `equipment.compressor` for timer tracking:
```cpp
bool actual_comp = read_bool("equipment.compressor");
// Use actual_comp for timer increments
```

---

### BUG-010: 3 consecutive defrost timeouts — log only, no alarm

**File:** `modules/defrost/src/defrost_module.cpp:398-400`
**Root cause:** When `consecutive_timeouts_ >= 3`, only `ESP_LOGW` is called. No state key, no alarm, no notification.

**Impact:** Spec §3.3 requires alarm signaling. Heater or sensor failure is invisible to operator (only in serial monitor).

**Fix:** Set state key + have Protection monitor it:
```cpp
if (consecutive_timeouts_ >= 3) {
    state_set("defrost.timeout_alarm", true);
    ESP_LOGW(TAG, "3 consecutive timeouts — alarm!");
}
```

---

### BUG-021: Race condition on WsService client_fds_[] — no synchronization

**File:** `components/modesp_net/src/ws_service.cpp:56-77`, `:295-327`
**Root cause:** `add_client()` is called from httpd thread (ws_handler on HTTP GET). `send_to_all()`, `cleanup_dead_clients()` are called from main loop (`on_update()`). `client_fds_[]` array has no mutex protection.

**Impact:** On dual-core ESP32, concurrent read/write to client_fds_[] is undefined behavior. Practically low risk (fd is int, atomic on ARM), but can lead to send to wrong/stale fd or missed client registration.

**Fix:** Add spinlock or FreeRTOS mutex around client_fds_[] operations.

---

### BUG-022: Race condition on PersistService dirty_[] and debounce_timer_

**File:** `components/modesp_services/src/persist_service.cpp:97-113`, `:115-132`
**Root cause:** `on_state_changed()` callback is invoked from SharedState::set() OUTSIDE mutex, potentially from httpd thread (POST /api/settings). `on_update()` reads dirty_[] from main loop. No synchronization.

**Impact:** Missed dirty flag or debounce reset on dual-core. A setting change could be delayed or lost until next change.

**Fix:** Use `std::atomic<bool>` for dirty flags, or protect with lightweight spinlock.

---

### BUG-023: HTTP POST /api/settings — no type validation against meta->type

**File:** `components/modesp_net/src/http_service.cpp` (handle_post_settings)
**Root cause:** Heuristic type detection (`.` in value = float, `t`/`f` = bool, else int). No cross-check with `meta->type`. Sending `"thermostat.setpoint": true` stores bool instead of expected float.

**Impact:** Module reads `etl::get_if<float>()` → returns nullptr → uses default value. Setting is silently ignored.

**Fix:** After parsing value, verify detected type matches `meta->type`. Reject mismatches with 400 error.

---

### BUG-024: ModuleManager init_all() always returns true

**File:** `components/modesp_core/src/module_manager.cpp`
**Root cause:** Even if a CRITICAL module's `on_init()` returns false, `init_all()` returns true.

**Impact:** main.cpp cannot detect partial init failure. System runs with broken modules.

**Fix:** Return false if any CRITICAL/HIGH priority module fails init.

---

## SEV-3 — Code Quality

### BUG-011: WsService broadcast buffer 2KB on stack — full state dump

**File:** `components/modesp_net/src/ws_service.cpp:237`
**Root cause:** `char buf[2048]` on stack in `broadcast_state()`. With 69 keys averaging ~30 bytes each, JSON is ~2070 bytes. Already borderline. At 100+ keys → truncation. Also, full dump on every version change (even single key change).

**Impact:** Stack overflow risk at scale. Unnecessary bandwidth usage. 4 clients × 2KB × 10 Hz = 80KB/s potential.

**Fix (short-term):** Increase buffer or use dynamic allocation.
**Fix (long-term):** Delta protocol — send only changed keys since last broadcast.

---

### BUG-012: NVS persist keys are positional — "p0", "p1", ...

**File:** `components/modesp_services/src/persist_service.cpp:27`, `generated/state_meta.h`
**Root cause:** PersistService uses array index in STATE_META as NVS key: `"p0"`, `"p1"`, etc. If STATE_META order changes (adding/removing module), saved values map to wrong parameters.

**Impact:** After firmware update that adds a module, `setpoint` value in NVS could be read as `alarm_delay`. Silent data corruption.

**Fix:** Use hash of key name or abbreviated key name as NVS key:
```cpp
// Instead of "p0", use CRC16 of state key name
uint16_t hash = crc16(meta.key);
snprintf(nvs_key, sizeof(nvs_key), "%04x", hash);
```

---

### BUG-013: Duplicated read_float/read_bool/read_int in 3 modules

**Files:** `thermostat_module.cpp:32-50`, `defrost_module.cpp:29-48`, `protection_module.cpp:31-50`
**Root cause:** Identical 18-line helper functions copied across 3 modules.

**Impact:** Maintenance burden. Bug fix in one → must fix in all three.

**Fix:** Move to `BaseModule`:
```cpp
class BaseModule {
protected:
    float state_read_float(const char* key, float def = 0.0f);
    bool  state_read_bool(const char* key, bool def = false);
    int32_t state_read_int(const char* key, int32_t def = 0);
};
```

---

### BUG-014: PersistService debounce 5s — critical settings can be lost on power failure

**File:** `components/modesp_services/src/persist_service.cpp:129`
**Root cause:** All persist keys share the same 5-second debounce. If power drops within 5 seconds of operator changing setpoint → change lost.

**Impact:** Operator changes setpoint via WebUI → power outage → controller boots with old setpoint → product damage.

**Fix:** Add priority persist for critical keys (setpoint, alarm limits) with immediate NVS write. NVS lifetime is ~100K cycles — at 10 writes/day = 27 years.

---

### BUG-015: comp_off_time_ms_ initialized as 999999 — magic number

**File:** `modules/thermostat/include/thermostat_module.h:112`
**Root cause:** `uint32_t comp_off_time_ms_ = 999999;` — hack to allow first compressor start (pretends compressor has been off "long enough").

**Impact:** Fragile. Works only because STARTUP state blocks `update_regulation()`. If code ordering changes, compressor could start during startup delay.

**Fix:** Initialize in `on_init()` after `sync_settings()`:
```cpp
comp_off_time_ms_ = min_off_ms_;
```

---

### BUG-016: FAD phase detection via string comparison — fragile coupling

**File:** `modules/thermostat/src/thermostat_module.cpp:200-206`
**Root cause:** Thermostat checks `defrost.phase` string: `if (sp && *sp == "fad")`. If Defrost renames "fad" → silent breakage.

**Fix:** Add explicit bool key `defrost.fad_active` in Defrost manifest:
```cpp
// defrost — enter_phase():
state_set("defrost.fad_active", p == Phase::FAD);
// thermostat — reads bool instead of string comparison
```

---

### BUG-017: SharedState version increments on every set(), even if value unchanged

**File:** `components/modesp_core/src/shared_state.cpp:43`
**Root cause:** `version_++` runs unconditionally in `set()`, even when value didn't change. The `changed` variable is only used for persist callback, not for version.

**Impact:** WsService broadcasts on every version change. With 4 modules publishing ~50 state_set() calls per cycle → version increments 50×/cycle → constant broadcast even with identical values. Wasted CPU/bandwidth.

**Fix:**
```cpp
if (changed) {
    map_[key] = value;
    version_++;
}
```

---

### BUG-018: SharedState capacity 96 — no runtime overflow detection

**File:** `components/modesp_core/src/shared_state.cpp:29-33`
**Root cause:** When map is full and new key is added, `set()` logs warning and returns `false`. But no module checks return value of `state_set()`. Silent key loss.

**Impact:** At 69/96 keys currently, adding 2-3 modules will overflow. Keys silently don't register — module works with stale/missing data.

**Fix (immediate):** Increase to 128. Add `ESP_LOGE` + assert in debug builds.
**Fix (long-term):** Generate `MODESP_MAX_STATE_ENTRIES` from manifest count + margin.

---

## SEV-4 — Minor

### BUG-019: Equipment sensor2 reports false immediately on init but has no guard like sensor1

**File:** `modules/equipment/src/equipment_module.cpp:140-151`
**Root cause:** `sensor_evap_` read failure unconditionally sets `sensor2_ok = false` (line 149), unlike `sensor_air_` which only sets false after first successful read (`if (has_air_temp_)`). Inconsistent behavior.

**Impact:** Minimal — sensor2 starts as false anyway. But inconsistent pattern.

---

### BUG-020: WsService uses malloc() in hot path for async send

**File:** `components/modesp_net/src/ws_service.cpp:308-310`
**Root cause:** `malloc(sizeof(AsyncSendCtx) + len)` called per client per broadcast. At 4 clients × 10 Hz = 40 malloc/free per second.

**Impact:** Heap fragmentation over time. For embedded 24/7 system, prefer pool allocation.

---

## Summary

| Severity | Count | Top Priority |
|----------|-------|-------------|
| SEV-1 | 4 | BUG-001 (key overflow), BUG-002 (HG defrost), BUG-003 (lockout), BUG-004 (cond fan) |
| SEV-2 | 10 | BUG-005 (SM reset), BUG-006 (re-entry), BUG-008 (demand interval), BUG-021-024 (races, validation) |
| SEV-3 | 8 | BUG-011 (WS buffer), BUG-012 (NVS keys), BUG-014 (persist debounce) |
| SEV-4 | 2 | Minor |
| **Total** | **24** | |

## Recommended Fix Order

### Sprint 1 — Safety (est. 4-6 hours)
1. **BUG-001** — Increase `MODESP_MAX_KEY_LENGTH` to 32 (10 min, rebuild all)
2. **BUG-002** — `active_type_` in defrost (15 min)
3. **BUG-006** — enter_phase guard (5 min)
4. **BUG-003** — Implement lockout in Protection (1-2 hr)
5. **BUG-015** — Remove magic number (10 min)

### Sprint 2 — Correctness (est. 4-6 hours)
6. **BUG-005** — Reset thermostat SM on defrost (30 min)
7. **BUG-007** — Phase-specific high temp blocking (30 min)
8. **BUG-009** — Track actual compressor state (1 hr)
9. **BUG-004** — Cond fan COd at defrost start (1 hr)
10. **BUG-008** — Demand defrost min interval (30 min)
11. **BUG-010** — Timeout alarm state key (30 min)

### Sprint 3 — Reliability (est. 2-3 days)
12. **BUG-012** — Stable NVS keys (1 day)
13. **BUG-014** — Priority persist (0.5 day)
14. **BUG-017** — Version increment only on change (15 min)
15. **BUG-018** — SharedState capacity + overflow check (30 min)
16. **BUG-013** — Extract helpers to BaseModule (30 min)
17. **BUG-016** — Replace string comparison with bool key (30 min)
18. **BUG-011** — WS delta protocol (1-2 days)

---

## Cross-reference with existing reviews

| This review | business_logic_review.md | architecture_review.md |
|-------------|------------------------|----------------------|
| BUG-001 | (not found) | (not found) |
| BUG-002 | B15 | — |
| BUG-003 | B2 + B4 | — |
| BUG-004 | B6 | — |
| BUG-005 | B1 | — |
| BUG-006 | B14 | — |
| BUG-007 | B7 | — |
| BUG-008 | B9 | — |
| BUG-009 | B11 | — |
| BUG-010 | B8 | — |
| BUG-011 | — | P5 |
| BUG-012 | — | P10 |
| BUG-013 | — | P6 |
| BUG-014 | — | P2 |
| BUG-015 | B10 | — |
| BUG-016 | — | P7 |
| BUG-017 | (new) | — |
| BUG-018 | — | P3 |

| BUG-019 | — | — |
| BUG-020 | — | — |
| BUG-021 | (new) | — |
| BUG-022 | (new) | — |
| BUG-023 | (new) | — |
| BUG-024 | (new) | — |

**New bugs found (not in existing reviews):** BUG-001 (StateKey overflow), BUG-017 (version increment), BUG-021 (WS race), BUG-022 (persist race), BUG-023 (settings type validation), BUG-024 (init_all always true)

---

## Changelog
- 2026-02-19 — Created. 24 bugs catalogued. 4 SEV-1, 10 SEV-2, 8 SEV-3, 2 SEV-4.
  Key finding: BUG-001 (StateKey overflow) — all existing reviews missed silent string truncation.
  6 new bugs not found in existing reviews (BUG-001, BUG-017, BUG-021–024).
