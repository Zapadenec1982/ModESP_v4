<script>
  import { apiGet, apiPost } from '../../lib/api.js';
  import { setStateKey, state } from '../../stores/state.js';
  import { onMount } from 'svelte';

  export let config;

  let loading = false;

  onMount(async () => {
    try {
      const d = await apiGet('/api/mqtt');
      setStateKey('mqtt.connected', d.connected || false);
      setStateKey('mqtt.status', d.status || 'unknown');
      setStateKey('mqtt.broker', d.broker || '');
      setStateKey('mqtt.enabled', d.enabled || false);
      // Fill form inputs via DOM
      setTimeout(() => {
        setInput('mqtt.broker', d.broker || '');
        setInput('mqtt.user', d.user || '');
        setInput('mqtt.prefix', d.prefix || '');
        setInput('mqtt.port', d.port || 1883);
      }, 50);
    } catch (e) {}
  });

  function setInput(key, val) {
    const el = document.querySelector(`[data-widget-key="${key}"] input`);
    if (el) el.value = val;
  }

  function getInput(key) {
    const el = document.querySelector(`[data-widget-key="${key}"] input`);
    return el?.value ?? '';
  }

  async function save() {
    loading = true;
    const data = {
      broker: getInput('mqtt.broker').trim(),
      port: parseInt(getInput('mqtt.port')) || 1883,
      user: getInput('mqtt.user').trim(),
      password: getInput('mqtt.password'),
      prefix: getInput('mqtt.prefix').trim(),
      enabled: !!$state['mqtt.enabled'],
    };

    try {
      const r = await apiPost('/api/mqtt', data);
      if (r.ok) alert('Збережено! MQTT перепідключається...');
      else alert('Помилка збереження');
    } catch (e) {
      alert('Помилка: ' + e.message);
    } finally {
      loading = false;
    }
  }
</script>

<div class="save-widget">
  <button class="action-btn" disabled={loading} on:click={save}>
    {loading ? '...' : (config.label || 'Save')}
  </button>
</div>

<style>
  .save-widget { padding: 4px 0; margin-top: 6px; }
  .action-btn {
    background: linear-gradient(135deg, var(--accent), #0369a1);
    color: #fff; border: none;
    padding: 12px 24px; border-radius: 10px;
    cursor: pointer; font-size: 14px; font-weight: 600;
    width: 100%; transition: all 0.2s;
    box-shadow: 0 2px 8px rgba(59,130,246,0.3);
  }
  .action-btn:hover:not(:disabled) { transform: translateY(-1px); }
  .action-btn:disabled { opacity: 0.6; cursor: not-allowed; }
</style>
