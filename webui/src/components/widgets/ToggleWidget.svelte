<script>
  import { apiPost } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';

  export let config;
  export let value;

  $: isOn = !!value;

  function toggle() {
    const nv = !isOn;
    setStateKey(config.key, nv);
    if (!config.form_only) {
      const endpoint = config.api_endpoint || '/api/settings';
      apiPost(endpoint, { [config.key]: nv }).catch(console.error);
    }
  }
</script>

<div class="widget-row">
  <span class="label">{config.description || config.key}</span>
  <button class="toggle" class:on={isOn} on:click={toggle}>
    <span class="toggle-thumb"></span>
  </button>
</div>

<style>
  .widget-row {
    display: flex; align-items: center; justify-content: space-between;
    min-height: var(--widget-min-h, 44px); padding: var(--widget-pad, 6px 0);
  }
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
