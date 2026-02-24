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
  let showEvents = false;

  // Канали: показувати/ховати (тільки фронтенд)
  let showAir = true;
  let showEvap = true;
  let showCond = true;

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
  const DOOR_OPEN = 8, DOOR_CLOSE = 9;
  const POWER_ON = 10;

  const EVENT_LABELS = {
    1: 'Компресор ON', 2: 'Компресор OFF',
    3: 'Дефрост старт', 4: 'Дефрост кінець',
    5: 'Аварія: висока T', 6: 'Аварія: низька T', 7: 'Аварія знята',
    8: 'Двері відкрито', 9: 'Двері зачинено',
    10: 'Увімкнення'
  };

  // Канали: кольори
  const CH_AIR  = { name: 'air',  label: 'Камера',      color: '#3b82f6', idx: 1 };
  const CH_EVAP = { name: 'evap', label: 'Випарник',     color: '#10b981', idx: 2 };
  const CH_COND = { name: 'cond', label: 'Конденсатор',  color: '#f59e0b', idx: 3 };

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

  // Чи є дані для каналу (хоча б один не-null)
  function channelHasData(pts, idx) {
    if (!pts) return false;
    return pts.some(p => p[idx] != null);
  }

  // Coordinate helpers
  function tsRange(pts) {
    if (!pts || pts.length === 0) return [0, 1];
    return [pts[0][0], pts[pts.length - 1][0]];
  }

  function tempRange(pts, channels) {
    if (!pts || pts.length === 0) return [-40, 10];
    let mn = Infinity, mx = -Infinity;
    for (const p of pts) {
      for (const ch of channels) {
        const raw = p[ch.idx];
        if (raw == null) continue;
        const v = raw / 10;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
      }
    }
    if (mn === Infinity) return [-40, 10];
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

  // Build polyline для одного каналу
  function buildPolyline(pts, chIdx, tMin, tMax, vMin, vMax) {
    const segments = [];
    let seg = [];
    for (const p of pts) {
      const raw = p[chIdx];
      if (raw == null) {
        if (seg.length > 0) { segments.push(seg); seg = []; }
        continue;
      }
      const x = PAD.left + xScale(p[0], tMin, tMax);
      const y = PAD.top + yScale(raw / 10, vMin, vMax);
      seg.push(`${x.toFixed(1)},${y.toFixed(1)}`);
    }
    if (seg.length > 0) segments.push(seg);
    return segments;
  }

  // Visible channels
  $: visibleChannels = [
    ...(showAir && hasAir ? [CH_AIR] : []),
    ...(showEvap && hasEvap ? [CH_EVAP] : []),
    ...(showCond && hasCond ? [CH_COND] : [])
  ];

  // Reactive calculations
  $: temp = data && data.temp ? data.temp : [];
  $: events = data && data.events ? data.events : [];
  $: channels = data && data.channels ? data.channels : [];

  // Які канали мають дані
  $: hasAir  = channelHasData(temp, 1);
  $: hasEvap = channelHasData(temp, 2);
  $: hasCond = channelHasData(temp, 3);

  // Downsample по первинному каналу (air)
  $: sampled = downsample(temp, MAX_POINTS, 1);
  $: [tMin, tMax] = tsRange(temp);
  $: [vMin, vMax] = tempRange(temp, visibleChannels);

  // Polylines per channel
  $: airLines  = showAir && hasAir  ? buildPolyline(sampled, 1, tMin, tMax, vMin, vMax) : [];
  $: evapLines = showEvap && hasEvap ? buildPolyline(sampled, 2, tMin, tMax, vMin, vMax) : [];
  $: condLines = showCond && hasCond ? buildPolyline(sampled, 3, tMin, tMax, vMin, vMax) : [];

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

  // Event list (останні 50, у зворотному порядку)
  $: eventList = events.slice().reverse().slice(0, 50).map(e => {
    const d = new Date(e[0] * 1000);
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    const ss = String(d.getSeconds()).padStart(2, '0');
    const dd = String(d.getDate()).padStart(2, '0');
    const mo = String(d.getMonth() + 1).padStart(2, '0');
    return {
      time: `${dd}.${mo} ${hh}:${mm}:${ss}`,
      label: EVENT_LABELS[e[1]] || `Event #${e[1]}`,
      type: e[1]
    };
  });

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
      // Y по air (основному каналу)
      const airVal = best[1] != null ? best[1] / 10 : null;
      const y = airVal != null ? PAD.top + yScale(airVal, vMin, vMax) : PAD.top + CH / 2;
      const d = new Date(best[0] * 1000);
      const time = `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`;
      // Зібрати значення всіх каналів
      const vals = [];
      if (showAir && hasAir && best[1] != null) vals.push(`${(best[1]/10).toFixed(1)}°`);
      if (showEvap && hasEvap && best[2] != null) vals.push(`E:${(best[2]/10).toFixed(1)}°`);
      if (showCond && hasCond && best[3] != null) vals.push(`C:${(best[3]/10).toFixed(1)}°`);
      tooltip = { x, y, text: vals.join(' ') + '  ' + time, width: Math.max(90, vals.join(' ').length * 7 + 50) };
    }
  }
  function handleLeave() { tooltip = null; }

  // CSV export (client-side)
  function downloadCSV() {
    if (!data) return;
    let csv = 'timestamp,datetime,air_temp';
    if (hasEvap) csv += ',evap_temp';
    if (hasCond) csv += ',cond_temp';
    csv += '\n';

    for (const p of temp) {
      const d = new Date(p[0] * 1000);
      const dt = d.toISOString().replace('T', ' ').slice(0, 19);
      let line = `${p[0]},${dt},${p[1] != null ? (p[1]/10).toFixed(1) : ''}`;
      if (hasEvap) line += `,${p[2] != null ? (p[2]/10).toFixed(1) : ''}`;
      if (hasCond) line += `,${p[3] != null ? (p[3]/10).toFixed(1) : ''}`;
      csv += line + '\n';
    }

    // Додати events в окрему секцію
    csv += '\n# Events\ntimestamp,datetime,event_type,description\n';
    for (const e of events) {
      const d = new Date(e[0] * 1000);
      const dt = d.toISOString().replace('T', ' ').slice(0, 19);
      csv += `${e[0]},${dt},${e[1]},${EVENT_LABELS[e[1]] || ''}\n`;
    }

    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `datalog_${hours}h.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }
</script>

<div class="chart-container">
  <div class="chart-header">
    <span class="chart-title">{config.label || 'Температура'}</span>
    <div class="header-controls">
      <div class="period-btns">
        <button class:active={hours === 24} on:click={() => setHours(24)}>24h</button>
        <button class:active={hours === 48} on:click={() => setHours(48)}>48h</button>
      </div>
      <button class="csv-btn" on:click={downloadCSV} title="Завантажити CSV">CSV</button>
    </div>
  </div>

  <!-- Перемикачі каналів -->
  {#if hasEvap || hasCond}
    <div class="channel-toggles">
      <label class="ch-toggle" style="--ch-color: {CH_AIR.color}">
        <input type="checkbox" bind:checked={showAir} /> {CH_AIR.label}
      </label>
      {#if hasEvap}
        <label class="ch-toggle" style="--ch-color: {CH_EVAP.color}">
          <input type="checkbox" bind:checked={showEvap} /> {CH_EVAP.label}
        </label>
      {/if}
      {#if hasCond}
        <label class="ch-toggle" style="--ch-color: {CH_COND.color}">
          <input type="checkbox" bind:checked={showCond} /> {CH_COND.label}
        </label>
      {/if}
    </div>
  {/if}

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

      <!-- Temperature lines (multi-channel) -->
      {#each airLines as seg}
        <polyline points={seg.join(' ')} class="line-air" />
      {/each}
      {#each evapLines as seg}
        <polyline points={seg.join(' ')} class="line-evap" />
      {/each}
      {#each condLines as seg}
        <polyline points={seg.join(' ')} class="line-cond" />
      {/each}

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
        <rect x={tooltip.x + 8} y={tooltip.y - 24} width={tooltip.width} height="20" rx="3"
              class="tooltip-bg" />
        <text x={tooltip.x + 12} y={tooltip.y - 10} class="tooltip-text">
          {tooltip.text}
        </text>
      {/if}

      <!-- Legend -->
      {#if showAir && hasAir}
        <rect x={PAD.left} y={H - 12} width="8" height="8" fill={CH_AIR.color} />
        <text x={PAD.left + 12} y={H - 5} class="legend-text">{CH_AIR.label}</text>
      {/if}
      {#if showEvap && hasEvap}
        <rect x={PAD.left + 70} y={H - 12} width="8" height="8" fill={CH_EVAP.color} />
        <text x={PAD.left + 82} y={H - 5} class="legend-text">{CH_EVAP.label}</text>
      {/if}
      {#if showCond && hasCond}
        <rect x={PAD.left + 150} y={H - 12} width="8" height="8" fill={CH_COND.color} />
        <text x={PAD.left + 162} y={H - 5} class="legend-text">{CH_COND.label}</text>
      {/if}
      <!-- Зони легенда (зсув якщо мульти-канал) -->
      {#if true}
        {@const lx = PAD.left + (hasEvap || hasCond ? 240 : 0)}
        <rect x={lx} y={H - 12} width="8" height="8" fill="#22c55e" opacity="0.5" />
        <text x={lx + 12} y={H - 5} class="legend-text">Комп.</text>
        <rect x={lx + 60} y={H - 12} width="8" height="8" fill="#f97316" opacity="0.5" />
        <text x={lx + 72} y={H - 5} class="legend-text">Дефрост</text>
        <line x1={lx + 130} y1={H - 8} x2={lx + 148} y2={H - 8}
              stroke="#f59e0b" stroke-width="1" stroke-dasharray="4 2" />
        <text x={lx + 152} y={H - 5} class="legend-text">Уставка</text>
      {/if}
    </svg>
  {/if}

  <!-- Список подій -->
  {#if events.length > 0}
    <details class="events-section" bind:open={showEvents}>
      <summary class="events-toggle">
        Події ({events.length})
      </summary>
      <div class="events-list">
        {#each eventList as ev}
          <div class="event-row" class:event-alarm={ev.type === ALARM_HIGH || ev.type === ALARM_LOW}
               class:event-defrost={ev.type === DEF_START || ev.type === DEF_END}
               class:event-power={ev.type === POWER_ON}>
            <span class="event-time">{ev.time}</span>
            <span class="event-label">{ev.label}</span>
          </div>
        {/each}
      </div>
    </details>
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
  .header-controls {
    display: flex;
    gap: 8px;
    align-items: center;
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
  .csv-btn {
    background: transparent;
    border: 1px solid var(--border);
    color: var(--fg-muted);
    padding: 2px 10px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 12px;
  }
  .csv-btn:hover {
    background: var(--accent-bg);
    border-color: var(--accent);
    color: var(--accent);
  }

  /* Channel toggles */
  .channel-toggles {
    display: flex;
    gap: 12px;
    margin-bottom: 6px;
  }
  .ch-toggle {
    display: flex;
    align-items: center;
    gap: 4px;
    font-size: 12px;
    color: var(--ch-color);
    cursor: pointer;
  }
  .ch-toggle input[type="checkbox"] {
    accent-color: var(--ch-color);
    width: 14px;
    height: 14px;
    cursor: pointer;
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
  .chart-svg .line-air  { fill: none; stroke: #3b82f6; stroke-width: 1.5; }
  .chart-svg .line-evap { fill: none; stroke: #10b981; stroke-width: 1.5; }
  .chart-svg .line-cond { fill: none; stroke: #f59e0b; stroke-width: 1.5; }
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

  /* Events section */
  .events-section {
    margin-top: 12px;
    border-top: 1px solid var(--border);
  }
  .events-toggle {
    padding: 8px 0;
    font-size: 13px;
    color: var(--fg-muted);
    cursor: pointer;
    list-style: none;
  }
  .events-toggle::marker { content: ''; }
  .events-toggle::before {
    content: '\25b6  ';
    font-size: 10px;
  }
  details[open] > .events-toggle::before {
    content: '\25bc  ';
  }
  .events-list {
    max-height: 300px;
    overflow-y: auto;
    padding-bottom: 4px;
  }
  .event-row {
    display: flex;
    gap: 10px;
    padding: 3px 0;
    font-size: 12px;
    border-bottom: 1px solid var(--border);
  }
  .event-row:last-child { border-bottom: none; }
  .event-time {
    color: var(--fg-muted);
    font-family: monospace;
    white-space: nowrap;
    min-width: 110px;
  }
  .event-label {
    color: var(--fg);
  }
  .event-alarm .event-label { color: #ef4444; }
  .event-defrost .event-label { color: #f97316; }
  .event-power .event-label { color: #64748b; }
</style>
