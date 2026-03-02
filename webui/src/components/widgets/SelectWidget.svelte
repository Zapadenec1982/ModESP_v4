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
  .select-widget { padding: 4px 0; transition: opacity 0.2s; }
  .select-widget.disabled { opacity: 0.5; }
  .select-widget.is-pending { opacity: 0.6; }
  .select-label { font-size: 14px; color: var(--fg-muted); margin-bottom: 8px; }
  .select-input {
    width: 100%;
    padding: 8px 12px;
    font-size: 14px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--fg);
    cursor: pointer;
    appearance: auto;
  }
  .select-input:disabled { cursor: not-allowed; }
  .select-input:focus { outline: none; border-color: var(--accent); }
  .select-input option:disabled {
    color: var(--fg-muted);
    text-decoration: line-through;
  }
  .disabled-hint { font-size: 12px; color: var(--warning, #f59e0b); margin-top: 4px; }
  .disabled-reason { font-size: 12px; color: var(--danger, #ef4444); margin-top: 4px; }
</style>
