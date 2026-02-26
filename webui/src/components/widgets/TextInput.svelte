<script>
  import { wifiSsid } from '../../stores/wifiForm.js';

  export let config;
  export let value;

  // Text inputs store their value locally for form submission
  export let inputValue = '';

  $: if (value !== undefined && value !== null && !inputValue) {
    inputValue = String(value);
  }

  // Sync wifi SSID з shared store (замість DOM querySelector)
  $: if (config.key === 'wifi.ssid') wifiSsid.set(inputValue);
  $: if (config.key === 'wifi.ssid' && $wifiSsid !== inputValue) inputValue = $wifiSsid;
</script>

<div class="input-widget">
  <div class="input-label">{config.description || config.key}</div>
  <input
    type="text"
    class="text-input"
    placeholder={config.description || ''}
    bind:value={inputValue}
  />
</div>

<style>
  .input-widget { padding: 4px 0; }
  .input-label { font-size: 14px; color: var(--fg-muted); margin-bottom: 6px; }
  .text-input {
    background: var(--bg);
    border: 1px solid var(--border);
    color: var(--fg);
    border-radius: 8px;
    padding: 10px 12px;
    width: 100%;
    font-size: 14px;
    transition: border-color 0.2s;
  }
  .text-input:focus { border-color: var(--accent); outline: none; }
</style>
