<script>
  import { apiPost } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';
  import { toastError } from '../../stores/toast.js';

  export let config;
  export let value;

  $: min = config.min ?? -35;
  $: max = config.max ?? 0;
  $: step = config.step ?? 0.5;
  $: display = value !== undefined && value !== null ? Number(value) : null;

  let pending = false;
  let flashOk = false;
  let debounceTimer = null;

  function onInput(e) {
    const v = parseFloat(e.target.value);
    display = v;
    pending = true;
    if (debounceTimer) clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => sendValue(v), 300);
  }

  function onChange(e) {
    const v = parseFloat(e.target.value);
    display = v;
    if (debounceTimer) clearTimeout(debounceTimer);
    sendValue(v);
  }

  async function sendValue(v) {
    pending = true;
    setStateKey(config.key, v);
    try {
      await apiPost('/api/settings', { [config.key]: v });
      flashOk = true;
      setTimeout(() => { flashOk = false; }, 400);
    } catch (e) {
      toastError(e.message);
      display = value;
    } finally {
      pending = false;
    }
  }
</script>

<div class="slider-wrap" class:flash-ok={flashOk}>
  <div class="slider-header">
    <span class="label">{config.description || config.key}{#if config.unit} ({config.unit}){/if}</span>
    <span class="slider-val">
      {display !== null ? Number(display).toFixed(1) : '—'}
      {#if pending}<span class="pending-dot"></span>{/if}
    </span>
  </div>
  <input
    type="range"
    {min} {max} {step}
    value={display ?? min}
    on:input={onInput}
    on:change={onChange}
  />
  <div class="slider-labels">
    <span>{min}</span>
    <span>{max}</span>
  </div>
</div>

<style>
  .slider-wrap {
    padding: 4px 0;
    border-left: 3px solid transparent;
    transition: border-color 0.2s;
  }
  .slider-wrap.flash-ok {
    border-left-color: var(--success);
  }
  .slider-header {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    margin-bottom: 8px;
  }
  .label { font-size: var(--text-md); color: var(--fg-muted); }
  .slider-val {
    font-size: var(--text-2xl);
    font-weight: var(--fw-bold);
    color: var(--accent);
    font-variant-numeric: tabular-nums;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .pending-dot {
    width: 8px; height: 8px;
    border-radius: var(--radius-full);
    background: var(--warning);
    animation: pulse-dot 0.8s infinite;
  }
  @keyframes pulse-dot {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.3; }
  }

  input[type=range] {
    -webkit-appearance: none;
    appearance: none;
    width: 100%;
    height: 10px;
    border-radius: 5px;
    background: var(--border);
    outline: none;
    cursor: pointer;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 44px; height: 44px;
    border-radius: var(--radius-full);
    background: var(--accent);
    border: 3px solid var(--card);
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
    cursor: pointer;
  }
  input[type=range]::-moz-range-thumb {
    width: 44px; height: 44px;
    border-radius: var(--radius-full);
    background: var(--accent);
    border: 3px solid var(--card);
    box-shadow: 0 2px 8px rgba(0,0,0,0.3);
    cursor: pointer;
  }

  .slider-labels {
    display: flex;
    justify-content: space-between;
    font-size: var(--text-xs);
    color: var(--fg-muted);
    margin-top: 4px;
    padding: 0 2px;
  }
</style>
