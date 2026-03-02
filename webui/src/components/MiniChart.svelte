<script>
  import { onMount, onDestroy } from 'svelte';
  import { state } from '../stores/state.js';
  import { navigateTo } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import { apiGet } from '../lib/api.js';
  import { downsampleAvg } from '../lib/downsample.js';
  import { buildSegments, tempRange, computeTimeLabels, computeTempLabels } from '../lib/chart.js';

  const W = 654, H = 220;
  const PAD = { top: 12, right: 12, bottom: 24, left: 44 };
  const CW = W - PAD.left - PAD.right;
  const CH = H - PAD.top - PAD.bottom;

  let chartData = null;
  let refreshTimer;
  let liveTimer;
  let svgEl;

  // Tooltip
  let tooltip = null;

  async function loadChart() {
    try { chartData = await apiGet('/api/log?hours=24'); } catch {}
    startLiveTimer();
  }

  function startLiveTimer() {
    if (liveTimer) clearInterval(liveTimer);
    const iv = ($state['datalogger.sample_interval'] || 60) * 1000;
    liveTimer = setInterval(appendPoint, iv);
  }

  function appendPoint() {
    if (!chartData?.channels || !chartData?.temp) return;
    const aIdx = chartData.channels.indexOf('air');
    if (aIdx < 0) return;
    const airVal = $state['equipment.air_temp'];
    if (airVal == null || typeof airVal !== 'number') return;
    const now = Math.floor(Date.now() / 1000);
    const point = new Array(chartData.channels.length + 1).fill(null);
    point[0] = now;
    point[aIdx + 1] = Math.round(airVal * 10);
    const cutoff = now - 24 * 3600;
    while (chartData.temp.length > 0 && chartData.temp[0][0] < cutoff) chartData.temp.shift();
    chartData.temp.push(point);
    chartData = chartData;
  }

  onMount(() => { loadChart(); refreshTimer = setInterval(loadChart, 300000); });
  onDestroy(() => { if (refreshTimer) clearInterval(refreshTimer); if (liveTimer) clearInterval(liveTimer); });

  $: airIdx = chartData?.channels ? chartData.channels.indexOf('air') + 1 : 1;
  $: pts = chartData?.temp || [];
  $: sampled = downsampleAvg(pts, 300, airIdx);

  $: tMin = pts.length > 0 ? pts[0][0] : 0;
  $: tMax = pts.length > 0 ? pts[pts.length - 1][0] : 1;
  $: [vMin, vMax] = tempRange(pts, [airIdx]);

  $: xFn = (ts) => tMax === tMin ? CW / 2 : ((ts - tMin) / (tMax - tMin)) * CW;
  $: yFn = (v) => vMax === vMin ? CH / 2 : CH - ((v - vMin) / (vMax - vMin)) * CH;

  $: pathSegs = buildSegments(sampled, airIdx, PAD, xFn, yFn);

  $: setpoint = $state['thermostat.setpoint'];
  $: spY = typeof setpoint === 'number' ? PAD.top + yFn(setpoint) : null;

  $: timeLabels = computeTimeLabels(tMin, tMax, 6, PAD.left, xFn);
  $: yLabels = computeTempLabels(vMin, vMax, 5, PAD.top, yFn);

  // Відносний час останньої точки
  $: lastTs = pts.length > 0 ? pts[pts.length - 1][0] : 0;
  $: agoMin = lastTs > 0 ? Math.max(0, Math.round((Date.now() / 1000 - lastTs) / 60)) : null;

  function handlePointer(e) {
    if (!svgEl || pts.length < 2) return;
    const rect = svgEl.getBoundingClientRect();
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const relX = (clientX - rect.left) / rect.width;
    const svgX = relX * W;
    const chartX = svgX - PAD.left;
    if (chartX < 0 || chartX > CW) { tooltip = null; return; }
    const ratio = chartX / CW;
    const targetTs = tMin + ratio * (tMax - tMin);
    let closest = null, closestDist = Infinity;
    for (const p of sampled) {
      const raw = p[airIdx];
      if (raw == null) continue;
      const dist = Math.abs(p[0] - targetTs);
      if (dist < closestDist) { closestDist = dist; closest = p; }
    }
    if (!closest) { tooltip = null; return; }
    const val = closest[airIdx] / 10;
    const d = new Date(closest[0] * 1000);
    const timeStr = `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}`;
    tooltip = {
      x: PAD.left + xFn(closest[0]),
      y: PAD.top + yFn(val),
      temp: val.toFixed(1) + '°C',
      time: timeStr
    };
  }

  function hideTooltip() { tooltip = null; }
</script>

{#if pts.length > 0}
  <div class="tile-chart">
    <svg bind:this={svgEl} viewBox="0 0 {W} {H}" class="mini-chart"
      on:click={handlePointer}
      on:touchstart|preventDefault={handlePointer}
      on:touchmove|preventDefault={handlePointer}
      on:mousemove={handlePointer}
      on:mouseleave={hideTooltip}
    >
      {#each yLabels as yl}
        <line x1={PAD.left} y1={yl.y} x2={W - PAD.right} y2={yl.y} class="mc-grid" />
        <text x={PAD.left - 4} y={yl.y + 3} class="mc-axis" text-anchor="end">{yl.label}</text>
      {/each}
      {#if spY != null}
        <line x1={PAD.left} y1={spY} x2={W - PAD.right} y2={spY} class="mc-setpoint" />
      {/if}
      {#each pathSegs as seg}
        <path d={seg} fill="none" stroke="#3b82f6" stroke-width="2.5" />
      {/each}
      {#each timeLabels as xl}
        <text x={xl.x} y={H - 4} class="mc-axis" text-anchor="middle">{xl.label}</text>
      {/each}
      {#if tooltip}
        <line x1={tooltip.x} y1={PAD.top} x2={tooltip.x} y2={H - PAD.bottom} class="mc-cross" />
        <circle cx={tooltip.x} cy={tooltip.y} r="5" class="mc-dot" />
        <rect x={tooltip.x - 42} y={tooltip.y - 32} width="84" height="28" rx="4" class="mc-tip-bg" />
        <text x={tooltip.x} y={tooltip.y - 20} class="mc-tip-val" text-anchor="middle">{tooltip.temp}</text>
        <text x={tooltip.x} y={tooltip.y - 10} class="mc-tip-time" text-anchor="middle">{tooltip.time}</text>
      {/if}
    </svg>
    <div class="chart-footer">
      {#if agoMin !== null}
        <span class="chart-ago">{agoMin} {$t['chart.min_ago']}</span>
      {/if}
      <button class="chart-link" on:click={() => $navigateTo = 'chart'}>
        {$t['chart.detail']} →
      </button>
    </div>
  </div>
{/if}

<style>
  .tile-chart {
    background: var(--card);
    border-radius: var(--radius-2xl);
    border: 1px solid var(--border);
    padding: var(--sp-4);
  }
  .mini-chart { width: 100%; display: block; cursor: crosshair; }
  .mini-chart .mc-grid { stroke: var(--border); stroke-width: 0.5; }
  .mini-chart .mc-setpoint { stroke: #f59e0b; stroke-width: 1; stroke-dasharray: 4 2; opacity: 0.7; }
  .mini-chart .mc-axis { font-size: 11px; fill: var(--fg-muted); }
  .mini-chart .mc-cross { stroke: var(--fg-muted); stroke-width: 0.5; stroke-dasharray: 2 2; opacity: 0.5; }
  .mini-chart .mc-dot { fill: #3b82f6; stroke: var(--card); stroke-width: 2; }
  .mini-chart .mc-tip-bg { fill: var(--bg2); stroke: var(--border); stroke-width: 0.5; }
  .mini-chart .mc-tip-val { font-size: 12px; fill: var(--fg); font-weight: 600; }
  .mini-chart .mc-tip-time { font-size: 10px; fill: var(--fg-muted); }

  .chart-footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-top: var(--sp-2);
    padding: 0 var(--sp-1);
  }
  .chart-ago {
    font-size: var(--text-xs);
    color: var(--fg-muted);
  }
  .chart-link {
    font-size: var(--text-sm);
    color: var(--accent);
    background: none;
    border: none;
    cursor: pointer;
    font-weight: var(--fw-semibold);
    padding: var(--sp-1) var(--sp-2);
    border-radius: var(--radius-sm);
    transition: background var(--transition-fast);
  }
  .chart-link:hover {
    background: var(--accent-bg);
  }

  @media (max-width: 480px) {
    .mini-chart .mc-axis { font-size: 16px; }
    .tile-chart { padding: var(--sp-3); }
  }
</style>
