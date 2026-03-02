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
    transition: opacity 0.2s;
  }
  .widget-row.is-pending { opacity: 0.6; }
  .label { font-size: 14px; color: var(--fg-muted); }
  .toggle {
    width: 48px; height: 26px;
    border-radius: 13px;
    border: none;
    background: var(--border);
    cursor: pointer;
    position: relative;
    transition: background 0.2s;
    padding: 0;
  }
  .toggle:disabled { cursor: wait; }
  .toggle.on { background: var(--success); }
  .toggle-thumb {
    position: absolute;
    top: 3px; left: 3px;
    width: 20px; height: 20px;
    border-radius: 50%;
    background: #fff;
    transition: transform 0.2s;
    box-shadow: 0 1px 3px rgba(0,0,0,0.3);
  }
  .toggle.on .toggle-thumb { transform: translateX(22px); }
</style>
