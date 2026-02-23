<script>
  import { apiPost } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';

  export let config;
  export let value;

  $: min = config.min ?? -35;
  $: max = config.max ?? 0;
  $: step = config.step ?? 0.5;
  $: display = value !== undefined && value !== null ? value : '—';
  $: pct = typeof value === 'number' ? ((value - min) / (max - min)) * 100 : 0;

  function onChange(e) {
    const v = parseFloat(e.target.value);
    setStateKey(config.key, v);
    apiPost('/api/settings', { [config.key]: v }).catch(console.error);
  }
</script>

<div class="slider-wrap">
  <div class="slider-header">
    <span class="label">{config.description || config.key}{#if config.unit} ({config.unit}){/if}</span>
    <span class="slider-val">{display}</span>
  </div>
  <input
    type="range"
    {min} {max} {step}
    value={value ?? min}
    on:change={onChange}
    on:input={e => { display = e.target.value; }}
    style="background: linear-gradient(to right, var(--accent) {pct}%, var(--border) {pct}%)"
  />
</div>

<style>
  .slider-wrap { padding: var(--widget-pad, 6px 0); }
  .slider-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 8px;
  }
  .label { font-size: 14px; color: var(--fg-muted); }
  .slider-val {
    font-size: 22px;
    font-weight: 700;
    color: var(--accent);
    font-variant-numeric: tabular-nums;
  }
  input[type=range] {
    -webkit-appearance: none;
    appearance: none;
    width: 100%;
    height: 8px;
    border-radius: 4px;
    background: var(--border);
    outline: none;
    cursor: pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 22px; height: 22px;
    border-radius: 50%;
    background: var(--accent);
    border: 3px solid var(--card);
    box-shadow: 0 2px 6px rgba(0,0,0,0.3);
    cursor: pointer;
  }
  input[type=range]::-moz-range-thumb {
    width: 22px; height: 22px;
    border-radius: 50%;
    background: var(--accent);
    border: 3px solid var(--card);
    box-shadow: 0 2px 6px rgba(0,0,0,0.3);
    cursor: pointer;
  }
</style>
