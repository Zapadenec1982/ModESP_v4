<script>
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';

  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: compressor = $state['equipment.compressor'];
  $: evapFan = $state['equipment.evap_fan'];
  $: heater = $state['equipment.heater'];
  $: thermoState = $state['thermostat.state'];
  $: defrostActive = $state['defrost.active'];
  $: defrostPhase = $state['defrost.phase'];
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];

  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: tempColor = getTemperatureColor(temperature, sp);

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

  $: spMeta = $stateMeta['thermostat.setpoint'] || { min: -35, max: 0, step: 0.5 };
</script>

<div class="dashboard">
  <!-- Main status card -->
  <div class="tile tile-main" class:alarm-border={alarmActive}>
    <div class="temp-value" style="color: {tempColor}">
      {#if typeof temperature === 'number'}
        {temperature.toFixed(1)}
      {:else}
        —
      {/if}
      <span class="temp-unit">°C</span>
    </div>

    <div class="status-row">
      <!-- Compressor -->
      <div class="status-item" title="Компресор">
        <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke={compressor ? '#22c55e' : 'var(--fg-muted)'} stroke-width="2">
          <path d="M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20z" opacity="0.15" fill={compressor ? '#22c55e' : 'none'}/>
          <circle cx="12" cy="12" r="3"/>
          <path d="M12 2v4M12 18v4M2 12h4M18 12h4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83"/>
        </svg>
        {#if compressor}
          <span class="status-dot" style="background:#22c55e"></span>
        {/if}
      </div>

      <!-- Evap Fan -->
      <div class="status-item" title="Вентилятор">
        <svg class="status-icon" class:spinning={!!evapFan} viewBox="0 0 24 24" fill="none" stroke={evapFan ? '#3b82f6' : 'var(--fg-muted)'} stroke-width="2">
          <path d="M12 12c-3-5-8-3-8 0s5 3 8 0z"/>
          <path d="M12 12c5-3 3-8 0-8s-3 5 0 8z"/>
          <path d="M12 12c3 5 8 3 8 0s-5-3-8 0z"/>
          <path d="M12 12c-5 3-3 8 0 8s3-5 0-8z"/>
          <circle cx="12" cy="12" r="1.5" fill={evapFan ? '#3b82f6' : 'var(--fg-muted)'}/>
        </svg>
      </div>

      <!-- Heater -->
      {#if heater}
        <div class="status-item" title="Нагрівач">
          <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2">
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z" fill="rgba(239,68,68,0.2)"/>
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z"/>
          </svg>
        </div>
      {/if}

      <!-- Divider -->
      <div class="status-divider"></div>

      <!-- State badge -->
      <div class="state-label" style="color: {stateColors[thermoState] || 'var(--fg-muted)'}">
        {stateLabels[thermoState] || thermoState || '—'}
      </div>

      <!-- Defrost -->
      {#if defrostActive}
        <div class="state-label defrost-label">
          DEFROST
        </div>
      {/if}
    </div>

    <!-- Alarm banner -->
    {#if alarmActive}
      <div class="alarm-banner">
        {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : 'ALARM'}
      </div>
    {/if}
  </div>

  <!-- Setpoint tile -->
  <div class="tile tile-setpoint">
    <div class="sp-header">
      <span class="sp-label">SETPOINT</span>
      <span class="sp-value" style="color: var(--accent)">
        {typeof setpoint === 'number' ? setpoint.toFixed(1) : '—'}°C
      </span>
    </div>
    <div class="setpoint-slider">
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

<style>
  .dashboard {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .tile {
    background: var(--card);
    border-radius: 12px;
    border: 1px solid var(--border);
    padding: 20px;
  }

  /* Main card */
  .tile-main {
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
    transition: border-color 0.3s;
  }

  .alarm-border {
    border-color: #ef4444;
    animation: alarm-pulse 1.5s infinite;
  }

  .temp-value {
    font-size: 64px;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    line-height: 1;
    transition: color 0.5s;
  }

  .temp-unit {
    font-size: 0.4em;
    font-weight: 400;
    vertical-align: super;
    color: var(--fg-muted);
  }

  /* Status row */
  .status-row {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-top: 16px;
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
    transition: stroke 0.3s;
  }

  .status-dot {
    position: absolute;
    bottom: -2px;
    right: -2px;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    box-shadow: 0 0 6px #22c55e;
    animation: dot-pulse 2s infinite;
  }

  @keyframes dot-pulse {
    0%, 100% { box-shadow: 0 0 4px #22c55e; }
    50% { box-shadow: 0 0 10px #22c55e; }
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
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 1px;
  }

  .defrost-label {
    color: #ef4444;
  }

  /* Alarm banner */
  .alarm-banner {
    margin-top: 12px;
    padding: 6px 16px;
    background: rgba(239, 68, 68, 0.1);
    border: 1px solid #ef4444;
    border-radius: 8px;
    color: #ef4444;
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 1px;
    animation: alarm-text 1.5s infinite;
  }

  @keyframes alarm-text {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.6; }
  }

  @keyframes alarm-pulse {
    0%, 100% { border-color: #ef4444; }
    50% { border-color: #991b1b; }
  }

  /* Setpoint tile */
  .tile-setpoint {
    padding: 16px 20px;
  }

  .sp-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 8px;
  }

  .sp-label {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1.5px;
  }

  .sp-value {
    font-size: 24px;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
  }

  .setpoint-slider {
    width: 100%;
  }

  @media (max-width: 480px) {
    .temp-value { font-size: 48px; }
    .sp-value { font-size: 20px; }
    .tile { padding: 16px; }
    .status-icon { width: 24px; height: 24px; }
  }
</style>
