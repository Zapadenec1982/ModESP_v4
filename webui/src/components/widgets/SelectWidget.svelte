<script>
  import { apiPost } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';
  import { state } from '../../stores/state.js';

  export let config;
  export let value;

  let open = false;

  $: options = config.options || [];
  $: disabled = config.disabled || false;
  $: current = value !== undefined && value !== null ? value : '';

  // Runtime: перевірка requires_state через $state
  $: enriched = options.map(opt => ({
    ...opt,
    isDisabled: opt.requires_state ? !$state[opt.requires_state] : false
  }));

  $: currentOpt = enriched.find(o => o.value === current);
  $: currentLabel = currentOpt ? currentOpt.label : '—';
  $: hint = currentOpt?.isDisabled ? currentOpt.disabled_hint : null;

  function toggle() {
    if (!disabled) open = !open;
  }

  function select(opt) {
    if (opt.isDisabled) return;
    open = false;
    const nv = parseInt(opt.value);
    if (isNaN(nv)) return;
    setStateKey(config.key, nv);
    const endpoint = config.api_endpoint || '/api/settings';
    apiPost(endpoint, { [config.key]: nv }).catch(console.error);
  }

  function onWindowClick(e) {
    if (open && !e.target.closest('.select-widget')) open = false;
  }
</script>

<svelte:window on:click={onWindowClick} />

<div class="select-widget" class:disabled>
  <div class="select-label">{config.description || config.key}</div>
  <button class="select-trigger" class:open {disabled} on:click|stopPropagation={toggle}>
    <span class="trigger-text">{currentLabel}{currentOpt?.isDisabled ? ' ⊘' : ''}</span>
    <svg class="chevron" class:open viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <polyline points="6 9 12 15 18 9"/>
    </svg>
  </button>
  {#if open}
    <div class="dropdown">
      {#each enriched as opt}
        <button
          class="dropdown-item"
          class:active={opt.value === current}
          class:item-disabled={opt.isDisabled}
          on:click|stopPropagation={() => select(opt)}
        >
          {opt.label}{opt.isDisabled ? ' ⊘' : ''}
        </button>
      {/each}
    </div>
  {/if}
  {#if hint}
    <div class="disabled-hint">{hint}</div>
  {/if}
  {#if disabled && config.disabled_reason}
    <div class="disabled-reason">{config.disabled_reason}</div>
  {/if}
</div>

<style>
  .select-widget { padding: var(--widget-pad, 6px 0); position: relative; }
  .select-widget.disabled { opacity: 0.5; }
  .select-label { font-size: 14px; color: var(--fg-muted); margin-bottom: 8px; }
  .select-trigger {
    width: 100%;
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 10px 12px;
    font-size: 14px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 8px;
    color: var(--fg);
    cursor: pointer;
    text-align: left;
  }
  .select-trigger:focus { outline: none; border-color: var(--accent); }
  .select-trigger:disabled { cursor: not-allowed; }
  .select-trigger.open { border-color: var(--accent); border-radius: 8px 8px 0 0; }
  .trigger-text { flex: 1; }
  .chevron {
    width: 16px; height: 16px;
    color: var(--fg-muted);
    transition: transform 0.2s;
    flex-shrink: 0;
  }
  .chevron.open { transform: rotate(180deg); }
  .dropdown {
    position: absolute;
    left: 0; right: 0;
    z-index: 50;
    background: var(--bg);
    border: 1px solid var(--accent);
    border-top: none;
    border-radius: 0 0 8px 8px;
    max-height: 200px;
    overflow-y: auto;
    box-shadow: 0 4px 16px rgba(0,0,0,0.4);
  }
  .dropdown-item {
    width: 100%;
    padding: 10px 12px;
    font-size: 14px;
    background: none;
    border: none;
    color: var(--fg);
    cursor: pointer;
    text-align: left;
    transition: background 0.15s;
  }
  .dropdown-item:hover:not(.item-disabled) { background: var(--bg-hover, rgba(59,130,246,0.1)); }
  .dropdown-item.active {
    color: var(--accent);
    background: rgba(59,130,246,0.08);
    font-weight: 600;
  }
  .dropdown-item.item-disabled {
    color: var(--fg-dim, #64748b);
    cursor: not-allowed;
  }
  .disabled-hint { font-size: 11px; color: var(--warning, #f39c12); margin-top: 4px; }
  .disabled-reason { font-size: 11px; color: var(--danger, #e74c3c); margin-top: 4px; }
</style>
