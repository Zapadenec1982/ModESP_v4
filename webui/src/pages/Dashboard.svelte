<script>
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';
  import MiniChart from '../components/MiniChart.svelte';

  $: displayTemp = $state['thermostat.display_temp'];
  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: compressor = $state['equipment.compressor'];
  $: evapFan = $state['equipment.evap_fan'];
  $: defrostRelay = $state['equipment.defrost_relay'];
  $: thermoState = $state['thermostat.state'];
  $: defrostActive = $state['defrost.active'];
  $: defrostPhase = $state['defrost.phase'];
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];
  $: nightActive = $state['thermostat.night_active'];
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

  const stateKeys = {
    idle: 'state.idle', cooling: 'state.cooling',
    safe_mode: 'state.safety_run', startup: 'state.startup'
  };
  const stateColors = {
    idle: 'var(--fg-muted)', cooling: 'var(--status-compressor)',
    safe_mode: 'var(--warning)', startup: 'var(--fg-muted)'
  };
</script>

<div class="dashboard">
  <!-- 1. Alarm banner — ПЕРШИМ -->
  {#if alarmActive}
    <div class="alarm-banner">
      {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : $t['state.alarm']}
    </div>
  {/if}

  <!-- 2. Main card: temp + status icons + setpoint + slider -->
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

    <div class="status-row">
      <!-- Compressor -->
      <div class="status-item" title={$t['dash.compressor']}>
        <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke={compressor ? 'var(--status-compressor)' : 'var(--fg-muted)'} stroke-width="2">
          <path d="M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20z" opacity="0.15" fill={compressor ? 'var(--status-compressor)' : 'none'}/>
          <circle cx="12" cy="12" r="3"/>
          <path d="M12 2v4M12 18v4M2 12h4M18 12h4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83"/>
        </svg>
        {#if compressor}
          <span class="status-dot"></span>
        {/if}
      </div>

      <!-- Evap Fan -->
      <div class="status-item" title={$t['dash.fan']}>
        <svg class="status-icon" class:spinning={!!evapFan} viewBox="0 0 24 24" fill="none" stroke={evapFan ? 'var(--status-fan)' : 'var(--fg-muted)'} stroke-width="2">
          <path d="M12 12c-3-5-8-3-8 0s5 3 8 0z"/>
          <path d="M12 12c5-3 3-8 0-8s-3 5 0 8z"/>
          <path d="M12 12c3 5 8 3 8 0s-5-3-8 0z"/>
          <path d="M12 12c-5 3-3 8 0 8s3-5 0-8z"/>
          <circle cx="12" cy="12" r="1.5" fill={evapFan ? 'var(--status-fan)' : 'var(--fg-muted)'}/>
        </svg>
      </div>

      <!-- Defrost relay -->
      {#if defrostRelay}
        <div class="status-item" title={$t['dash.heater']}>
          <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke="var(--status-heater)" stroke-width="2">
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z" fill="rgba(239,68,68,0.2)"/>
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z"/>
          </svg>
        </div>
      {/if}

      <!-- Divider -->
      <div class="status-divider"></div>

      <!-- State badge -->
      {#if defrostActive}
        <div class="state-label defrost-label">
          {$t['state.defrost']}
        </div>
      {:else}
        <div class="state-label" style="color: {stateColors[thermoState] || 'var(--fg-muted)'}">
          {stateKeys[thermoState] ? $t[stateKeys[thermoState]] : (thermoState || '—')}
        </div>
      {/if}

      <!-- Night mode badge -->
      {#if nightActive}
        <div class="state-label night-label">{$t['state.night']}</div>
      {/if}
    </div>

    <!-- Setpoint + slider inline -->
    <div class="sp-section">
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
  </div>

  <!-- 3. MiniChart -->
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
    transition: border-color var(--transition-slow);
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

  /* ── Status row (SVG icons) ───────────────── */
  .status-row {
    display: flex;
    align-items: center;
    gap: var(--sp-3);
    margin-top: var(--sp-4);
    flex-wrap: wrap;
    justify-content: center;
  }

  .status-item {
    position: relative;
    display: flex;
    align-items: center;
  }

  .status-icon {
    width: 28px;
    height: 28px;
    transition: stroke var(--transition-slow);
  }

  .status-dot {
    position: absolute;
    bottom: -2px;
    right: -2px;
    width: 8px;
    height: 8px;
    border-radius: var(--radius-full);
    background: var(--status-compressor);
    box-shadow: 0 0 6px var(--status-compressor);
    animation: dot-pulse 2s infinite;
  }

  @keyframes dot-pulse {
    0%, 100% { box-shadow: 0 0 4px var(--status-compressor); }
    50% { box-shadow: 0 0 10px var(--status-compressor); }
  }

  .spinning {
    animation: spin 2s linear infinite;
  }

  @keyframes spin {
    from { transform: rotate(0deg); }
    to { transform: rotate(360deg); }
  }

  .status-divider {
    width: 1px;
    height: 20px;
    background: var(--border);
  }

  .state-label {
    font-size: var(--text-base);
    font-weight: var(--fw-bold);
    letter-spacing: 1px;
  }

  .defrost-label {
    color: var(--status-defrost);
  }

  .night-label {
    color: var(--status-night);
  }

  /* ── Setpoint section ─────────────────────── */
  .sp-section {
    width: 100%;
    margin-top: var(--sp-4);
    padding-top: var(--sp-4);
    border-top: 1px solid var(--border);
  }

  .sp-inline {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: var(--sp-2);
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
  }

  /* ── Responsive ───────────────────────────── */
  @media (max-width: 480px) {
    .temp-value { font-size: var(--text-hero); }
    .sp-value { font-size: var(--text-2xl); }
    .tile { padding: var(--sp-4); }
    .status-icon { width: 24px; height: 24px; }
  }
</style>
