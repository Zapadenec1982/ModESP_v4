<script>
  import { onDestroy } from 'svelte';
  import { setStateKey } from '../../stores/state.js';
  import { createSettingSender } from '../../lib/settings.js';

  export let config;
  export let value;

  $: min = config.min ?? 0;
  $: max = config.max ?? 100;
  $: step = config.step ?? 1;
  $: display = value !== undefined && value !== null ? value : '—';

  const sender = createSettingSender(config.key);
  const { pending, flashOk, cleanup } = sender;
  onDestroy(cleanup);

  function adjust(delta) {
    const cur = typeof value === 'number' ? value : 0;
    const nv = Math.round(Math.max(min, Math.min(max, cur + delta)) * 100) / 100;
    if (config.form_only) {
      setStateKey(config.key, nv);
    } else {
      sender.send(nv);
    }
  }

  function onInput(e) {
    const nv = Math.max(min, Math.min(max, parseFloat(e.target.value) || 0));
    if (config.form_only) {
      setStateKey(config.key, nv);
    } else {
      sender.send(nv);
    }
  }
</script>

<div class="number-widget" class:flash-ok={$flashOk}>
  <div class="number-label">{config.description || config.key}</div>
  <div class="number-row">
    <button class="num-btn" on:click={() => adjust(-step)}>−</button>
    <input
      type="number"
      class="num-display"
      {min} {max} {step}
      value={display}
      on:change={onInput}
    />
    <button class="num-btn" on:click={() => adjust(step)}>+</button>
    {#if config.unit}
      <span class="unit">{config.unit}</span>
    {/if}
    {#if $pending}
      <span class="pending-dot"></span>
    {/if}
  </div>
</div>

<style>
  .number-widget {
    padding: 4px 0;
    border-left: 3px solid transparent;
    transition: border-color 0.2s;
  }
  .number-widget.flash-ok {
    border-left-color: var(--success);
  }
  .number-label { font-size: 14px; color: var(--fg-muted); margin-bottom: 8px; }
  .number-row { display: flex; align-items: center; gap: 6px; }
  .num-btn {
    width: 44px; height: 44px; border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--border);
    color: var(--fg);
    font-size: 18px; font-weight: 600;
    cursor: pointer;
    display: flex; align-items: center; justify-content: center;
    transition: all 0.15s;
    line-height: 1;
  }
  .num-btn:hover { background: var(--accent); border-color: var(--accent); color: #fff; }
  .num-btn:active { transform: scale(0.92); }
  .num-display {
    width: 80px;
    text-align: center;
    font-size: 16px; font-weight: 600;
    font-variant-numeric: tabular-nums;
    padding: 6px 8px;
    background: var(--bg);
    border-radius: 6px;
    border: 1px solid var(--border);
    color: var(--fg);
    -moz-appearance: textfield;
  }
  .num-display::-webkit-inner-spin-button,
  .num-display::-webkit-outer-spin-button {
    -webkit-appearance: none;
    margin: 0;
  }
  .unit { font-size: 12px; color: var(--fg-muted); margin-left: 2px; }
  .pending-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--warning, #f59e0b);
    animation: pulse-dot 0.8s infinite;
  }
  @keyframes pulse-dot {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.3; }
  }
</style>
