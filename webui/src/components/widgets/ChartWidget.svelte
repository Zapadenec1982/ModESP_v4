<script>
  import { onMount, onDestroy } from 'svelte';
  import { apiGet } from '../../lib/api.js';
  import { state } from '../../stores/state.js';
  import { downsample } from '../../lib/downsample.js';

  export let config;
  export let value;

  let data = null;
  let loading = true;
  let error = null;
  let hours = (config && config.default_hours) || 24;
  let tooltip = null;
  let svgEl;
  let refreshTimer;

  // SVG dimensions
  const W = 720;
  const H = 280;
  const PAD = { top: 20, right: 16, bottom: 40, left: 50 };
  const CW = W - PAD.left - PAD.right;
  const CH = H - PAD.top - PAD.bottom;

  const MAX_POINTS = 720;

  // Event type constants (match C++ EventType enum)
  const COMP_ON = 1, COMP_OFF = 2;
  const DEF_START = 3, DEF_END = 4;
  const ALARM_HIGH = 5, ALARM_LOW = 6, ALARM_CLEAR = 7;
  const POWER_ON = 10;

  async function loadData() {
    loading = true;
    error = null;
    try {
      const src = (config && config.data_source) || '/api/log';
      data = await apiGet(`${src}?hours=${hours}`);
    } catch (e) {
      error = e.message;
      data = null;
    }
    loading = false;
  }

  onMount(() => {
    loadData();
    refreshTimer = setInterval(loadData, 300000);
  });
  onDestroy(() => { if (refreshTimer) clearInterval(refreshTimer); });

  function setHours(h) { hours = h; loadData(); }

  // Coordinate helpers
  function tsRange(pts) {
    if (!pts || pts.length === 0) return [0, 1];
    return [pts[0][0], pts[pts.length - 1][0]];
  }
  function tempRange(pts) {
    if (!pts || pts.length === 0) return [-40, 10];
    let mn = Infinity, mx = -Infinity;
    for (const p of pts) {
      const v = p[1] / 10;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    const margin = Math.max((mx - mn) * 0.1, 1);
    return [Math.floor(mn - margin), Math.ceil(mx + margin)];
  }
  function xScale(ts, tMin, tMax) {
    if (tMax === tMin) return CW / 2;
    return ((ts - tMin) / (tMax - tMin)) * CW;
  }
  function yScale(tempC, vMin, vMax) {
    if (vMax === vMin) return CH / 2;
    return CH - ((tempC - vMin) / (vMax - vMin)) * CH;
  }

  // Build zones from event pairs
  function buildZones(events, onType, offType, tMin, tMax) {
    const zones = [];
    let start = null;
    for (const [ts, type] of events) {
      if (type === onType) start = ts;
      else if (type === offType && start !== null) {
        zones.push({ x1: xScale(start, tMin, tMax), x2: xScale(ts, tMin, tMax) });
        start = null;
      }
    }
    if (start !== null) zones.push({ x1: xScale(start, tMin, tMax), x2: CW });
    return zones;
  }

  // Reactive calculations
  $: temp = data && data.temp ? data.temp : [];
  $: events = data && data.events ? data.events : [];
  $: sampled = downsample(temp, MAX_POINTS);
  $: [tMin, tMax] = tsRange(temp);
  $: [vMin, vMax] = tempRange(temp);

  // Polyline path
  $: polyline = sampled.map(p => {
    const x = PAD.left + xScale(p[0], tMin, tMax);
    const y = PAD.top + yScale(p[1] / 10, vMin, vMax);
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');

  // Zones
  $: compZones = buildZones(events, COMP_ON, COMP_OFF, tMin, tMax);
  $: defrostZones = buildZones(events, DEF_START, DEF_END, tMin, tMax);

  // Alarm markers
  $: alarmMarkers = events
    .filter(e => e[1] === ALARM_HIGH || e[1] === ALARM_LOW)
    .map(e => PAD.left + xScale(e[0], tMin, tMax));

  // Power-on markers
  $: powerMarkers = events
    .filter(e => e[1] === POWER_ON)
    .map(e => PAD.left + xScale(e[0], tMin, tMax));

  // Setpoint line
  $: setpoint = $state['thermostat.setpoint'];
  $: spY = setpoint != null ? PAD.top + yScale(setpoint, vMin, vMax) : null;

  // Time axis labels
  $: timeLabels = (() => {
    if (tMin === tMax) return [];
    const labels = [];
    const step = (tMax - tMin) / 6;
    for (let i = 0; i <= 6; i++) {
      const ts = tMin + step * i;
      const d = new Date(ts * 1000);
      const hh = String(d.getHours()).padStart(2, '0');
      const mm = String(d.getMinutes()).padStart(2, '0');
      labels.push({ x: PAD.left + xScale(ts, tMin, tMax), label: `${hh}:${mm}` });
    }
    return labels;
  })();

  // Temperature axis labels
  $: tempLabels = (() => {
    const labels = [];
    const step = Math.max(1, Math.round((vMax - vMin) / 5));
    for (let v = Math.ceil(vMin); v <= Math.floor(vMax); v += step) {
      labels.push({ y: PAD.top + yScale(v, vMin, vMax), label: `${v}°` });
    }
    return labels;
  })();

  // Y grid
  $: gridLines = tempLabels.map(l => l.y);

  // Tooltip on mouse/touch
  function handleMove(e) {
    if (!svgEl || !temp.length) return;
    const rect = svgEl.getBoundingClientRect();
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const mouseX = (clientX - rect.left) / rect.width * W - PAD.left;
    const ts = tMin + (mouseX / CW) * (tMax - tMin);
    // Find nearest point
    let best = null, bestDist = Infinity;
    for (const p of temp) {
      const d = Math.abs(p[0] - ts);
      if (d < bestDist) { bestDist = d; best = p; }
    }
    if (best) {
      const x = PAD.left + xScale(best[0], tMin, tMax);
      const y = PAD.top + yScale(best[1] / 10, vMin, vMax);
      const d = new Date(best[0] * 1000);
      const time = `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`;
      tooltip = { x, y, temp: (best[1] / 10).toFixed(1), time };
    }
  }
  function handleLeave() { tooltip = null; }
</script>

<div class="chart-container">
  <div class="chart-header">
    <span class="chart-title">{config.label || 'Температура'}</span>
    <div class="period-btns">
      <button class:active={hours === 24} on:click={() => setHours(24)}>24h</button>
      <button class:active={hours === 48} on:click={() => setHours(48)}>48h</button>
    </div>
  </div>

  {#if loading}
    <div class="chart-status">Завантаження...</div>
  {:else if error}
    <div class="chart-status chart-error">{error}</div>
  {:else if temp.length === 0}
    <div class="chart-status">Немає даних</div>
  {:else}
    <svg bind:this={svgEl} viewBox="0 0 {W} {H}" class="chart-svg"
         on:mousemove={handleMove} on:touchmove|preventDefault={handleMove}
         on:mouseleave={handleLeave} on:touchend={handleLeave}>

      <!-- Grid -->
      {#each gridLines as gy}
        <line x1={PAD.left} y1={gy} x2={W - PAD.right} y2={gy} class="grid" />
      {/each}

      <!-- Compressor zones -->
      {#each compZones as z}
        <rect x={PAD.left + z.x1} y={PAD.top} width={z.x2 - z.x1} height={CH}
              class="zone-comp" />
      {/each}

      <!-- Defrost zones -->
      {#each defrostZones as z}
        <rect x={PAD.left + z.x1} y={PAD.top} width={z.x2 - z.x1} height={CH}
              class="zone-defrost" />
      {/each}

      <!-- Alarm markers -->
      {#each alarmMarkers as ax}
        <line x1={ax} y1={PAD.top} x2={ax} y2={PAD.top + CH} class="alarm-mark" />
      {/each}

      <!-- Power-on markers -->
      {#each powerMarkers as px}
        <line x1={px} y1={PAD.top} x2={px} y2={PAD.top + CH} class="power-mark" />
      {/each}

      <!-- Setpoint line -->
      {#if spY != null}
        <line x1={PAD.left} y1={spY} x2={W - PAD.right} y2={spY} class="setpoint" />
      {/if}

      <!-- Temperature line -->
      {#if polyline}
        <polyline points={polyline} class="temp-line" />
      {/if}

      <!-- Y axis labels -->
      {#each tempLabels as tl}
        <text x={PAD.left - 6} y={tl.y + 4} class="axis-label" text-anchor="end">{tl.label}</text>
      {/each}

      <!-- X axis labels -->
      {#each timeLabels as xl}
        <text x={xl.x} y={H - PAD.bottom + 18} class="axis-label" text-anchor="middle">{xl.label}</text>
      {/each}

      <!-- Tooltip -->
      {#if tooltip}
        <circle cx={tooltip.x} cy={tooltip.y} r="3" class="tooltip-dot" />
        <rect x={tooltip.x + 8} y={tooltip.y - 24} width="90" height="20" rx="3"
              class="tooltip-bg" />
        <text x={tooltip.x + 12} y={tooltip.y - 10} class="tooltip-text">
          {tooltip.temp}°C  {tooltip.time}
        </text>
      {/if}

      <!-- Legend -->
      <rect x={PAD.left} y={H - 12} width="8" height="8" fill="#22c55e" opacity="0.5" />
      <text x={PAD.left + 12} y={H - 5} class="legend-text">Комп.</text>
      <rect x={PAD.left + 60} y={H - 12} width="8" height="8" fill="#f97316" opacity="0.5" />
      <text x={PAD.left + 72} y={H - 5} class="legend-text">Дефрост</text>
      <line x1={PAD.left + 130} y1={H - 8} x2={PAD.left + 148} y2={H - 8}
            stroke="#f59e0b" stroke-width="1" stroke-dasharray="4 2" />
      <text x={PAD.left + 152} y={H - 5} class="legend-text">Уставка</text>
    </svg>
  {/if}
</div>

<style>
  .chart-container {
    width: 100%;
  }
  .chart-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
  }
  .chart-title {
    font-size: 14px;
    color: var(--fg-muted);
  }
  .period-btns {
    display: flex;
    gap: 4px;
  }
  .period-btns button {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--fg-muted);
    padding: 2px 10px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
  }
  .period-btns button.active {
    background: var(--accent-bg);
    border-color: var(--accent);
    color: var(--accent);
  }
  .chart-status {
    text-align: center;
    padding: 40px 0;
    color: var(--fg-muted);
    font-size: 14px;
  }
  .chart-error { color: #ef4444; }
  .chart-svg {
    width: 100%;
    height: auto;
    display: block;
  }
  .chart-svg .grid { stroke: var(--border); stroke-width: 0.5; }
  .chart-svg .temp-line { fill: none; stroke: #3b82f6; stroke-width: 1.5; }
  .chart-svg .setpoint { stroke: #f59e0b; stroke-width: 1; stroke-dasharray: 4 2; }
  .chart-svg .zone-comp { fill: #22c55e; opacity: 0.15; }
  .chart-svg .zone-defrost { fill: #f97316; opacity: 0.2; }
  .chart-svg .alarm-mark { stroke: #ef4444; stroke-width: 1; opacity: 0.7; }
  .chart-svg .power-mark { stroke: #64748b; stroke-width: 1; stroke-dasharray: 2 2; opacity: 0.5; }
  .chart-svg .axis-label { font-size: 10px; fill: var(--fg-muted); }
  .chart-svg .legend-text { font-size: 9px; fill: var(--fg-muted); }
  .chart-svg .tooltip-dot { fill: #3b82f6; }
  .chart-svg .tooltip-bg { fill: var(--bg2); stroke: var(--border); stroke-width: 0.5; }
  .chart-svg .tooltip-text { font-size: 11px; fill: var(--fg); }
</style>
