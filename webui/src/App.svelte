<script>
  import { onMount } from 'svelte';
  import { fade, fly, scale } from 'svelte/transition';
  import { loadUiConfig, uiLoading, uiError, deviceName, pages } from './stores/ui.js';
  import { initWebSocket, state } from './stores/state.js';
  import { apiGet } from './lib/api.js';
  import { t } from './stores/i18n.js';
  import './stores/theme.js';
  import Layout from './components/Layout.svelte';
  import Toast from './components/Toast.svelte';
  import Dashboard from './pages/Dashboard.svelte';
  import DynamicPage from './pages/DynamicPage.svelte';
  import BindingsEditor from './pages/BindingsEditor.svelte';

  let currentPage = 'dashboard';

  onMount(async () => {
    await loadUiConfig();
    try {
      const s = await apiGet('/api/state');
      state.update(st => ({ ...st, ...s }));
    } catch (e) {
      console.warn('Initial state load failed', e);
    }
    initWebSocket();
  });

  $: document.title = $deviceName;
</script>

{#if $uiLoading}
  <div class="loading-screen" in:fade={{ duration: 200 }}>
    <div class="loading-spinner"></div>
    <div class="loading-text">{$t['app.loading']}</div>
  </div>
{:else if $uiError}
  <div class="error-screen" in:scale={{ start: 0.95, duration: 200 }}>
    <div class="error-icon">!</div>
    <div class="error-title">{$t['app.error']}</div>
    <div class="error-msg">{$uiError}</div>
    <button class="retry-btn" on:click={() => location.reload()}>{$t['app.retry']}</button>
  </div>
{:else}
  <Layout bind:currentPage>
    {#key currentPage}
      <div in:fly={{ x: 20, duration: 200, delay: 50 }} out:fade={{ duration: 100 }}>
        {#if currentPage === 'dashboard'}
          <Dashboard />
        {:else if currentPage === 'bindings'}
          <BindingsEditor />
        {:else}
          <DynamicPage pageId={currentPage} />
        {/if}
      </div>
    {/key}
  </Layout>
{/if}

<Toast />

<style>
  :global(:root) {
    --bg: #0f172a;
    --bg2: #1e293b;
    --card: #1e293b;
    --border: #334155;
    --bg-hover: rgba(59, 130, 246, 0.1);
    --fg: #f1f5f9;
    --fg-muted: #94a3b8;
    --accent: #3b82f6;
    --accent-bg: rgba(59, 130, 246, 0.15);
    --success: #22c55e;
    --warning: #f59e0b;
    --error: #ef4444;
    --color-scheme: dark;
  }

  :global(:root[data-theme="light"]) {
    --bg: #f8fafc;
    --bg2: #ffffff;
    --card: #ffffff;
    --border: #e2e8f0;
    --bg-hover: rgba(59, 130, 246, 0.08);
    --fg: #0f172a;
    --fg-muted: #64748b;
    --accent: #2563eb;
    --accent-bg: rgba(37, 99, 235, 0.1);
    --success: #16a34a;
    --warning: #d97706;
    --error: #dc2626;
    --color-scheme: light;
  }

  :global(*) {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
  }

  :global(body) {
    background: var(--bg);
    color: var(--fg);
    font-family: system-ui, -apple-system, 'Segoe UI', sans-serif;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
    transition: background-color 0.3s, color 0.3s;
  }

  .loading-screen {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    gap: 16px;
  }

  .loading-spinner {
    width: 32px;
    height: 32px;
    border: 3px solid var(--border);
    border-top-color: var(--accent);
    border-radius: 50%;
    animation: spin 0.8s linear infinite;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  .loading-text {
    font-size: 14px;
    color: var(--fg-muted);
  }

  .error-screen {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    gap: 12px;
  }

  .error-icon {
    width: 48px;
    height: 48px;
    border-radius: 50%;
    background: var(--error);
    color: white;
    font-size: 24px;
    font-weight: 700;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .error-title {
    font-size: 18px;
    font-weight: 600;
  }

  .error-msg {
    font-size: 14px;
    color: var(--fg-muted);
  }

  .retry-btn {
    margin-top: 8px;
    padding: 10px 24px;
    border-radius: 8px;
    border: 1px solid var(--accent);
    background: transparent;
    color: var(--accent);
    font-size: 14px;
    cursor: pointer;
  }

  .retry-btn:hover {
    background: var(--accent-bg);
  }
</style>
