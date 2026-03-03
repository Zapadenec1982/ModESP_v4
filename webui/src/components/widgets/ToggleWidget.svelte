<script>
  import { onDestroy } from 'svelte';
  import { setStateKey } from '../../stores/state.js';
  import { createSettingSender } from '../../lib/settings.js';

  export let config;
  export let value;

  $: isOn = !!value;

  const sender = createSettingSender(config.key, { debounceMs: 0 });
  const { pending, cleanup } = sender;
  onDestroy(cleanup);

  function toggle() {
    const nv = !isOn;
    if (config.form_only) {
      setStateKey(config.key, nv);
    } else {
      sender.send(nv);
    }
  }
</script>

<div class="widget-row" class:is-pending={$pending}>
  <span class="label">{config.description || config.key}</span>
  <button class="toggle" class:on={isOn} on:click={toggle} disabled={$pending}>
    <span class="toggle-thumb"></span>
  </button>
</div>

<style>
  .widget-row {
    display: flex; align-items: center; justify-content: space-between;
    min-height: 44px; padding: 4px 0;
    transition: opacity 0.2s ease;
  }
  .widget-row.is-pending { opacity: 0.6; }
  .label { font-size: 13px; color: var(--text-2); }
  .toggle {
    width: 48px; height: 28px;
    border-radius: 14px;
    border: 1px solid var(--border);
    background: var(--surface-3);
    cursor: pointer;
    position: relative;
    transition: background 0.3s cubic-bezier(0.16, 1, 0.3, 1),
                border-color 0.3s ease;
    padding: 0;
  }
  .toggle:disabled { cursor: wait; }
  .toggle.on {
    background: var(--ok);
    border-color: var(--ok-border);
  }
  .toggle-thumb {
    position: absolute;
    top: 3px; left: 3px;
    width: 22px; height: 22px;
    border-radius: 50%;
    background: #fff;
    transition: transform 0.3s cubic-bezier(0.16, 1, 0.3, 1);
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.3);
  }
  .toggle.on .toggle-thumb { transform: translateX(20px); }
</style>
