<script>
  import { onMount, onDestroy } from 'svelte';
  import { state } from '../stores/state.js';
  import { apiGet } from '../lib/api.js';
  import { downsampleAvg } from '../lib/downsample.js';
  import { buildSmoothSegments, tempRange, computeTimeLabels, computeTempLabels } from '../lib/chart.js';

  const W = 654, H = 200;
  const PAD = { top: 12, right: 12, bottom: 24, left: 44 };
  const CW = W - PAD.left - PAD.right;
  const CH = H - PAD.top - PAD.bottom;

  let chartData = null;
  let refreshTimer;
  let liveTimer;

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

  $: pathSegs = buildSmoothSegments(sampled, airIdx, PAD, xFn, yFn);

  $: setpoint = $state['thermostat.setpoint'];
  $: spY = typeof setpoint === 'number' ? PAD.top + yFn(setpoint) : null;

  $: timeLabels = computeTimeLabels(tMin, tMax, 6, PAD.left, xFn);
  $: yLabels = computeTempLabels(vMin, vMax, 5, PAD.top, yFn);
</script>

{#if pts.length > 0}
  <div class="tile-chart">
    <svg viewBox="0 0 {W} {H}" class="mini-chart">
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
    </svg>
  </div>
{/if}

<style>
  .tile-chart {
    background: var(--card);
    border-radius: 12px;
    border: 1px solid var(--border);
    padding: 16px;
  }
  .mini-chart { width: 100%; display: block; }
  .mini-chart .mc-grid { stroke: var(--border); stroke-width: 0.5; }
  .mini-chart .mc-setpoint { stroke: #f59e0b; stroke-width: 1; stroke-dasharray: 4 2; opacity: 0.7; }
  .mini-chart .mc-axis { font-size: 11px; fill: var(--fg-muted); }

  @media (max-width: 480px) {
    .mini-chart { height: 180px; }
    .mini-chart .mc-axis { font-size: 16px; }
    .tile-chart { padding: 12px; }
  }
</style>
