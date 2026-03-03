<script>
  import { state } from '../stores/state.js';
  import { t } from '../stores/i18n.js';
  import { apiPost } from '../lib/api.js';
  import { toastSuccess, toastError } from '../stores/toast.js';
  import Icon from '../components/Icon.svelte';

  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];

  // Individual alarms
  $: checks = [
    { key: 'protection.high_temp_alarm', label: 'High Temp' },
    { key: 'protection.low_temp_alarm', label: 'Low Temp' },
    { key: 'protection.sensor1_alarm', label: 'Sensor 1' },
    { key: 'protection.sensor2_alarm', label: 'Sensor 2' },
    { key: 'protection.door_alarm', label: 'Door', requires: 'equipment.has_door_contact' },
    { key: 'protection.short_cycle_alarm', label: 'Short Cycle' },
    { key: 'protection.rapid_cycle_alarm', label: 'Rapid Cycle' },
    { key: 'protection.continuous_run_alarm', label: 'Continuous Run' },
    { key: 'protection.pulldown_alarm', label: 'Pulldown' },
    { key: 'protection.rate_alarm', label: 'Rate Rise' },
  ];

  $: activeChecks = checks.filter(c => !c.requires || $state[c.requires]);
  $: passedCount = activeChecks.filter(c => !$state[c.key]).length;

  // Diagnostics
  $: starts = $state['protection.compressor_starts_1h'] || 0;
  $: duty = $state['protection.compressor_duty'] || 0;
  $: runTime = $state['protection.compressor_run_time'] || 0;
  $: lastRun = $state['protection.last_cycle_run'] || 0;
  $: lastOff = $state['protection.last_cycle_off'] || 0;
  $: compHours = $state['protection.compressor_hours'] || 0;

  let resetting = false;

  async function resetAlarms() {
    resetting = true;
    try {
      await apiPost('/api/settings', { 'protection.reset_alarms': true });
      toastSuccess($t['alert.saved']);
    } catch (e) {
      toastError(e.message);
    } finally {
      resetting = false;
    }
  }

  function fmtDuration(sec) {
    if (typeof sec !== 'number' || sec < 0) return '—';
    const s = Math.floor(sec);
    const m = Math.floor(s / 60);
    const ss = s % 60;
    if (m >= 60) {
      const h = Math.floor(m / 60);
      return `${h}h ${m % 60}m`;
    }
    return `${m}:${String(ss).padStart(2, '0')}`;
  }

  function fmtHours(h) {
    if (typeof h !== 'number') return '—';
    return h.toFixed(1) + 'h';
  }
</script>

<div class="prot-page">
  <!-- Status hero -->
  {#if !alarmActive}
    <div class="status-hero ok">
      <div class="status-circle ok">
        <Icon name="check-circle" size={32} />
      </div>
      <div class="status-text">{$t['prot.all_ok']}</div>
      <div class="status-sub">{passedCount} {$t['prot.checks_passed']}</div>
    </div>
  {:else}
    <div class="status-hero alarm">
      <div class="alarm-card">
        <div class="alarm-header">
          <Icon name="shield-alert" size={24} />
          <span class="alarm-code">
            {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : $t['state.alarm']}
          </span>
          <span class="badge danger">
            <span class="badge-dot"></span>
            {$t['state.alarm']}
          </span>
        </div>
        <button class="reset-btn" on:click={resetAlarms} disabled={resetting}>
          {resetting ? '...' : $t['prot.reset']}
        </button>
      </div>
    </div>
  {/if}

  <!-- Check chips -->
  <div class="checks-grid">
    {#each activeChecks as check}
      <div class="check-chip" class:check-alarm={$state[check.key]}>
        <span class="check-dot" class:alarm={$state[check.key]}></span>
        <span class="check-label">{check.label}</span>
      </div>
    {/each}
  </div>

  <!-- Diagnostics -->
  <div class="diag-card">
    <div class="diag-title">{$t['prot.diagnostics']}</div>
    <div class="diag-grid">
      <div class="diag-item">
        <span class="diag-label">{$t['diag.starts']}</span>
        <span class="diag-value">{starts}</span>
      </div>
      <div class="diag-item">
        <span class="diag-label">{$t['diag.duty']}</span>
        <span class="diag-value">{duty}%</span>
      </div>
      <div class="diag-item">
        <span class="diag-label">{$t['diag.runtime']}</span>
        <span class="diag-value">{fmtDuration(runTime)}</span>
      </div>
      <div class="diag-item">
        <span class="diag-label">{$t['diag.hours']}</span>
        <span class="diag-value">{fmtHours(compHours)}</span>
      </div>
      <div class="diag-item">
        <span class="diag-label">{$t['diag.last_run']}</span>
        <span class="diag-value">{fmtDuration(lastRun)}</span>
      </div>
      <div class="diag-item">
        <span class="diag-label">{$t['diag.last_off']}</span>
        <span class="diag-value">{fmtDuration(lastOff)}</span>
      </div>
    </div>
  </div>
</div>

<style>
  .prot-page {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  /* ── Status hero ─────────────────────────────────── */
  .status-hero {
    text-align: center;
    padding: 32px 24px;
    background: var(--surface);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    position: relative;
    overflow: hidden;
  }

  .status-hero::after {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent, var(--ok-border), transparent);
  }

  .status-hero.alarm::after {
    background: linear-gradient(90deg, transparent, var(--danger-border), transparent);
  }

  .status-circle {
    width: 64px;
    height: 64px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    margin: 0 auto 16px;
  }

  .status-circle.ok {
    background: var(--ok-dim);
    color: var(--ok);
    box-shadow: 0 0 24px var(--ok-glow);
  }

  .status-text {
    font-size: 18px;
    font-weight: 600;
    color: var(--text-1);
    margin-bottom: 4px;
  }

  .status-sub {
    font-size: 13px;
    color: var(--text-3);
  }

  /* ── Alarm card ──────────────────────────────────── */
  .alarm-card {
    background: var(--surface);
    border-radius: var(--radius-sm);
    border: 1px solid var(--danger-border);
    padding: 20px;
    background: linear-gradient(135deg, var(--danger-dim) 0%, var(--surface) 100%);
  }

  .alarm-header {
    display: flex;
    align-items: center;
    gap: 12px;
    color: var(--danger);
    margin-bottom: 16px;
  }

  .alarm-code {
    font-size: 16px;
    font-weight: 700;
    font-family: var(--font-mono);
    letter-spacing: 1px;
    flex: 1;
    text-align: left;
  }

  .badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 3px 8px;
    border-radius: 20px;
    font-size: 10px;
    font-weight: 600;
    font-family: var(--font-mono);
    text-transform: uppercase;
  }

  .badge.danger {
    background: var(--danger-dim);
    color: var(--danger);
    border: 1px solid var(--danger-border);
  }

  .badge-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--danger);
    animation: dotPulse 2s infinite;
  }

  @keyframes dotPulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
  }

  .reset-btn {
    background: var(--accent);
    color: #fff;
    border: none;
    border-radius: var(--radius-xs);
    padding: 10px 24px;
    font-size: 14px;
    font-weight: 600;
    font-family: var(--font-display);
    cursor: pointer;
    min-height: 44px;
    width: 100%;
    transition: opacity 0.15s ease;
  }

  .reset-btn:hover { opacity: 0.9; }
  .reset-btn:disabled { opacity: 0.5; cursor: wait; }

  /* ── Check chips ─────────────────────────────────── */
  .checks-grid {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }

  .check-chip {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 6px 12px;
    border-radius: 20px;
    background: var(--surface);
    border: 1px solid var(--border);
    font-size: 12px;
    font-weight: 500;
    color: var(--text-2);
  }

  .check-chip.check-alarm {
    background: var(--danger-dim);
    border-color: var(--danger-border);
    color: var(--danger);
  }

  .check-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--ok);
    box-shadow: 0 0 6px var(--ok-glow);
  }

  .check-dot.alarm {
    background: var(--danger);
    box-shadow: 0 0 6px var(--danger-glow);
    animation: dotPulse 2s infinite;
  }

  /* ── Diagnostics card ────────────────────────────── */
  .diag-card {
    background: var(--surface);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    padding: 16px;
    position: relative;
    overflow: hidden;
  }

  .diag-card::after {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent, var(--border-accent), transparent);
  }

  .diag-title {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-2);
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 14px;
  }

  .diag-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }

  .diag-item {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }

  .diag-label {
    font-size: 11px;
    color: var(--text-3);
  }

  .diag-value {
    font-size: 16px;
    font-weight: 600;
    font-family: var(--font-mono);
    font-variant-numeric: tabular-nums;
    color: var(--text-1);
  }
</style>
