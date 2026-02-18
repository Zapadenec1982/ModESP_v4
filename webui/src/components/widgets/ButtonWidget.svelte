<script>
  import { apiPost } from '../../lib/api.js';

  export let config;

  let loading = false;

  async function onClick() {
    if (config.confirm && !confirm(config.confirm)) return;
    if (!config.api_endpoint) return;
    loading = true;
    try {
      await apiPost(config.api_endpoint, {});
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
  .btn-widget { padding: 4px 0; }
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
