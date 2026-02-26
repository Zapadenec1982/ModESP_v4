<script>
  import { onMount, onDestroy } from 'svelte';
  import { apiGet } from '../../lib/api.js';
  import { state } from '../../stores/state.js';
  import { t } from '../../stores/i18n.js';
  import { downsampleAvg } from '../../lib/downsample.js';
  import { buildSmoothSegments, tempRange as calcTempRange, computeTimeLabels, computeTempLabels } from '../../lib/chart.js';

  export let config;
  export let value;

  let data = null;
  let loading = true;
  let error = null;
  let hours = (config && config.default_hours) || 24;
  let tooltip = null;
  let svgEl;
  let refreshTimer;
  let liveTimer;
  let showEvents = false;

  // Channel name → state key (дзеркалює CHANNEL_DEFS з бекенду)
  const CH_STATE_KEYS = {
    air: 'equipment.air_temp', evap: 'equipment.evap_temp',
    cond: 'equipment.cond_temp', setpoint: 'thermostat.setpoint',
    humidity: 'equipment.humidity'
  };

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

  // Палітра кольорів для каналів
  const PALETTE = ['#3b82f6', '#10b981', '#f59e0b', '#f97316', '#8b5cf6', '#ec4899'];
  // i18n ключі для відомих каналів
  const CH_TKEYS = {
    air: 'chart.ch_air', evap: 'chart.ch_evap', cond: 'chart.ch_cond',
    setpoint: 'chart.ch_setpoint', humidity: 'chart.ch_humidity'
  };

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
    startLiveTimer();
  }

  // ── Real-time: додаємо нові точки з WebSocket state ──
  function startLiveTimer() {
    if (liveTimer) clearInterval(liveTimer);
    const interval = ($state['datalogger.sample_interval'] || 60) * 1000;
    liveTimer = setInterval(appendLivePoint, interval);
  }

  function appendLivePoint() {
    if (!data || !data.channels || !data.temp) return;
    const chs = data.channels;
    const now = Math.floor(Date.now() / 1000);
    const point = [now];
    let hasAny = false;
    for (const name of chs) {
      const key = CH_STATE_KEYS[name];
      const sv = key ? $state[key] : undefined;
      if (sv != null && typeof sv === 'number') {
        point.push(Math.round(sv * 10));
        hasAny = true;
      } else {
        point.push(null);
      }
    }
    if (!hasAny) return;
    // Видалити точки що вийшли за вікно
    const cutoff = now - hours * 3600;
    while (data.temp.length > 0 && data.temp[0][0] < cutoff) {
      data.temp.shift();
    }
    data.temp.push(point);
    data = data; // Svelte reactivity trigger
  }

  onMount(() => {
    loadData();
    refreshTimer = setInterval(loadData, 300000);
  });
  onDestroy(() => {
    if (refreshTimer) clearInterval(refreshTimer);
    if (liveTimer) clearInterval(liveTimer);
  });

  function setHours(h) { hours = h; loadData(); }

  // ── Динамічні канали з API response ──

  let channelShow = {};  // {name: bool} — стан toggles

  $: channels = (data?.channels || []).map((name, i) => {
    if (!(name in channelShow)) channelShow[name] = true;
    return {
      name,
      idx: i + 1,
      color: PALETTE[i % PALETTE.length],
      tkey: CH_TKEYS[name] || name,
    };
  });

  // Чи є дані для каналу (хоча б один не-null)
  function channelHasData(pts, idx) {
    if (!pts) return false;
    return pts.some(p => p[idx] != null);
  }

  // Видимі канали (мають дані + toggle on)
  $: visibleChannels = channels.filter(ch =>
    channelShow[ch.name] !== false && channelHasData(temp, ch.idx)
  );

  // ── Reactive data ──
  $: temp = data?.temp || [];
  $: events = data?.events || [];

  // Які канали мають дані
  $: channelsWithData = channels.filter(ch => channelHasData(temp, ch.idx));

  // Downsample по першому каналу (зазвичай air)
  $: primaryIdx = channels.length > 0 ? channels[0].idx : 1;
  $: sampled = downsampleAvg(temp, MAX_POINTS, primaryIdx);

  // Coordinate helpers
  function tsRange(pts) {
    if (!pts || pts.length === 0) return [0, 1];
    return [pts[0][0], pts[pts.length - 1][0]];
  }

  $: [tMin, tMax] = tsRange(temp);
  $: [vMin, vMax] = calcTempRange(temp, visibleChannels.map(ch => ch.idx), [-40, 10]);

  function xScale(ts, tMn, tMx) {
    if (tMx === tMn) return CW / 2;
    return ((ts - tMn) / (tMx - tMn)) * CW;
  }
  function yScale(tempC, vMn, vMx) {
    if (vMx === vMn) return CH / 2;
    return CH - ((tempC - vMn) / (vMx - vMn)) * CH;
  }

  // Build zones from event pairs
  function buildZones(evts, onType, offType) {
    const zones = [];
    let start = null;
    for (const [ts, type] of evts) {
      if (type === onType) start = ts;
      else if (type === offType && start !== null) {
        zones.push({ x1: xScale(start, tMin, tMax), x2: xScale(ts, tMin, tMax) });
        start = null;
      }
    }
    if (start !== null) zones.push({ x1: xScale(start, tMin, tMax), x2: CW });
    return zones;
  }

  // Smooth paths для кожного видимого каналу
  $: channelLines = visibleChannels.map(ch => ({
    ...ch,
    segments: buildSmoothSegments(sampled, ch.idx, PAD,
      ts => xScale(ts, tMin, tMax), v => yScale(v, vMin, vMax))
  }));

  // Zones
  $: compZones = buildZones(events, COMP_ON, COMP_OFF);
  $: defrostZones = buildZones(events, DEF_START, DEF_END);

  // Alarm markers
  $: alarmMarkers = events
    .filter(e => e[1] === ALARM_HIGH || e[1] === ALARM_LOW)
    .map(e => PAD.left + xScale(e[0], tMin, tMax));

  // Power-on markers
  $: powerMarkers = events
    .filter(e => e[1] === POWER_ON)
    .map(e => PAD.left + xScale(e[0], tMin, tMax));

  // Setpoint line (якщо setpoint НЕ логується як канал → показати з live state)
  $: hasSetpointChannel = channels.some(ch => ch.name === 'setpoint');
  $: setpoint = !hasSetpointChannel ? $state['thermostat.setpoint'] : null;
  $: spY = setpoint != null ? PAD.top + yScale(setpoint, vMin, vMax) : null;

  // Time axis labels
  $: timeLabels = computeTimeLabels(tMin, tMax, 6, PAD.left, ts => xScale(ts, tMin, tMax));

  // Temperature axis labels
  $: tempLabels = computeTempLabels(vMin, vMax, 5, PAD.top, v => yScale(v, vMin, vMax));

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
      label: $t[`event.${e[1]}`] || `Event #${e[1]}`,
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
    let best = null, bestDist = Infinity;
    for (const p of temp) {
      const d = Math.abs(p[0] - ts);
      if (d < bestDist) { bestDist = d; best = p; }
    }
    if (best) {
      const x = PAD.left + xScale(best[0], tMin, tMax);
      const firstVis = visibleChannels[0];
      const firstVal = firstVis && best[firstVis.idx] != null ? best[firstVis.idx] / 10 : null;
      const y = firstVal != null ? PAD.top + yScale(firstVal, vMin, vMax) : PAD.top + CH / 2;
      const d = new Date(best[0] * 1000);
      const time = `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`;
      const vals = visibleChannels
        .filter(ch => best[ch.idx] != null)
        .map(ch => `${($t[ch.tkey] || ch.name).slice(0,3)}:${(best[ch.idx]/10).toFixed(1)}°`);
      tooltip = { x, y, text: vals.join(' ') + '  ' + time, width: Math.max(90, vals.join(' ').length * 7 + 50) };
    }
  }
  function handleLeave() { tooltip = null; }

  // CSV export (client-side)
  function downloadCSV() {
    if (!data) return;
    let csv = 'timestamp,datetime';
    for (const ch of channelsWithData) csv += `,${ch.name}`;
    csv += '\n';

    for (const p of temp) {
      const d = new Date(p[0] * 1000);
      const dt = d.toISOString().replace('T', ' ').slice(0, 19);
      let line = `${p[0]},${dt}`;
      for (const ch of channelsWithData) {
        line += `,${p[ch.idx] != null ? (p[ch.idx]/10).toFixed(1) : ''}`;
      }
      csv += line + '\n';
    }

    csv += '\n# Events\ntimestamp,datetime,event_type,description\n';
    for (const e of events) {
      const d = new Date(e[0] * 1000);
      const dt = d.toISOString().replace('T', ' ').slice(0, 19);
      csv += `${e[0]},${dt},${e[1]},${$t[`event.${e[1]}`] || ''}\n`;
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
    <span class="chart-title">{config.label || $t['chart.title']}</span>
    <div class="header-controls">
      <div class="period-btns">
        <button class:active={hours === 24} on:click={() => setHours(24)}>24h</button>
        <button class:active={hours === 48} on:click={() => setHours(48)}>48h</button>
      </div>
      <button class="csv-btn" on:click={downloadCSV} title="CSV">CSV</button>
    </div>
  </div>

  <!-- Перемикачі каналів (якщо більше 1) -->
  {#if channelsWithData.length > 1}
    <div class="channel-toggles">
      {#each channelsWithData as ch}
        <label class="ch-toggle" style="--ch-color: {ch.color}">
          <input type="checkbox" bind:checked={channelShow[ch.name]} /> {$t[ch.tkey] || ch.name}
        </label>
      {/each}
    </div>
  {/if}

  {#if loading}
    <div class="chart-status">{$t['chart.loading']}</div>
  {:else if error}
    <div class="chart-status chart-error">{error}</div>
  {:else if temp.length === 0}
    <div class="chart-status">{$t['chart.no_data']}</div>
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

      <!-- Setpoint line (live з state, якщо НЕ логується як канал) -->
      {#if spY != null}
        <line x1={PAD.left} y1={spY} x2={W - PAD.right} y2={spY} class="setpoint" />
      {/if}

      <!-- Temperature lines (dynamic channels, smooth curves) -->
      {#each channelLines as ch}
        {#each ch.segments as seg}
          <path d={seg} fill="none" stroke={ch.color} stroke-width="1.5" />
        {/each}
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

      <!-- Legend (dynamic channels) -->
      {#each visibleChannels as ch, i}
        <rect x={PAD.left + i * 80} y={H - 12} width="8" height="8" fill={ch.color} />
        <text x={PAD.left + i * 80 + 12} y={H - 5} class="legend-text">{$t[ch.tkey] || ch.name}</text>
      {/each}
      <!-- Zone legend -->
      {#if true}
        {@const lx = PAD.left + visibleChannels.length * 80}
        <rect x={lx} y={H - 12} width="8" height="8" fill="#22c55e" opacity="0.5" />
        <text x={lx + 12} y={H - 5} class="legend-text">{$t['chart.legend_comp']}</text>
        <rect x={lx + 60} y={H - 12} width="8" height="8" fill="#f97316" opacity="0.5" />
        <text x={lx + 72} y={H - 5} class="legend-text">{$t['chart.legend_defrost']}</text>
        {#if spY != null}
          <line x1={lx + 130} y1={H - 8} x2={lx + 148} y2={H - 8}
                stroke="#f59e0b" stroke-width="1" stroke-dasharray="4 2" />
          <text x={lx + 152} y={H - 5} class="legend-text">{$t['chart.legend_setpoint']}</text>
        {/if}
      {/if}
    </svg>
  {/if}

  <!-- Список подій -->
  {#if events.length > 0}
    <details class="events-section" bind:open={showEvents}>
      <summary class="events-toggle">
        {$t['chart.events']} ({events.length})
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
    flex-wrap: wrap;
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
