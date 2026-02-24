<script>
  import { apiPost } from '../../lib/api.js';
  import { t } from '../../stores/i18n.js';

  export let config;

  let loading = false;

  async function save() {
    // Collect sibling inputs from same card via DOM
    const card = document.querySelector(`[data-widget-key="wifi.ssid"] input`);
    const passEl = document.querySelector(`[data-widget-key="wifi.password"] input`);
    const ssid = card?.value?.trim() || '';
    const password = passEl?.value || '';

    if (!ssid) { alert($t['alert.ssid_empty']); return; }

    loading = true;
    try {
      const r = await apiPost('/api/wifi', { ssid, password });
      if (r.ok) alert($t['alert.saved_restart']);
      else alert($t['alert.error']);
    } catch (e) {
      alert($t['alert.error'] + ': ' + e.message);
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
