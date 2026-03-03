<script>
  import { state } from '../stores/state.js';
  import { stateMeta, navigateTo } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import Icon from '../components/Icon.svelte';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';
  import MiniChart from '../components/MiniChart.svelte';

  $: displayTemp = $state['thermostat.display_temp'];
  $: temperature = $state['thermostat.temperature'];
  $: setpoint = $state['thermostat.setpoint'];
  $: compressor = $state['equipment.compressor'];
  $: evapFan = $state['equipment.evap_fan'];
  $: condFan = $state['equipment.cond_fan'];
  $: defrostRelay = $state['equipment.defrost_relay'];
  $: thermoState = $state['thermostat.state'];
  $: defrostActive = $state['defrost.active'];
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];
  $: nightActive = $state['thermostat.night_active'];
  $: showDefrostSymbol = typeof displayTemp === 'number' && displayTemp <= -900;

  // Metrics
  $: evapTemp = $state['equipment.evap_temp'];
  $: condTemp = $state['equipment.cond_temp'];
  $: hasEvap = $state['equipment.has_evap_temp'];
  $: hasCond = $state['equipment.has_cond_temp'];

  // Protection summary
  $: alarmCount = $state['protection.alarm_count'] || 0;

  // Equipment runtime (from protection diagnostics)
  $: compRunTime = $state['protection.comp_run_time'] || 0;

  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: shownTemp = showDefrostSymbol ? null : (typeof displayTemp === 'number' ? displayTemp : temperature);
  $: differential = (typeof shownTemp === 'number' && typeof sp === 'number') ? (shownTemp - sp) : null;
  $: spMeta = $stateMeta['thermostat.setpoint'] || { min: -35, max: 0, step: 0.5 };

  // State badge
  const stateKeys = {
    idle: 'state.idle', cooling: 'state.cooling',
    safe_mode: 'state.safety_run', startup: 'state.startup'
  };
  const stateVariants = {
    idle: 'neutral', cooling: 'info', safe_mode: 'warn', startup: 'neutral'
  };

  $: stateBadgeKey = defrostActive ? 'state.defrost' : (stateKeys[thermoState] || null);
  $: stateBadgeVariant = defrostActive ? 'warn'
    : alarmActive ? 'danger'
    : (stateVariants[thermoState] || 'neutral');

  function fmtRuntime(sec) {
    if (!sec || sec < 0) return '0:00';
    const m = Math.floor(sec / 60);
    const s = sec % 60;
    if (m >= 60) {
      const h = Math.floor(m / 60);
      return `${h}:${String(m % 60).padStart(2, '0')}h`;
    }
    return `${m}:${String(s).padStart(2, '0')}`;
  }
</script>

<div class="dashboard">
  <!-- 1. Hero temperature card -->
  <div class="hero-temp" class:alarm={alarmActive}>
    <div class="hero-eyebrow">{$t['dash.chamber']}</div>
    <div class="hero-row">
      <div class="hero-digits">
        {#if showDefrostSymbol}
          <span class="hero-num">-d-</span>
        {:else if typeof shownTemp === 'number'}
          <span class="hero-num" class:alarm-text={alarmActive}>{shownTemp.toFixed(1)}</span>
          <span class="hero-unit">°C</span>
        {:else}
          <span class="hero-num">—</span>
        {/if}
      </div>
      <div class="hero-badges">
        <span class="badge {stateBadgeVariant}">
          <span class="badge-dot"></span>
          {stateBadgeKey ? $t[stateBadgeKey] : '—'}
        </span>
        {#if nightActive}
          <span class="badge night">
            <span class="badge-dot"></span>
            {$t['state.night']}
          </span>
        {/if}
      </div>
    </div>

    <!-- Setpoint section -->
    <div class="sp-section">
      <div class="sp-row">
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

  <!-- 2. Metrics row -->
  {#if hasEvap || hasCond}
    <div class="metrics-row">
      {#if hasEvap}
        <div class="metric-card mc-frost">
          <div class="metric-header">
            <Icon name="thermometer" size={14} />
            <span>{$t['dash.evaporator']}</span>
          </div>
          <div class="metric-value">{typeof evapTemp === 'number' ? evapTemp.toFixed(1) : '—'}°C</div>
        </div>
      {/if}
      {#if hasCond}
        <div class="metric-card mc-warn">
          <div class="metric-header">
            <Icon name="thermometer" size={14} />
            <span>{$t['dash.cond_temp']}</span>
          </div>
          <div class="metric-value">{typeof condTemp === 'number' ? condTemp.toFixed(1) : '—'}°C</div>
        </div>
      {/if}
      <div class="metric-card mc-accent">
        <div class="metric-header">
          <Icon name="activity" size={14} />
          <span>{$t['dash.differential']}</span>
        </div>
        <div class="metric-value">{differential !== null ? (differential > 0 ? '+' : '') + differential.toFixed(1) : '—'}°C</div>
      </div>
    </div>
  {/if}

  <!-- 3. Equipment grid -->
  <div class="eq-grid">
    <div class="eq-cell" class:eq-on={compressor} class:eq-idle={!compressor}>
      <div class="eq-top">
        <span class="eq-label">{$t['dash.compressor']}</span>
        <span class="badge {compressor ? 'ok' : 'neutral'} badge-sm">
          <span class="badge-dot"></span>
          {compressor ? 'ON' : 'OFF'}
        </span>
      </div>
      {#if compressor && compRunTime > 0}
        <div class="eq-timer">{fmtRuntime(compRunTime)}</div>
      {/if}
    </div>

    <div class="eq-cell" class:eq-on={evapFan} class:eq-idle={!evapFan}>
      <div class="eq-top">
        <span class="eq-label">{$t['dash.fan']}</span>
        <span class="badge {evapFan ? 'ok' : 'neutral'} badge-sm">
          <span class="badge-dot"></span>
          {evapFan ? 'ON' : 'OFF'}
        </span>
      </div>
    </div>

    <div class="eq-cell" class:eq-on={condFan} class:eq-idle={!condFan}>
      <div class="eq-top">
        <span class="eq-label">{$t['dash.condenser']}</span>
        <span class="badge {condFan ? 'ok' : 'neutral'} badge-sm">
          <span class="badge-dot"></span>
          {condFan ? 'ON' : 'OFF'}
        </span>
      </div>
    </div>

    <div class="eq-cell" class:eq-on={defrostRelay} class:eq-idle={!defrostRelay}>
      <div class="eq-top">
        <span class="eq-label">{$t['defrost.toggle']}</span>
        <span class="badge {defrostRelay ? 'warn' : 'neutral'} badge-sm">
          <span class="badge-dot"></span>
          {defrostRelay ? 'ON' : 'OFF'}
        </span>
      </div>
    </div>
  </div>

  <!-- 4. Chart -->
  <div class="chart-card">
    <MiniChart />
  </div>

  <!-- 5. Protection summary -->
  <button class="prot-summary" on:click={() => $navigateTo = 'protection'}>
    <div class="prot-left">
      <Icon name="shield" size={18} />
      <span class="prot-title">{$t['prot.summary']}</span>
    </div>
    <div class="prot-right">
      {#if alarmActive}
        <span class="badge danger badge-sm">
          <span class="badge-dot"></span>
          {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : $t['state.alarm']}
        </span>
      {:else}
        <span class="badge ok badge-sm">
          <span class="badge-dot"></span>
          {$t['prot.ok']}
        </span>
      {/if}
      <span class="prot-detail">{alarmCount} {$t['prot.alarms']}</span>
      <Icon name="chevron-right" size={16} />
    </div>
  </button>
</div>

<style>
  .dashboard {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  /* ── Badge component (inline) ────────────────────── */
  .badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 4px 10px;
    border-radius: 20px;
    font-size: 11px;
    font-weight: 600;
    font-family: var(--font-mono);
    letter-spacing: 0.5px;
    text-transform: uppercase;
  }

  .badge-sm {
    padding: 3px 8px;
    font-size: 10px;
  }

  .badge-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .badge.ok { background: var(--ok-dim); color: var(--ok); border: 1px solid var(--ok-border); }
  .badge.ok .badge-dot { background: var(--ok); box-shadow: 0 0 6px var(--ok-glow); }

  .badge.danger { background: var(--danger-dim); color: var(--danger); border: 1px solid var(--danger-border); }
  .badge.danger .badge-dot { background: var(--danger); box-shadow: 0 0 6px var(--danger-glow); animation: dotPulse 2s infinite; }

  .badge.warn { background: var(--warn-dim); color: var(--warn); border: 1px solid rgba(251, 191, 36, 0.15); }
  .badge.warn .badge-dot { background: var(--warn); }

  .badge.info { background: var(--frost-dim); color: var(--frost); border: 1px solid var(--frost-border); }
  .badge.info .badge-dot { background: var(--frost); box-shadow: 0 0 6px var(--frost-glow); }

  .badge.neutral { background: var(--surface-3); color: var(--text-3); border: 1px solid var(--border); }
  .badge.neutral .badge-dot { background: var(--text-4); }

  .badge.night { background: rgba(124, 58, 237, 0.08); color: #7c3aed; border: 1px solid rgba(124, 58, 237, 0.15); }
  .badge.night .badge-dot { background: #7c3aed; }

  @keyframes dotPulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }

  /* ── Hero temperature card ───────────────────────── */
  .hero-temp {
    background: var(--surface);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    padding: 24px;
    position: relative;
    overflow: hidden;
  }

  /* Top-line gradient accent */
  .hero-temp::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent 0%, var(--frost-border) 50%, transparent 100%);
  }

  /* OK state: frost glow */
  .hero-temp:not(.alarm) .hero-digits::after {
    content: '';
    position: absolute;
    top: -30px;
    right: -60px;
    width: 180px;
    height: 180px;
    border-radius: 50%;
    background: radial-gradient(circle, rgba(86, 212, 232, 0.08) 0%, transparent 70%);
    opacity: 0.6;
    pointer-events: none;
  }

  /* Alarm state: danger glow + breathing */
  .hero-temp.alarm {
    border-color: var(--danger-border);
  }

  .hero-temp.alarm .hero-digits::after {
    content: '';
    position: absolute;
    top: -30px;
    right: -60px;
    width: 180px;
    height: 180px;
    border-radius: 50%;
    background: radial-gradient(circle, var(--danger-glow) 0%, transparent 70%);
    pointer-events: none;
    animation: heroBreath 3s ease-in-out infinite;
  }

  @keyframes heroBreath {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.6; }
  }

  .hero-eyebrow {
    font-size: 11px;
    font-weight: 600;
    font-family: var(--font-mono);
    color: var(--text-3);
    letter-spacing: 2px;
    text-transform: uppercase;
    margin-bottom: 8px;
  }

  .hero-row {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 16px;
    flex-wrap: wrap;
  }

  .hero-digits {
    display: flex;
    align-items: baseline;
    gap: 4px;
    position: relative;
  }

  .hero-num {
    font-size: 72px;
    font-weight: 800;
    font-family: var(--font-display);
    letter-spacing: -4px;
    line-height: 1;
    color: var(--text-1);
    font-variant-numeric: tabular-nums;
  }

  .hero-num.alarm-text {
    color: var(--danger);
    text-shadow: 0 0 60px var(--danger-glow), 0 0 120px rgba(248, 113, 113, 0.06), 0 0 30px var(--danger-glow);
  }

  .hero-unit {
    font-size: 28px;
    font-weight: 400;
    color: var(--text-3);
    position: relative;
    top: -8px;
  }

  .hero-badges {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 6px;
    padding-bottom: 8px;
  }

  /* ── Setpoint section ────────────────────────────── */
  .sp-section {
    margin-top: 20px;
    padding-top: 16px;
    border-top: 1px solid var(--border);
    position: relative;
    z-index: 1;
  }

  .sp-row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 8px;
  }

  .sp-label {
    font-size: 11px;
    font-weight: 600;
    font-family: var(--font-mono);
    color: var(--text-3);
    letter-spacing: 2px;
    text-transform: uppercase;
  }

  .sp-value {
    font-size: 28px;
    font-weight: 600;
    font-family: var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--ok);
    text-shadow: 0 0 30px var(--ok-glow);
  }

  .slider-wrap {
    width: 100%;
  }

  /* ── Metrics row ─────────────────────────────────── */
  .metrics-row {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 12px;
  }

  .metric-card {
    background: var(--surface);
    border-radius: var(--radius-sm);
    border: 1px solid var(--border);
    padding: 14px 16px;
    position: relative;
    overflow: hidden;
  }

  /* Top-line gradient */
  .metric-card::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent 0%, var(--border-accent) 50%, transparent 100%);
  }

  /* Colored left accent bar */
  .metric-card::before {
    content: '';
    position: absolute;
    left: 0;
    top: 12px;
    bottom: 12px;
    width: 3px;
    border-radius: 0 3px 3px 0;
  }

  .mc-frost::before { background: var(--frost); box-shadow: 0 0 8px var(--frost-glow); }
  .mc-warn::before { background: var(--warn); box-shadow: 0 0 8px var(--warn-glow); }
  .mc-accent::before { background: var(--accent); box-shadow: 0 0 8px var(--accent-glow); }

  .metric-header {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 9px;
    font-weight: 600;
    font-family: var(--font-mono);
    color: var(--text-3);
    letter-spacing: 1.5px;
    text-transform: uppercase;
    margin-bottom: 6px;
    padding-left: 8px;
  }

  .metric-value {
    font-size: 24px;
    font-weight: 600;
    font-family: var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-1);
    padding-left: 8px;
  }

  /* ── Equipment grid ──────────────────────────────── */
  .eq-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1px;
    background: var(--border);
    border-radius: var(--radius-sm);
    overflow: hidden;
  }

  .eq-cell {
    background: var(--surface);
    padding: 14px 16px;
    transition: all 0.3s ease;
  }

  .eq-cell.eq-on {
    border-left: 2px solid var(--ok);
    background: linear-gradient(90deg, var(--ok-dim) 0%, var(--surface) 40%);
  }

  .eq-cell.eq-idle {
    opacity: 0.55;
  }

  .eq-top {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 8px;
  }

  .eq-label {
    font-size: 13px;
    font-weight: 500;
    color: var(--text-2);
  }

  .eq-idle .eq-label {
    color: var(--text-4);
  }

  .eq-timer {
    font-size: 12px;
    font-family: var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-3);
    margin-top: 6px;
  }

  /* ── Chart card ──────────────────────────────────── */
  .chart-card {
    background: var(--surface);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    padding: 16px;
    position: relative;
    overflow: hidden;
  }

  .chart-card::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent 0%, var(--border-accent) 50%, transparent 100%);
  }

  /* ── Protection summary ──────────────────────────── */
  .prot-summary {
    display: flex;
    align-items: center;
    justify-content: space-between;
    background: var(--surface);
    border-radius: var(--radius-sm);
    border: 1px solid var(--border);
    padding: 14px 16px;
    cursor: pointer;
    width: 100%;
    font-family: var(--font-display);
    transition: all 0.15s ease;
  }

  .prot-summary:hover {
    background: var(--surface-2);
    border-color: var(--border-accent);
  }

  .prot-left {
    display: flex;
    align-items: center;
    gap: 10px;
    color: var(--text-2);
  }

  .prot-title {
    font-size: 14px;
    font-weight: 600;
    color: var(--text-1);
  }

  .prot-right {
    display: flex;
    align-items: center;
    gap: 10px;
    color: var(--text-3);
  }

  .prot-detail {
    font-size: 12px;
    color: var(--text-3);
  }

  /* ── Responsive ──────────────────────────────────── */
  @media (max-width: 480px) {
    .hero-num { font-size: 56px; letter-spacing: -3px; }
    .hero-unit { font-size: 22px; }
    .sp-value { font-size: 22px; }
    .metrics-row { grid-template-columns: 1fr; }
    .metric-value { font-size: 20px; }
    .hero-row { flex-direction: column; align-items: flex-start; }
    .hero-badges { flex-direction: row; align-items: center; }
  }
</style>
