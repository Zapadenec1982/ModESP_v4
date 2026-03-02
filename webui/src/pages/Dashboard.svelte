<script>
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';
  import MiniChart from '../components/MiniChart.svelte';

  // Main values
  $: displayTemp = $state['thermostat.display_temp'];
  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: thermoState = $state['thermostat.state'];

  // Equipment outputs
  $: compressor = $state['equipment.compressor'];
  $: evapFan = $state['equipment.evap_fan'];
  $: condFan = $state['equipment.cond_fan'];
  $: defrostRelay = $state['equipment.defrost_relay'];
  $: doorOpen = $state['equipment.door_open'];

  // Defrost / alarm / night
  $: defrostActive = $state['defrost.active'];
  $: defrostPhase = $state['defrost.phase'];
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];
  $: nightActive = $state['thermostat.night_active'];

  // Hardware availability
  $: hasCondFan = !!$state['equipment.has_cond_fan'];
  $: hasDoor = !!$state['equipment.has_door_contact'];
  $: hasEvapTemp = !!$state['equipment.has_evap_temp'];
  $: hasCondTemp = !!$state['equipment.has_cond_temp'];

  // Secondary temperatures
  $: evapTemp = $state['equipment.evap_temp'];
  $: condTemp = $state['equipment.cond_temp'];

  // Display logic
  $: showDefrostSymbol = typeof displayTemp === 'number' && displayTemp <= -900;
  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: shownTemp = showDefrostSymbol ? null : (typeof displayTemp === 'number' ? displayTemp : temperature);
  $: tempColor = getTemperatureColor(shownTemp, sp);
  $: spMeta = $stateMeta['thermostat.setpoint'] || { min: -35, max: 0, step: 0.5 };

  function getTemperatureColor(temp, sp) {
    if (typeof temp !== 'number') return 'var(--fg-muted)';
    if (temp < sp - 2) return '#3b82f6';
    if (temp <= sp + 2) return 'var(--status-ok)';
    if (temp <= sp + 5) return '#f59e0b';
    return '#ef4444';
  }

  // State label
  const stateKeys = {
    idle: 'state.idle', cooling: 'state.cooling',
    safe_mode: 'state.safety_run', startup: 'state.startup'
  };
  const stateColors = {
    idle: 'var(--fg-muted)', cooling: 'var(--status-compressor)',
    safe_mode: 'var(--warning)', startup: 'var(--fg-muted)'
  };

  // Defrost phase i18n
  const phaseKeys = {
    stabilize: 'defrost.stabilize', valve_open: 'defrost.valve_open',
    active: 'defrost.active', equalize: 'defrost.equalize',
    drip: 'defrost.drip', fad: 'defrost.fad'
  };
</script>

<div class="dashboard">
  <!-- 1. Alarm banner — ПЕРШИМ -->
  {#if alarmActive}
    <div class="alarm-banner">
      {$t['alarm.banner']}: {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : ''}
    </div>
  {/if}

  <!-- 2. Main temperature + setpoint + slider -->
  <div class="tile tile-main" class:alarm-glow={alarmActive}>
    <div class="temp-value" style="color: {tempColor}">
      {#if showDefrostSymbol}
        -d-
      {:else if typeof shownTemp === 'number'}
        {shownTemp.toFixed(1)}
      {:else}
        —
      {/if}
      <span class="temp-unit">{showDefrostSymbol ? '' : '°C'}</span>
    </div>

    <div class="sp-inline">
      <span class="sp-label">{$t['dash.setpoint']}</span>
      <span class="sp-value">{typeof setpoint === 'number' ? setpoint.toFixed(1) : '—'}°C</span>
    </div>

    <div class="slider-wrap">
      <SliderWidget
        config={{
          key: 'thermostat.setpoint', description: '', unit: '°C',
          min: spMeta.min, max: spMeta.max, step: spMeta.step
        }}
        value={setpoint}
      />
    </div>
  </div>

  <!-- 3. Equipment pills + state -->
  <div class="tile tile-equip">
    <div class="pills-row">
      <span class="pill pill-comp" class:active={compressor}>
        {$t['dash.comp']}
      </span>
      <span class="pill pill-fan" class:active={evapFan}>
        {$t['dash.evap_fan']}
      </span>
      {#if hasCondFan}
        <span class="pill pill-cond" class:active={condFan}>
          {$t['dash.cond_fan']}
        </span>
      {/if}
      {#if defrostRelay}
        <span class="pill pill-defrost active">
          {$t['dash.defrost_pill']}
        </span>
      {/if}
      {#if hasDoor && doorOpen}
        <span class="pill pill-door active">
          {$t['dash.door_open']}
        </span>
      {/if}
    </div>

    <div class="state-row">
      {#if defrostActive}
        <span class="state-text" style="color: var(--status-defrost)">
          {$t['state.defrost']}
          {#if defrostPhase && phaseKeys[defrostPhase]}
            — {$t[phaseKeys[defrostPhase]]}
          {/if}
        </span>
      {:else}
        <span class="state-text" style="color: {stateColors[thermoState] || 'var(--fg-muted)'}">
          {stateKeys[thermoState] ? $t[stateKeys[thermoState]] : (thermoState || '—')}
        </span>
      {/if}
      {#if nightActive}
        <span class="badge badge-night">{$t['state.night']}</span>
      {/if}
    </div>
  </div>

  <!-- 4. Secondary temperatures -->
  {#if hasEvapTemp || hasCondTemp}
    <div class="tile tile-temps">
      {#if hasEvapTemp}
        <div class="temp-item">
          <span class="temp-label">{$t['dash.evap_temp']}</span>
          <span class="temp-num">{typeof evapTemp === 'number' ? evapTemp.toFixed(1) : '—'}°C</span>
        </div>
      {/if}
      {#if hasCondTemp}
        <div class="temp-item">
          <span class="temp-label">{$t['dash.cond_temp']}</span>
          <span class="temp-num">{typeof condTemp === 'number' ? condTemp.toFixed(1) : '—'}°C</span>
        </div>
      {/if}
    </div>
  {/if}

  <!-- 5. MiniChart -->
  <MiniChart />
</div>

<style>
  .dashboard {
    display: flex;
    flex-direction: column;
    gap: var(--sp-4);
  }

  .tile {
    background: var(--card);
    border-radius: var(--radius-2xl);
    border: 1px solid var(--border);
    padding: var(--sp-5);
  }

  /* ── Alarm banner ─────────────────────────── */
  .alarm-banner {
    padding: var(--sp-3) var(--sp-4);
    background: var(--alarm-bg-strong);
    border: 1px solid var(--alarm-border);
    border-radius: var(--radius-lg);
    color: var(--alarm-border);
    font-size: var(--text-base);
    font-weight: var(--fw-bold);
    letter-spacing: 1px;
    text-align: center;
    animation: alarm-pulse 1.5s infinite;
  }

  @keyframes alarm-pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.7; }
  }

  /* ── Main temperature card ────────────────── */
  .tile-main {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
  }

  .alarm-glow {
    border-color: var(--alarm-border);
    box-shadow: 0 0 12px var(--alarm-bg-strong);
  }

  .temp-value {
    font-size: var(--text-hero-lg);
    font-weight: var(--fw-bold);
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    line-height: 1;
    transition: color var(--transition-slow);
  }

  .temp-unit {
    font-size: 0.4em;
    font-weight: var(--fw-normal);
    vertical-align: super;
    color: var(--fg-muted);
  }

  .sp-inline {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
    margin-top: var(--sp-3);
  }

  .sp-label {
    font-size: var(--text-sm);
    font-weight: var(--fw-bold);
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1.5px;
  }

  .sp-value {
    font-size: var(--text-3xl);
    font-weight: var(--fw-bold);
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    color: var(--accent);
  }

  .slider-wrap {
    width: 100%;
    margin-top: var(--sp-3);
  }

  /* ── Equipment pills ──────────────────────── */
  .tile-equip {
    padding: var(--sp-4);
  }

  .pills-row {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    justify-content: center;
  }

  .pill {
    display: inline-flex;
    align-items: center;
    padding: var(--sp-1-5) var(--sp-3);
    border-radius: var(--radius-pill);
    font-size: var(--text-sm);
    font-weight: var(--fw-bold);
    letter-spacing: 0.5px;
    text-transform: uppercase;
    border: 1px solid var(--border);
    color: var(--fg-muted);
    background: transparent;
    transition: all var(--transition-normal);
    white-space: nowrap;
  }

  .pill-comp.active {
    background: var(--status-compressor);
    border-color: var(--status-compressor);
    color: #fff;
  }

  .pill-fan.active {
    background: var(--status-fan);
    border-color: var(--status-fan);
    color: #fff;
  }

  .pill-cond.active {
    background: var(--status-fan);
    border-color: var(--status-fan);
    color: #fff;
  }

  .pill-defrost {
    background: var(--status-defrost);
    border-color: var(--status-defrost);
    color: #fff;
  }

  .pill-door {
    background: var(--alarm-border);
    border-color: var(--alarm-border);
    color: #fff;
  }

  .state-row {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: var(--sp-3);
    margin-top: var(--sp-3);
    flex-wrap: wrap;
  }

  .state-text {
    font-size: var(--text-base);
    font-weight: var(--fw-bold);
    letter-spacing: 1px;
    text-transform: uppercase;
  }

  .badge {
    display: inline-flex;
    align-items: center;
    padding: var(--sp-1) var(--sp-2-5);
    border-radius: var(--radius-pill);
    font-size: var(--text-xs);
    font-weight: var(--fw-bold);
    letter-spacing: 0.5px;
    text-transform: uppercase;
  }

  .badge-night {
    background: rgba(124, 58, 237, 0.15);
    color: var(--status-night);
    border: 1px solid var(--status-night);
  }

  /* ── Secondary temperatures ───────────────── */
  .tile-temps {
    display: flex;
    justify-content: space-around;
    padding: var(--sp-4);
    gap: var(--sp-4);
  }

  .temp-item {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: var(--sp-1);
  }

  .temp-label {
    font-size: var(--text-sm);
    font-weight: var(--fw-bold);
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1px;
  }

  .temp-num {
    font-size: var(--text-lg);
    font-weight: var(--fw-semibold);
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    color: var(--fg);
  }

  /* ── Responsive ───────────────────────────── */
  @media (max-width: 480px) {
    .temp-value { font-size: var(--text-hero); }
    .sp-value { font-size: var(--text-2xl); }
    .tile { padding: var(--sp-4); }
  }
</style>
