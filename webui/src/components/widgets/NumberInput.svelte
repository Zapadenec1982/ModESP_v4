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
    transition: opacity 0.2s ease;
  }
  .number-widget.flash-ok {
    animation: valPulse 0.4s ease;
  }
  @keyframes valPulse {
    0% { opacity: 1; }
    50% { opacity: 0.6; }
    100% { opacity: 1; }
  }
  .number-label { font-size: 13px; color: var(--text-2); margin-bottom: 8px; }
  .number-row { display: flex; align-items: center; gap: 0; }
  .num-btn {
    width: 38px; height: 38px;
    border-radius: var(--radius-xs);
    border: 1px solid var(--border);
    background: transparent;
    color: var(--accent);
    font-size: 18px; font-weight: 600;
    cursor: pointer;
    display: flex; align-items: center; justify-content: center;
    transition: all 0.15s ease;
    line-height: 1;
    flex-shrink: 0;
  }
  .num-btn:hover { background: var(--accent-dim); }
  .num-btn:active { background: var(--accent-glow); transform: scale(0.95); }
  .num-display {
    min-width: 52px;
    text-align: center;
    font-size: 14px; font-weight: 600;
    font-family: var(--font-mono);
    font-variant-numeric: tabular-nums;
    padding: 8px 8px;
    background: var(--bg-warm);
    border-top: 1px solid var(--border);
    border-bottom: 1px solid var(--border);
    border-left: none;
    border-right: none;
    color: var(--text-1);
    -moz-appearance: textfield;
  }
  .num-display::-webkit-inner-spin-button,
  .num-display::-webkit-outer-spin-button {
    -webkit-appearance: none;
    margin: 0;
  }
  .num-display:focus {
    outline: none;
    background: var(--surface-2);
  }
  .unit { font-size: 11px; color: var(--text-4); margin-left: 8px; }
  .pending-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--warn);
    animation: pulse-dot 0.8s infinite;
  }
  @keyframes pulse-dot {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.3; }
  }
</style>
