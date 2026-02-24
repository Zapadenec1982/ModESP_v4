<script>
  import { onMount, onDestroy } from 'svelte';
  import { state } from '../stores/state.js';
  import { stateMeta } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import { apiGet } from '../lib/api.js';
  import { downsample } from '../lib/downsample.js';
  import SliderWidget from '../components/widgets/SliderWidget.svelte';

  $: displayTemp = $state['thermostat.display_temp'];
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
  $: nightActive = $state['thermostat.night_active'];
  $: showDefrostSymbol = typeof displayTemp === 'number' && displayTemp <= -900;

  // ── Mini chart: temperature in chamber ──
  const MC_W = 654, MC_H = 200;
  const MC_PAD = { top: 12, right: 12, bottom: 24, left: 44 };
  const MC_CW = MC_W - MC_PAD.left - MC_PAD.right;
  const MC_CH = MC_H - MC_PAD.top - MC_PAD.bottom;

  let chartData = null;
  let chartRefresh;
  let chartLive;

  async function loadChart() {
    try {
      chartData = await apiGet('/api/log?hours=24');
    } catch (e) { /* ignore */ }
    startChartLive();
  }

  function startChartLive() {
    if (chartLive) clearInterval(chartLive);
    const iv = ($state['datalogger.sample_interval'] || 60) * 1000;
    chartLive = setInterval(appendChartPoint, iv);
  }

  function appendChartPoint() {
    if (!chartData?.channels || !chartData?.temp) return;
    const airIdx = chartData.channels.indexOf('air');
    if (airIdx < 0) return;
    const airVal = $state['equipment.air_temp'];
    if (airVal == null || typeof airVal !== 'number') return;
    const now = Math.floor(Date.now() / 1000);
    const point = new Array(chartData.channels.length + 1).fill(null);
    point[0] = now;
    point[airIdx + 1] = Math.round(airVal * 10);
    const cutoff = now - 24 * 3600;
    while (chartData.temp.length > 0 && chartData.temp[0][0] < cutoff) chartData.temp.shift();
    chartData.temp.push(point);
    chartData = chartData;
  }

  onMount(() => { loadChart(); chartRefresh = setInterval(loadChart, 300000); });
  onDestroy(() => { if (chartRefresh) clearInterval(chartRefresh); if (chartLive) clearInterval(chartLive); });

  // Reactive chart computations
  $: mcAirIdx = chartData?.channels ? chartData.channels.indexOf('air') + 1 : 1;
  $: mcTemp = chartData?.temp || [];
  $: mcSampled = downsample(mcTemp, 600, mcAirIdx);

  function mcRange(pts, idx) {
    let mn = Infinity, mx = -Infinity;
    for (const p of pts) {
      const r = p[idx];
      if (r == null) continue;
      const v = r / 10;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    if (mn === Infinity) return [-25, -15];
    const m = Math.max((mx - mn) * 0.1, 1);
    return [Math.floor(mn - m), Math.ceil(mx + m)];
  }

  $: mcTMin = mcTemp.length > 0 ? mcTemp[0][0] : 0;
  $: mcTMax = mcTemp.length > 0 ? mcTemp[mcTemp.length - 1][0] : 1;
  $: [mcVMin, mcVMax] = mcRange(mcTemp, mcAirIdx);

  function mcX(ts) { return mcTMax === mcTMin ? MC_CW / 2 : ((ts - mcTMin) / (mcTMax - mcTMin)) * MC_CW; }
  function mcY(v) { return mcVMax === mcVMin ? MC_CH / 2 : MC_CH - ((v - mcVMin) / (mcVMax - mcVMin)) * MC_CH; }

  function mcCatmull(points) {
    if (points.length < 2) return '';
    if (points.length === 2) return `M${points[0].x},${points[0].y} L${points[1].x},${points[1].y}`;
    let d = `M${points[0].x},${points[0].y}`;
    for (let i = 0; i < points.length - 1; i++) {
      const p0 = points[Math.max(0, i - 1)];
      const p1 = points[i];
      const p2 = points[i + 1];
      const p3 = points[Math.min(points.length - 1, i + 2)];
      d += ` C${(p1.x + (p2.x - p0.x) / 6).toFixed(1)},${(p1.y + (p2.y - p0.y) / 6).toFixed(1)} ${(p2.x - (p3.x - p1.x) / 6).toFixed(1)},${(p2.y - (p3.y - p1.y) / 6).toFixed(1)} ${p2.x},${p2.y}`;
    }
    return d;
  }

  $: mcPath = (() => {
    const segs = [];
    let seg = [];
    for (const p of mcSampled) {
      const raw = p[mcAirIdx];
      if (raw == null) { if (seg.length) { segs.push(mcCatmull(seg)); seg = []; } continue; }
      seg.push({ x: +(MC_PAD.left + mcX(p[0])).toFixed(1), y: +(MC_PAD.top + mcY(raw / 10)).toFixed(1) });
    }
    if (seg.length) segs.push(mcCatmull(seg));
    return segs;
  })();

  // Setpoint line Y
  $: mcSpY = typeof setpoint === 'number' ? MC_PAD.top + mcY(setpoint) : null;

  // Time labels (6 labels)
  $: mcTimeLabels = (() => {
    if (mcTMin === mcTMax) return [];
    const labels = [];
    for (let i = 0; i <= 6; i++) {
      const ts = mcTMin + (mcTMax - mcTMin) * i / 6;
      const d = new Date(ts * 1000);
      labels.push({ x: MC_PAD.left + mcX(ts), label: `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}` });
    }
    return labels;
  })();

  // Y axis labels
  $: mcYLabels = (() => {
    const labels = [];
    const step = Math.max(1, Math.round((mcVMax - mcVMin) / 5));
    for (let v = Math.ceil(mcVMin); v <= Math.floor(mcVMax); v += step) {
      labels.push({ y: MC_PAD.top + mcY(v), label: `${v}°` });
    }
    return labels;
  })();

  $: sp = typeof setpoint === 'number' ? setpoint : -18;
  $: shownTemp = showDefrostSymbol ? null : (typeof displayTemp === 'number' ? displayTemp : temperature);
  $: tempColor = getTemperatureColor(shownTemp, sp);

  function getTemperatureColor(temp, sp) {
    if (typeof temp !== 'number') return 'var(--fg-muted)';
    if (temp < sp - 2) return '#3b82f6';
    if (temp <= sp + 2) return '#22c55e';
    if (temp <= sp + 5) return '#f59e0b';
    return '#ef4444';
  }

  const stateKeys = {
    idle: 'state.idle', cooling: 'state.cooling',
    safe_mode: 'state.safety_run', startup: 'state.startup'
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
      <div class="status-item" title={$t['dash.fan']}>
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
        <div class="status-item" title={$t['dash.heater']}>
          <svg class="status-icon" viewBox="0 0 24 24" fill="none" stroke="#ef4444" stroke-width="2">
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z" fill="rgba(239,68,68,0.2)"/>
            <path d="M12 2c0 4-4 6-4 10a4 4 0 0 0 8 0c0-4-4-6-4-10z"/>
          </svg>
        </div>
      {/if}

      <!-- Divider -->
      <div class="status-divider"></div>

      <!-- State badge: defrost overrides thermostat -->
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

    <!-- Alarm banner -->
    {#if alarmActive}
      <div class="alarm-banner">
        {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : $t['state.alarm']}
      </div>
    {/if}
  </div>

  <!-- Setpoint tile -->
  <div class="tile tile-setpoint">
    <div class="sp-header">
      <span class="sp-label">{$t['dash.setpoint']}</span>
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

  <!-- Mini chart: chamber temperature -->
  {#if mcTemp.length > 0}
    <div class="tile tile-chart">
      <svg viewBox="0 0 {MC_W} {MC_H}" class="mini-chart">
        <!-- Grid -->
        {#each mcYLabels as yl}
          <line x1={MC_PAD.left} y1={yl.y} x2={MC_W - MC_PAD.right} y2={yl.y} class="mc-grid" />
          <text x={MC_PAD.left - 4} y={yl.y + 3} class="mc-axis" text-anchor="end">{yl.label}</text>
        {/each}
        <!-- Setpoint dashed line -->
        {#if mcSpY != null}
          <line x1={MC_PAD.left} y1={mcSpY} x2={MC_W - MC_PAD.right} y2={mcSpY} class="mc-setpoint" />
        {/if}
        <!-- Temperature line -->
        {#each mcPath as seg}
          <path d={seg} fill="none" stroke="#3b82f6" stroke-width="2.5" />
        {/each}
        <!-- X axis labels -->
        {#each mcTimeLabels as xl}
          <text x={xl.x} y={MC_H - 4} class="mc-axis" text-anchor="middle">{xl.label}</text>
        {/each}
      </svg>
    </div>
  {/if}
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

  .night-label {
    color: #8b5cf6;
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

  /* Mini chart */
  .tile-chart { padding: 16px; }
  .mini-chart { width: 100%; display: block; }
  .mini-chart .mc-grid { stroke: var(--border); stroke-width: 0.5; }
  .mini-chart .mc-setpoint { stroke: #f59e0b; stroke-width: 1; stroke-dasharray: 4 2; opacity: 0.7; }
  .mini-chart .mc-axis { font-size: 11px; fill: var(--fg-muted); }

  @media (max-width: 480px) {
    .temp-value { font-size: 48px; }
    .sp-value { font-size: 20px; }
    .tile { padding: 16px; }
    .status-icon { width: 24px; height: 24px; }
    .mini-chart { height: 180px; }
    .mini-chart .mc-axis { font-size: 16px; }
    .tile-chart { padding: 12px; }
  }
</style>
