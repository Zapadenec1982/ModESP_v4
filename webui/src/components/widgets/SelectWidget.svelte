<script>
  import { apiPost } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';

  export let config;
  export let value;

  $: options = config.options || [];
  $: disabled = config.disabled || false;
  $: current = value !== undefined && value !== null ? value : '';

  function onChange(e) {
    const nv = parseInt(e.target.value);
    if (isNaN(nv)) return;
    setStateKey(config.key, nv);
    const endpoint = config.api_endpoint || '/api/settings';
    apiPost(endpoint, { [config.key]: nv }).catch(console.error);
  }
</script>

<div class="select-widget" class:disabled>
  <div class="select-label">{config.description || config.key}</div>
  <select class="select-input" value={current} on:change={onChange} {disabled}>
    {#each options as opt}
      <option value={opt.value}>{opt.label}</option>
    {/each}
  </select>
  {#if disabled && config.disabled_reason}
    <div class="disabled-reason">{config.disabled_reason}</div>
  {/if}
</div>

<style>
  .select-widget { padding: 4px 0; }
  .select-widget.disabled { opacity: 0.5; }
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
  .disabled-reason { font-size: 11px; color: var(--danger, #e74c3c); margin-top: 4px; }
</style>
