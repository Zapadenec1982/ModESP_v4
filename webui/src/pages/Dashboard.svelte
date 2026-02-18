<script>
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';

  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: compressor = $state['thermostat.compressor'];
  $: thermoState = $state['thermostat.state'];

  // Температурні зони відносно setpoint
  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: tempColor = getTemperatureColor(temperature, sp);

  function getTemperatureColor(temp, sp) {
    if (typeof temp !== 'number') return 'var(--fg-muted)';
    if (temp < sp - 2) return '#3b82f6';   // blue — холодно
    if (temp <= sp + 2) return '#22c55e';   // green — в нормі
    if (temp <= sp + 5) return '#f59e0b';   // amber — тепліше
    return '#ef4444';                       // red — alarm
  }

  const stateLabels = {
    idle: 'IDLE',
    cooling: 'COOLING',
    safe_mode: 'SAFE MODE'
  };

  const stateColors = {
    idle: 'var(--fg-muted)',
    cooling: 'var(--accent)',
    safe_mode: 'var(--warning)'
  };

  $: spMeta = $stateMeta['thermostat.setpoint'] || { min: -35, max: 0, step: 0.5 };
</script>

<div class="dashboard">
  <!-- Temperature tile -->
  <div class="tile tile-temp">
    <div class="tile-label">TEMPERATURE</div>
    <div class="temp-value" style="color: {tempColor}">
      {#if typeof temperature === 'number'}
        {temperature.toFixed(1)}
      {:else}
        —
      {/if}
      <span class="temp-unit">°C</span>
    </div>
    <div class="temp-zone">
      {#if typeof temperature === 'number' && typeof setpoint === 'number'}
        {#if temperature < setpoint - 2}
          Below setpoint
        {:else if temperature <= setpoint + 2}
          In range
        {:else if temperature <= setpoint + 5}
          Above setpoint
        {:else}
          ALARM
        {/if}
      {/if}
    </div>
  </div>

  <!-- Compressor tile -->
  <div class="tile tile-compressor">
    <div class="tile-label">COMPRESSOR</div>
    <div class="compressor-indicator">
      <span
        class="compressor-dot"
        class:on={!!compressor}
        style="background: {compressor ? 'var(--success)' : 'var(--fg-muted)'}"
      ></span>
      <span class="compressor-text">{compressor ? 'Running' : 'Off'}</span>
    </div>
  </div>

  <!-- Setpoint tile -->
  <div class="tile tile-setpoint">
    <div class="tile-label">SETPOINT</div>
    <div class="setpoint-value">
      {typeof setpoint === 'number' ? setpoint.toFixed(1) : '—'}
      <span class="temp-unit">°C</span>
    </div>
    <div class="setpoint-slider">
      <SliderWidget
        config={{
          key: 'thermostat.setpoint',
          description: '',
          unit: '°C',
          min: spMeta.min,
          max: spMeta.max,
          step: spMeta.step
        }}
        value={setpoint}
      />
    </div>
  </div>

  <!-- System state tile -->
  <div class="tile tile-state">
    <div class="tile-label">SYSTEM STATE</div>
    <div
      class="state-badge"
      style="color: {stateColors[thermoState] || 'var(--fg-muted)'}; border-color: {stateColors[thermoState] || 'var(--fg-muted)'}"
    >
      {stateLabels[thermoState] || thermoState || '—'}
    </div>
  </div>
</div>

<style>
  .dashboard {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 16px;
  }

  .tile {
    background: var(--card);
    border-radius: 12px;
    border: 1px solid var(--border);
    padding: 20px;
    display: flex;
    flex-direction: column;
    align-items: center;
    text-align: center;
  }

  .tile-label {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1.5px;
    margin-bottom: 12px;
  }

  /* Temperature tile — spans full width on mobile */
  .tile-temp {
    grid-column: 1 / -1;
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

  .temp-zone {
    font-size: 13px;
    color: var(--fg-muted);
    margin-top: 8px;
  }

  /* Compressor */
  .compressor-indicator {
    display: flex;
    align-items: center;
    gap: 12px;
    margin-top: 4px;
  }

  .compressor-dot {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    transition: all 0.3s;
  }

  .compressor-dot.on {
    box-shadow: 0 0 12px var(--success);
    animation: pulse 2s infinite;
  }

  @keyframes pulse {
    0%, 100% { box-shadow: 0 0 8px var(--success); }
    50% { box-shadow: 0 0 16px var(--success); }
  }

  .compressor-text {
    font-size: 18px;
    font-weight: 600;
  }

  /* Setpoint */
  .tile-setpoint {
    grid-column: 1 / -1;
  }

  .setpoint-value {
    font-size: 32px;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
    font-family: 'SF Mono', 'Cascadia Code', 'Fira Code', monospace;
    color: var(--accent);
    margin-bottom: 8px;
  }

  .setpoint-slider {
    width: 100%;
    max-width: 400px;
  }

  /* State badge */
  .state-badge {
    font-size: 16px;
    font-weight: 700;
    padding: 8px 20px;
    border: 2px solid;
    border-radius: 24px;
    letter-spacing: 1px;
    margin-top: 4px;
  }

  @media (max-width: 480px) {
    .dashboard {
      grid-template-columns: 1fr;
    }
    .temp-value {
      font-size: 48px;
    }
    .setpoint-value {
      font-size: 24px;
    }
    .tile {
      padding: 16px;
    }
  }
</style>
