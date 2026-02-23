<script>
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';

  // Temperature
  $: displayTemp = $state['thermostat.display_temp'];
  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: showDefrostSymbol = typeof displayTemp === 'number' && displayTemp <= -900;
  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: shownTemp = showDefrostSymbol ? null : (typeof displayTemp === 'number' ? displayTemp : temperature);
  $: tempColor = getTemperatureColor(shownTemp, sp);

  // Equipment
  $: compressor = $state['equipment.compressor'];
  $: evapFan = $state['equipment.evap_fan'];
  $: condFan = $state['equipment.cond_fan'];
  $: heater = $state['equipment.heater'];
  $: hgValve = $state['equipment.hg_valve'];
  $: hasCond = $state['equipment.has_cond_fan'];
  $: hasEvapTemp = $state['equipment.has_evap_temp'];
  $: evapTemp = $state['equipment.evap_temp'];
  $: condTemp = $state['equipment.cond_temp'];

  // Status
  $: thermoState = $state['thermostat.state'];
  $: defrostActive = $state['defrost.active'];
  $: defrostPhase = $state['defrost.phase'];
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];
  $: nightActive = $state['thermostat.night_active'];

  function getTemperatureColor(temp, sp) {
    if (typeof temp !== 'number') return 'var(--fg-muted)';
    if (temp < sp - 2) return '#3b82f6';
    if (temp <= sp + 2) return '#22c55e';
    if (temp <= sp + 5) return '#f59e0b';
    return '#ef4444';
  }

  const stateLabels = {
    idle: 'IDLE', cooling: 'COOLING',
    safe_mode: 'SAFE MODE', startup: 'STARTUP'
  };
  const stateColors = {
    idle: 'var(--fg-muted)', cooling: 'var(--accent)',
    safe_mode: 'var(--warning)', startup: 'var(--fg-muted)'
  };

  const phaseLabels = {
    stabilize: 'СТАБІЛІЗ.', valve_open: 'КЛАПАН', active: 'НАГРІВ',
    equalize: 'ВИРІВН.', drip: 'СТІК', fad: 'ОХОЛОДЖ.'
  };

  $: spMeta = $stateMeta['thermostat.setpoint'] || { min: -35, max: 0, step: 0.5 };

  // Equipment items for grid (filter by availability)
  $: equipItems = [
    { key: 'compressor', label: 'COMP', on: !!compressor, show: true },
    { key: 'evap_fan', label: 'EVAP', on: !!evapFan, show: true },
    { key: 'cond_fan', label: 'COND', on: !!condFan, show: !!hasCond },
    { key: 'heater', label: 'HEAT', on: !!heater, show: !!heater || defrostActive },
    { key: 'hg_valve', label: 'HG', on: !!hgValve, show: !!hgValve || defrostActive }
  ].filter(e => e.show);
</script>

<div class="dashboard">
  <!-- Hero: Temperature + Setpoint -->
  <div class="hero" class:alarm-border={alarmActive}>
    <div class="hero-top">
      <div class="temp-block">
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
        <div class="thermo-state" style="color: {defrostActive ? '#ef4444' : (stateColors[thermoState] || 'var(--fg-muted)')}">
          {#if defrostActive}
            DEFROST{defrostPhase ? ' · ' + (phaseLabels[defrostPhase] || defrostPhase) : ''}
          {:else}
            {stateLabels[thermoState] || thermoState || '—'}
          {/if}
        </div>
      </div>
      <div class="sp-block">
        <div class="sp-label">SP</div>
        <div class="sp-value">{typeof setpoint === 'number' ? setpoint.toFixed(1) : '—'}°</div>
      </div>
    </div>
    <div class="slider-row">
      <SliderWidget
        config={{ key: 'thermostat.setpoint', description: '', unit: '°C',
          min: spMeta.min, max: spMeta.max, step: spMeta.step }}
        value={setpoint}
      />
    </div>
  </div>

  <!-- Equipment Grid -->
  <div class="eq-grid">
    {#each equipItems as eq}
      <div class="eq-cell" class:eq-on={eq.on}>
        <div class="eq-dot" class:dot-on={eq.on}></div>
        <div class="eq-label">{eq.label}</div>
      </div>
    {/each}
  </div>

  <!-- Active States Bar -->
  {#if alarmActive || defrostActive || nightActive}
    <div class="states-bar">
      {#if alarmActive}
        <div class="state-chip alarm-chip">
          {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : 'ALARM'}
        </div>
      {/if}
      {#if defrostActive}
        <div class="state-chip defrost-chip">
          DEFROST{defrostPhase ? ' · ' + (phaseLabels[defrostPhase] || defrostPhase) : ''}
        </div>
      {/if}
      {#if nightActive}
        <div class="state-chip night-chip">NIGHT</div>
      {/if}
    </div>
  {/if}

  <!-- Sensor Readings -->
  {#if hasEvapTemp || condTemp !== undefined}
    <div class="sensors-row">
      {#if hasEvapTemp && typeof evapTemp === 'number'}
        <div class="sensor-card">
          <div class="sensor-label">Випарник</div>
          <div class="sensor-value">{evapTemp.toFixed(1)}°C</div>
        </div>
      {/if}
      {#if typeof condTemp === 'number'}
        <div class="sensor-card">
          <div class="sensor-label">Конденсатор</div>
          <div class="sensor-value">{condTemp.toFixed(1)}°C</div>
        </div>
      {/if}
    </div>
  {/if}
</div>

<style>
  .dashboard { display: flex; flex-direction: column; gap: 12px; }

  /* Hero card */
  .hero {
    background: var(--card);
    border-radius: 12px;
    border: 1px solid var(--border);
    padding: 20px;
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
    transition: border-color 0.3s;
  }
  .alarm-border {
    border-color: #ef4444;
    animation: alarm-pulse 1.5s infinite;
  }
  .hero-top {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
  }
  .temp-block { flex: 1; }
  .temp-value {
    font-size: 56px;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    line-height: 1;
    transition: color 0.5s;
  }
  .temp-unit {
    font-size: 0.35em;
    font-weight: 400;
    vertical-align: super;
    color: var(--fg-muted);
  }
  .thermo-state {
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 1px;
    margin-top: 6px;
  }
  .sp-block {
    text-align: right;
    flex-shrink: 0;
    padding-left: 16px;
  }
  .sp-label {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-muted);
    letter-spacing: 1.5px;
  }
  .sp-value {
    font-size: 28px;
    font-weight: 700;
    color: var(--accent);
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    line-height: 1;
  }
  .slider-row { margin-top: 12px; }

  /* Equipment Grid */
  .eq-grid {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .eq-cell {
    flex: 1;
    min-width: 60px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 6px;
    padding: 10px 6px;
    background: var(--card);
    border-radius: 10px;
    border: 1px solid var(--border);
    transition: all 0.3s;
  }
  .eq-cell.eq-on {
    border-color: var(--success);
    background: color-mix(in srgb, var(--success) 8%, var(--card));
  }
  .eq-dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: var(--fg-dim, #64748b);
    transition: all 0.3s;
  }
  .eq-dot.dot-on {
    background: var(--success);
    box-shadow: 0 0 8px var(--success);
  }
  .eq-label {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-muted);
    letter-spacing: 0.5px;
  }
  .eq-cell.eq-on .eq-label { color: var(--fg); }

  /* Active States Bar */
  .states-bar {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .state-chip {
    flex: 1;
    min-width: 80px;
    text-align: center;
    padding: 8px 12px;
    border-radius: 8px;
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 0.5px;
    border: 1px solid;
  }
  .alarm-chip {
    color: #ef4444;
    border-color: #ef4444;
    background: rgba(239,68,68,0.1);
    animation: alarm-text 1.5s infinite;
  }
  .defrost-chip {
    color: #f59e0b;
    border-color: #f59e0b;
    background: rgba(245,158,11,0.1);
  }
  .night-chip {
    color: #8b5cf6;
    border-color: #8b5cf6;
    background: rgba(139,92,246,0.1);
  }

  /* Sensor Readings */
  .sensors-row {
    display: flex;
    gap: 8px;
  }
  .sensor-card {
    flex: 1;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 12px;
    text-align: center;
  }
  .sensor-label {
    font-size: 11px;
    color: var(--fg-muted);
    letter-spacing: 0.5px;
    margin-bottom: 4px;
  }
  .sensor-value {
    font-size: 20px;
    font-weight: 700;
    color: var(--fg);
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
  }

  @keyframes alarm-text {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.6; }
  }
  @keyframes alarm-pulse {
    0%, 100% { border-color: #ef4444; }
    50% { border-color: #991b1b; }
  }

  @media (max-width: 480px) {
    .temp-value { font-size: 44px; }
    .sp-value { font-size: 22px; }
    .hero { padding: 16px; }
    .eq-cell { min-width: 50px; padding: 8px 4px; }
  }
</style>
