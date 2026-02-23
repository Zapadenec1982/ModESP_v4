<script>
  import { apiPost } from '../../lib/api.js';

  export let config;

  let loading = false;

  async function onClick() {
    if (config.confirm && !confirm(config.confirm)) return;
    loading = true;
    try {
      if (config.api_endpoint) {
        // Прямий API виклик (restart, ota тощо)
        await apiPost(config.api_endpoint, {});
      } else if (config.key) {
        // AUDIT-005: state key toggle через /api/settings (manual defrost, reset alarms)
        await apiPost('/api/settings', { [config.key]: true });
      }
    } catch (e) {
      alert('Error: ' + e.message);
    } finally {
      loading = false;
    }
  }
</script>

<div class="btn-widget">
  <button
    class="action-btn"
    class:danger={!!config.confirm}
    disabled={loading}
    on:click={onClick}
  >
    {loading ? '...' : (config.label || 'Action')}
  </button>
</div>

<style>
  .btn-widget { padding: var(--widget-pad, 6px 0); }
  .action-btn {
    background: linear-gradient(135deg, var(--accent), #0369a1);
    color: #fff; border: none;
    padding: 12px 24px; border-radius: 10px;
    cursor: pointer; font-size: 14px; font-weight: 600;
    width: 100%; transition: all 0.2s;
    box-shadow: 0 2px 8px rgba(59,130,246,0.3);
  }
  .action-btn:hover:not(:disabled) {
    transform: translateY(-1px);
    box-shadow: 0 4px 12px rgba(59,130,246,0.4);
  }
  .action-btn:active:not(:disabled) { transform: translateY(0); }
  .action-btn:disabled { opacity: 0.6; cursor: not-allowed; }
  .action-btn.danger {
    background: linear-gradient(135deg, #991b1b, #7f1d1d);
    box-shadow: 0 2px 8px rgba(153,27,27,0.3);
  }
  .action-btn.danger:hover:not(:disabled) {
    box-shadow: 0 4px 12px rgba(239,68,68,0.4);
  }
</style>
