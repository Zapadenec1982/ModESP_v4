<script>
  import { onMount } from 'svelte';
  import { fade, fly, scale } from 'svelte/transition';
  import { loadUiConfig, uiLoading, uiError, deviceName, pages, navigateTo } from './stores/ui.js';
  import { initWebSocket, state } from './stores/state.js';
  import { apiGet, apiPost, needsLogin } from './lib/api.js';
  import { t } from './stores/i18n.js';
  import './stores/theme.js';
  import Layout from './components/Layout.svelte';
  import Toast from './components/Toast.svelte';
  import LoginModal from './components/LoginModal.svelte';
  import Dashboard from './pages/Dashboard.svelte';
  import DynamicPage from './pages/DynamicPage.svelte';
  import BindingsEditor from './pages/BindingsEditor.svelte';
  import ProtectionPage from './pages/ProtectionPage.svelte';

  let currentPage = 'dashboard';

  // Ambient light state class
  $: alarmActive = $state['protection.alarm_active'];
  $: ambientClass = alarmActive ? 'state-alarm' : 'state-ok';

  onMount(async () => {
    await loadUiConfig();
    try {
      const s = await apiGet('/api/state');
      state.update(st => ({ ...st, ...s }));
    } catch (e) {
      console.warn('Initial state load failed', e);
    }
    // Auth probe: POST з пустим body → 401 якщо auth потрібен
    try { await apiPost('/api/settings', {}); } catch (e) { /* 401 → needsLogin */ }
    initWebSocket();
  });

  $: document.title = $deviceName;
  $: if ($navigateTo) { currentPage = $navigateTo; $navigateTo = null; }
</script>

<!-- Mesh gradient background — subtle frost+accent depth -->
<div class="mesh-bg"></div>

<!-- Ambient light — state-dependent glow -->
<div class="ambient {ambientClass}"></div>

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
        {:else if currentPage === 'protection'}
          <ProtectionPage />
        {:else if currentPage === 'bindings'}
          <BindingsEditor />
        {:else}
          <DynamicPage pageId={currentPage} />
        {/if}
      </div>
    {/key}
  </Layout>
{/if}

{#if $needsLogin}
  <LoginModal />
{/if}

<Toast />

<style>
  /* ── Light theme overrides ─────────────────────── */
  :global(:root[data-theme="light"]) {
    --bg: #edf2f9;
    --bg-warm: #e4ebf5;
    --surface: #f8fafe;
    --surface-2: #f0f4fb;
    --surface-3: #e6ecf5;
    --surface-hover: #dfe7f3;
    --glass: rgba(248, 250, 254, 0.9);
    --glass-heavy: rgba(237, 242, 249, 0.95);
    --border: rgba(50, 80, 140, 0.09);
    --border-subtle: rgba(50, 80, 140, 0.05);
    --border-accent: rgba(50, 80, 140, 0.11);
    --border-glow: rgba(58, 127, 224, 0.10);
    --text-1: #1a2438;
    --text-2: #4a5570;
    --text-3: #7a8599;
    --text-4: #a8b2c2;
    --accent: #3a7fe0;
    --accent-bright: #2968c4;
    --accent-dim: rgba(58, 127, 224, 0.06);
    --accent-glow: rgba(58, 127, 224, 0.08);
    --accent-glow-s: rgba(58, 127, 224, 0.15);
    --ok: #10b981;
    --ok-dim: rgba(16, 185, 129, 0.06);
    --ok-glow: rgba(16, 185, 129, 0.10);
    --ok-border: rgba(16, 185, 129, 0.14);
    --danger: #dc2626;
    --danger-dim: rgba(220, 38, 38, 0.05);
    --danger-glow: rgba(220, 38, 38, 0.08);
    --danger-border: rgba(220, 38, 38, 0.16);
    --warn: #b45309;
    --warn-dim: rgba(180, 83, 9, 0.05);
    --warn-glow: rgba(180, 83, 9, 0.08);
    --info: #0891b2;
    --info-dim: rgba(8, 145, 178, 0.05);
    --info-glow: rgba(8, 145, 178, 0.08);
    --frost: #0aa4c2;
    --frost-dim: rgba(10, 164, 194, 0.06);
    --frost-glow: rgba(10, 164, 194, 0.10);
    --frost-border: rgba(10, 164, 194, 0.12);
    --fg: var(--text-1);
    --fg-muted: var(--text-2);
    --card: var(--surface);
    --bg2: var(--surface);
    --bg-hover: var(--surface-hover);
    --accent-bg: var(--accent-dim);
    --success: var(--ok);
    --error: var(--danger);
    --warning: var(--warn);
    --color-scheme: light;
  }

  /* ── Global reset ──────────────────────────────── */
  :global(*) {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
    -webkit-tap-highlight-color: transparent;
  }

  :global(body) {
    background: var(--bg);
    color: var(--text-1);
    font-family: var(--font-display);
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
    overflow-x: hidden;
    transition: background 0.4s ease, color 0.4s ease;
  }

  /* ── Noise texture overlay ─────────────────────── */
  :global(body::before) {
    content: '';
    position: fixed;
    inset: 0;
    background-image: url("data:image/svg+xml,%3Csvg viewBox='0 0 256 256' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.015'/%3E%3C/svg%3E");
    pointer-events: none;
    z-index: 9999;
    opacity: 0.5;
  }

  :global(:root[data-theme="light"] body::before) {
    opacity: 0.12;
  }

  /* ── Mesh gradient background ──────────────────── */
  .mesh-bg {
    position: fixed;
    inset: 0;
    z-index: 0;
    pointer-events: none;
    background:
      radial-gradient(ellipse 70% 50% at 8% 3%, rgba(86, 212, 232, 0.04) 0%, transparent 55%),
      radial-gradient(ellipse 60% 60% at 88% 85%, rgba(91, 156, 245, 0.03) 0%, transparent 50%);
    transition: background 0.4s ease;
  }

  :global(:root[data-theme="light"]) .mesh-bg {
    background:
      radial-gradient(ellipse 70% 50% at 8% 3%, rgba(86, 212, 232, 0.09) 0%, transparent 55%),
      radial-gradient(ellipse 60% 60% at 88% 85%, rgba(58, 127, 224, 0.06) 0%, transparent 50%);
  }

  /* ── Ambient light ─────────────────────────────── */
  .ambient {
    position: fixed;
    top: -200px;
    left: 50%;
    transform: translateX(-50%);
    width: 800px;
    height: 600px;
    border-radius: 50%;
    pointer-events: none;
    z-index: 0;
    transition: all 1.2s cubic-bezier(0.16, 1, 0.3, 1);
    filter: blur(120px);
    opacity: 0.3;
    background: radial-gradient(circle, var(--accent-glow) 0%, transparent 70%);
  }

  .ambient.state-alarm {
    background: radial-gradient(circle, var(--danger-glow) 0%, transparent 70%);
    opacity: 0.5;
    animation: ambientPulse 4s ease-in-out infinite;
  }

  .ambient.state-ok {
    background: radial-gradient(circle, var(--ok-glow) 0%, transparent 70%);
    opacity: 0.25;
  }

  @keyframes ambientPulse {
    0%, 100% { opacity: 0.35; }
    50% { opacity: 0.6; }
  }

  /* ── Smooth theme transitions for all components ─ */
  :global(body), :global(.sidebar), :global(.card), :global(.group),
  :global(.hero-temp), :global(.metric-card), :global(.eq-cell),
  :global(.stepper), :global(.st-val), :global(.sel), :global(.badge),
  :global(.sum-chip) {
    transition: background 0.4s ease, color 0.4s ease, border-color 0.4s ease, box-shadow 0.4s ease;
  }

  /* ── Loading screen ────────────────────────────── */
  .loading-screen {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    gap: 16px;
    position: relative;
    z-index: 1;
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
    color: var(--text-2);
  }

  /* ── Error screen ──────────────────────────────── */
  .error-screen {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
    gap: 12px;
    position: relative;
    z-index: 1;
  }

  .error-icon {
    width: 48px;
    height: 48px;
    border-radius: 50%;
    background: var(--danger);
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
    color: var(--text-2);
  }

  .retry-btn {
    margin-top: 8px;
    padding: 10px 24px;
    border-radius: var(--radius-xs);
    border: 1px solid var(--accent);
    background: transparent;
    color: var(--accent);
    font-size: 14px;
    font-family: var(--font-display);
    cursor: pointer;
    transition: background var(--transition-fast);
  }

  .retry-btn:hover {
    background: var(--accent-dim);
  }
</style>
