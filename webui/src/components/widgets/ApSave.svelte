<script>
  import { apiGet, apiPost } from '../../lib/api.js';
  import { onMount } from 'svelte';

  export let config;

  let loading = false;

  onMount(async () => {
    try {
      const d = await apiGet('/api/wifi/ap');
      setTimeout(() => {
        setInput('wifi.ap_ssid', d.ssid || '');
        setInput('wifi.ap_channel', d.channel || 1);
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
    const ssid = getInput('wifi.ap_ssid').trim();
    const password = getInput('wifi.ap_password');
    const channel = parseInt(getInput('wifi.ap_channel')) || 1;

    if (!ssid) { alert('SSID не може бути порожнім'); return; }
    if (password && password.length < 8) {
      alert('Пароль повинен бути не менше 8 символів (або порожній для відкритої мережі)');
      return;
    }

    loading = true;
    try {
      const r = await apiPost('/api/wifi/ap', { ssid, password, channel });
      if (r.ok) alert('Збережено! Перезавантажте для застосування.');
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
    {loading ? '...' : (config.label || 'Зберегти AP')}
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
