<script>
  import { apiGet, apiUpload } from '../../lib/api.js';
  import { setStateKey } from '../../stores/state.js';
  import { onMount } from 'svelte';

  export let config;

  let progress = 0;
  let uploading = false;
  let status = '';
  let fileInput;

  onMount(async () => {
    try {
      const d = await apiGet('/api/ota');
      setStateKey('_ota.version', d.version || '');
      setStateKey('_ota.partition', d.partition || '');
      setStateKey('_ota.idf', d.idf || '');
      setStateKey('_ota.date', `${d.date || ''} ${d.time || ''}`);
    } catch (e) {}
  });

  async function onFile(e) {
    const file = e.target.files[0];
    if (!file) return;
    if (!file.name.endsWith('.bin')) { alert('Only .bin files'); return; }
    if (!confirm('Оновити прошивку? Пристрій перезавантажиться.')) {
      fileInput.value = '';
      return;
    }

    uploading = true;
    status = 'Uploading...';
    progress = 0;

    try {
      await apiUpload('/api/ota', file, (pct, bytes) => {
        progress = pct;
        status = `${pct}% (${Math.round(bytes / 1024)} KB)`;
      });
      status = 'Done! Restarting...';
      progress = 100;
      setTimeout(() => location.reload(), 5000);
    } catch (e) {
      status = 'Error: ' + e.message;
    }
    fileInput.value = '';
  }
</script>

<div class="upload-widget">
  <input
    type="file"
    accept=".bin"
    bind:this={fileInput}
    on:change={onFile}
    style="display:none"
  />
  <button
    class="action-btn"
    disabled={uploading}
    on:click={() => fileInput.click()}
  >
    {config.label || 'Upload firmware'}
  </button>
  {#if uploading}
    <div class="progress-area">
      <div class="progress-bar">
        <div class="progress-fill" style="width: {progress}%"></div>
      </div>
      <span class="progress-status">{status}</span>
    </div>
  {/if}
</div>

<style>
  .upload-widget { padding: 4px 0; }
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
  .progress-area { margin-top: 12px; }
  .progress-bar {
    height: 8px; background: var(--border);
    border-radius: 4px; overflow: hidden;
  }
  .progress-fill {
    height: 100%; background: var(--accent);
    transition: width 0.3s; border-radius: 4px;
  }
  .progress-status {
    font-size: 12px; color: var(--fg-muted);
    margin-top: 6px; display: block;
  }
</style>
