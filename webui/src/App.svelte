<script>
  import { onMount } from 'svelte';
  import { loadUiConfig, uiLoading, uiError, deviceName, pages } from './stores/ui.js';
  import { initWebSocket, state } from './stores/state.js';
  import { apiGet } from './lib/api.js';
  import Layout from './components/Layout.svelte';
  import Dashboard from './pages/Dashboard.svelte';
  import DynamicPage from './pages/DynamicPage.svelte';
  import BindingsEditor from './pages/BindingsEditor.svelte';

  let currentPage = 'dashboard';

  onMount(async () => {
    await loadUiConfig();
    // Load initial state
    try {
      const s = await apiGet('/api/state');
      state.update(st => ({ ...st, ...s }));
    } catch (e) {
      console.warn('Initial state load failed', e);
    }
    // Start WebSocket
    initWebSocket();
  });

  $: document.title = $deviceName;
</script>

{#if $uiLoading}
  <div class="loading-screen">
    <div class="loading-spinner"></div>
    <div class="loading-text">Loading...</div>
  </div>
{:else if $uiError}
  <div class="error-screen">
    <div class="error-icon">!</div>
    <div class="error-title">Connection Error</div>
    <div class="error-msg">{$uiError}</div>
    <button class="retry-btn" on:click={() => location.reload()}>Retry</button>
  </div>
{:else}
  <Layout bind:currentPage>
    {#if currentPage === 'dashboard'}
      <Dashboard />
    {:else if currentPage === 'bindings'}
      <BindingsEditor />
    {:else}
      <DynamicPage pageId={currentPage} />
    {/if}
  </Layout>
{/if}

<style>
  :global(:root) {
    /* Base palette */
    --bg: #0f172a;
    --bg2: #1e293b;
    --card: #1f2a3d;
    --card-hover: #253347;
    --border: #334155;
    --border-light: #3d4f67;
    --bg-hover: rgba(59, 130, 246, 0.1);

    /* Text */
    --fg: #f1f5f9;
    --fg-muted: #94a3b8;
    --fg-dim: #64748b;

    /* Accent */
    --accent: #3b82f6;
    --accent-light: #60a5fa;
    --accent-bg: rgba(59, 130, 246, 0.12);

    /* Semantic */
    --success: #22c55e;
    --warning: #f59e0b;
    --error: #ef4444;

    /* Type scale */
    --text-xs: 11px;
    --text-sm: 13px;
    --text-base: 14px;
    --text-lg: 16px;
    --text-xl: 18px;
    --text-2xl: 24px;

    /* Spacing */
    --space-1: 4px;
    --space-2: 8px;
    --space-3: 12px;
    --space-4: 16px;
    --space-5: 20px;
    --space-6: 24px;

    /* Widget */
    --widget-min-h: 44px;
    --widget-pad: 6px 0;
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
