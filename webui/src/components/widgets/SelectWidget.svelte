<script>
  import { onDestroy } from 'svelte';
  import { setStateKey } from '../../stores/state.js';
  import { state } from '../../stores/state.js';
  import { createSettingSender } from '../../lib/settings.js';

  export let config;
  export let value;

  $: options = config.options || [];
  $: disabled = config.disabled || false;
  $: current = value !== undefined && value !== null ? value : '';

  // Runtime: перевірка requires_state через $state
  $: enriched = options.map(opt => ({
    ...opt,
    isDisabled: opt.requires_state ? !$state[opt.requires_state] : false
  }));

  // Hint для поточного disabled значення (якщо NVS має старе значення)
  $: currentOpt = enriched.find(o => o.value === current);
  $: hint = currentOpt?.isDisabled ? currentOpt.disabled_hint : null;

  const sender = createSettingSender(config.key, {
    debounceMs: 0,
    endpoint: config.api_endpoint || '/api/settings'
  });
  const { pending, cleanup } = sender;
  onDestroy(cleanup);

  function onChange(e) {
    const nv = parseInt(e.target.value);
    if (isNaN(nv)) return;
    sender.send(nv);
  }
</script>

<div class="select-widget" class:disabled class:is-pending={$pending}>
  <div class="select-label">{config.description || config.key}</div>
  <select class="select-input" value={current} on:change={onChange} disabled={disabled || $pending}>
    {#each enriched as opt}
      <option value={opt.value} disabled={opt.isDisabled}
        title={opt.isDisabled && opt.disabled_hint ? opt.disabled_hint : ''}>
        {opt.label}
      </option>
    {/each}
  </select>
  {#if hint}
    <div class="disabled-hint">{hint}</div>
  {/if}
  {#if disabled && config.disabled_reason}
    <div class="disabled-reason">{config.disabled_reason}</div>
  {/if}
</div>

<style>
  .select-widget { padding: 4px 0; transition: opacity 0.2s ease; }
  .select-widget.disabled { opacity: 0.5; }
  .select-widget.is-pending { opacity: 0.6; }
  .select-label { font-size: 13px; color: var(--text-2); margin-bottom: 8px; }
  .select-input {
    width: 100%;
    padding: 10px 32px 10px 12px;
    font-size: 13px;
    font-weight: 500;
    font-family: var(--font-display);
    min-height: 44px;
    background: var(--bg-warm);
    border: 1px solid var(--border);
    border-radius: var(--radius-xs);
    color: var(--text-1);
    cursor: pointer;
    appearance: none;
    -webkit-appearance: none;
    background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%236b6f7e' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpolyline points='6 9 12 15 18 9'/%3E%3C/svg%3E");
    background-repeat: no-repeat;
    background-position: right 10px center;
    transition: border-color 0.15s ease;
  }
  .select-input:disabled { cursor: not-allowed; opacity: 0.5; }
  .select-input:hover { border-color: var(--border-accent); }
  .select-input:focus { outline: none; border-color: var(--accent); }
  .select-input option:disabled {
    color: var(--text-3);
    text-decoration: line-through;
  }
  .disabled-hint { font-size: 11px; color: var(--warn); margin-top: 4px; }
  .disabled-reason { font-size: 11px; color: var(--danger); margin-top: 4px; }
</style>
