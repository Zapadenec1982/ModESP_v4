<script>
  import { onDestroy } from 'svelte';
  import { fade, fly } from 'svelte/transition';
  import { pages, deviceName } from '../stores/ui.js';
  import { wsConnected, state } from '../stores/state.js';
  import { theme, toggleTheme } from '../stores/theme.js';
  import { t, language, toggleLanguage } from '../stores/i18n.js';
  import { toastSuccess } from '../stores/toast.js';
  import Icon from './Icon.svelte';
  import Clock from './Clock.svelte';

  export let currentPage = 'dashboard';

  // Alarm banner on all pages (AUDIT-009)
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];

  // Mobile sidebar state
  let sidebarOpen = false;

  function navigate(id) {
    currentPage = id;
    sidebarOpen = false;
  }

  $: currentTitle = $pages.find(p => p.id === currentPage)?.title || '';
  $: sortedPages = [...$pages].sort((a, b) => (a.order || 0) - (b.order || 0));

  // Connection overlay — show after 5s disconnect (avoid flicker)
  let showOverlay = false;
  let overlayTimer = null;

  const unsub = wsConnected.subscribe(connected => {
    if (!connected) {
      if (!overlayTimer) {
        overlayTimer = setTimeout(() => { showOverlay = true; }, 5000);
      }
    } else {
      if (overlayTimer) { clearTimeout(overlayTimer); overlayTimer = null; }
      if (showOverlay) toastSuccess($t['conn.restored']);
      showOverlay = false;
    }
  });

  onDestroy(() => {
    unsub();
    if (overlayTimer) clearTimeout(overlayTimer);
  });
</script>

<div class="layout">
  <!-- Sidebar -->
  <aside class="sidebar" class:open={sidebarOpen}>
    <div class="sidebar-header">
      <div class="logo-mark">
        <Icon name="snowflake" size={20} />
      </div>
      <span class="logo-text">{$deviceName}</span>
    </div>
    <nav class="sidebar-nav">
      {#each sortedPages as page}
        <button
          class="nav-item"
          class:active={currentPage === page.id}
          on:click={() => navigate(page.id)}
        >
          <Icon name={page.icon || 'home'} size={18} />
          <span class="nav-label">{page.title}</span>
        </button>
      {/each}
    </nav>
    <div class="sidebar-footer">
      <div class="ws-status" class:connected={$wsConnected}>
        <span class="ws-dot"></span>
        <span>{$wsConnected ? $t['status.online'] : $t['status.offline']} · v4.0</span>
      </div>
    </div>
  </aside>

  <!-- Mobile overlay -->
  {#if sidebarOpen}
    <!-- svelte-ignore a11y-click-events-have-key-events -->
    <div class="sidebar-overlay" on:click={() => sidebarOpen = false}
         transition:fade={{ duration: 200 }}></div>
  {/if}

  <!-- Main content -->
  <div class="main-area">
    <!-- Alarm strip -->
    {#if alarmActive}
      <button class="alarm-strip" on:click={() => navigate('protection')}>
        <span class="alarm-blink">⚠</span>
        <span>{$t['alarm.banner']}: {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : ''}</span>
      </button>
    {/if}

    <header class="topbar">
      <div class="topbar-left">
        <button class="hamburger" on:click={() => sidebarOpen = !sidebarOpen}>
          <Icon name="menu" size={22} />
        </button>
        <h1 class="topbar-title">{currentTitle}</h1>
      </div>
      <div class="topbar-right">
        <Clock />
        <button class="topbar-btn lang-btn" on:click={toggleLanguage} title="Language">
          {$language === 'uk' ? 'UA' : 'EN'}
        </button>
        <!-- Theme toggle -->
        <button class="theme-toggle" on:click={toggleTheme}
                title={$theme === 'dark' ? 'Light mode' : 'Dark mode'}>
          <span class="toggle-track">
            <span class="toggle-knob" class:light={$theme === 'light'}>
              <Icon name={$theme === 'dark' ? 'moon' : 'sun'} size={12} />
            </span>
          </span>
        </button>
        <div class="ws-indicator" class:connected={$wsConnected}>
          <span class="ws-dot"></span>
        </div>
      </div>
    </header>

    <main class="content">
      <slot />
    </main>

    <!-- Connection overlay -->
    {#if showOverlay}
      <div class="conn-overlay" transition:fade={{ duration: 200 }}>
        <div class="conn-dialog">
          <div class="conn-spinner"></div>
          <div class="conn-text">{$t['conn.lost']}</div>
          <button class="conn-retry" on:click={() => location.reload()}>
            {$t['conn.retry']}
          </button>
        </div>
      </div>
    {/if}
  </div>
</div>

<style>
  .layout {
    display: flex;
    min-height: 100vh;
    position: relative;
    z-index: 1;
  }

  /* ── Sidebar ─────────────────────────────────────── */
  .sidebar {
    width: var(--sidebar-w);
    background: var(--surface);
    border-right: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    position: fixed;
    top: 0;
    left: 0;
    bottom: 0;
    z-index: 30;
    transition: transform 0.35s cubic-bezier(0.16, 1, 0.3, 1),
                background 0.4s ease, border-color 0.4s ease;
  }

  .sidebar-header {
    padding: 20px 16px;
    display: flex;
    align-items: center;
    gap: 12px;
    border-bottom: 1px solid var(--border);
  }

  .logo-mark {
    width: 36px;
    height: 36px;
    border-radius: 9px;
    background: linear-gradient(135deg, var(--frost-dim) 0%, rgba(86, 212, 232, 0.12) 100%);
    border: 1px solid var(--frost-border);
    box-shadow: 0 0 16px var(--frost-glow), inset 0 1px 0 rgba(255, 255, 255, 0.04);
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--frost);
    flex-shrink: 0;
  }

  .logo-text {
    font-size: 16px;
    font-weight: 700;
    background: linear-gradient(135deg, var(--frost) 0%, var(--text-1) 40%, var(--text-2) 100%);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
    letter-spacing: 0.5px;
  }

  .sidebar-nav {
    flex: 1;
    padding: 12px 8px;
    display: flex;
    flex-direction: column;
    gap: 2px;
    overflow-y: auto;
  }

  .nav-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px 12px;
    border: 1px solid transparent;
    background: none;
    color: var(--text-3);
    font-size: 13.5px;
    font-weight: 450;
    font-family: var(--font-display);
    border-radius: var(--radius-xs);
    cursor: pointer;
    transition: all 0.15s ease;
    text-align: left;
    width: 100%;
    position: relative;
  }

  .nav-item:hover {
    background: var(--surface-hover);
    color: var(--text-1);
  }

  .nav-item.active {
    background: var(--frost-dim);
    border-color: var(--frost-border);
    color: var(--frost);
  }

  /* Frost accent bar for active item */
  .nav-item.active::before {
    content: '';
    position: absolute;
    left: 0;
    top: 8px;
    bottom: 8px;
    width: 3px;
    border-radius: 0 3px 3px 0;
    background: var(--frost);
    box-shadow: 0 0 8px var(--frost-glow);
  }

  .nav-label {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .sidebar-footer {
    padding: 14px 16px;
    border-top: 1px solid var(--border);
  }

  .ws-status {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 11px;
    color: var(--text-3);
    font-weight: 450;
  }

  .ws-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--danger);
    display: inline-block;
    flex-shrink: 0;
  }

  .ws-status.connected .ws-dot {
    background: var(--ok);
    box-shadow: 0 0 8px var(--ok-glow);
  }

  /* ── Mobile sidebar overlay ──────────────────────── */
  .sidebar-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.6);
    z-index: 25;
  }

  /* ── Alarm strip ─────────────────────────────────── */
  .alarm-strip {
    background: linear-gradient(90deg, var(--alarm-dark) 0%, var(--alarm-text) 50%, var(--alarm-dark) 100%);
    background-size: 200% 100%;
    animation: alarmSlide 3s ease-in-out infinite;
    color: #fff;
    text-align: center;
    padding: 8px 16px;
    font-family: var(--font-mono);
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 1.5px;
    text-transform: uppercase;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    height: 36px;
  }

  .alarm-blink {
    animation: alarmBlink 1s step-end infinite;
  }

  @keyframes alarmSlide {
    0%, 100% { background-position: 0% 50%; }
    50% { background-position: 100% 50%; }
  }

  @keyframes alarmBlink {
    0%, 50% { opacity: 1; }
    51%, 100% { opacity: 0; }
  }

  /* ── Topbar (glass header) ───────────────────────── */
  .topbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 24px;
    height: var(--header-h);
    background: var(--glass-heavy);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 10;
    transition: background 0.4s ease, border-color 0.4s ease;
  }

  .topbar-left {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .hamburger {
    display: none;
    background: none;
    border: none;
    color: var(--text-2);
    cursor: pointer;
    width: 40px;
    height: 40px;
    border-radius: var(--radius-xs);
    align-items: center;
    justify-content: center;
    transition: background 0.15s ease;
  }

  .hamburger:hover {
    background: var(--surface-hover);
  }

  .topbar-title {
    font-size: 16px;
    font-weight: 600;
    color: var(--text-1);
  }

  .topbar-right {
    display: flex;
    align-items: center;
    gap: 10px;
  }

  .topbar-btn {
    background: none;
    border: 1px solid var(--border);
    color: var(--text-3);
    width: 32px;
    height: 32px;
    border-radius: var(--radius-xs);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 11px;
    font-weight: 600;
    font-family: var(--font-display);
    transition: all 0.15s ease;
  }

  .topbar-btn:hover {
    background: var(--surface-hover);
    color: var(--text-1);
    border-color: var(--border-accent);
  }

  /* ── Theme toggle ────────────────────────────────── */
  .theme-toggle {
    background: none;
    border: none;
    cursor: pointer;
    padding: 0;
    display: flex;
    align-items: center;
  }

  .toggle-track {
    width: 52px;
    height: 28px;
    border-radius: 14px;
    background: var(--surface-3);
    border: 1px solid var(--border);
    position: relative;
    display: flex;
    align-items: center;
    transition: background 0.4s ease, border-color 0.4s ease;
  }

  .toggle-knob {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: var(--accent);
    display: flex;
    align-items: center;
    justify-content: center;
    color: #fff;
    position: absolute;
    left: 3px;
    transition: transform 0.35s cubic-bezier(0.16, 1, 0.3, 1),
                background 0.4s ease;
    box-shadow: 0 1px 4px rgba(0, 0, 0, 0.3);
  }

  .toggle-knob.light {
    transform: translateX(24px);
  }

  /* ── WS indicator (mobile) ───────────────────────── */
  .ws-indicator {
    display: none;
  }

  .ws-indicator .ws-dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--danger);
    display: block;
  }

  .ws-indicator.connected .ws-dot {
    background: var(--ok);
    box-shadow: 0 0 8px var(--ok-glow);
  }

  /* ── Main area ───────────────────────────────────── */
  .main-area {
    flex: 1;
    margin-left: var(--sidebar-w);
    display: flex;
    flex-direction: column;
    min-height: 100vh;
    transition: margin-left 0.35s cubic-bezier(0.16, 1, 0.3, 1);
  }

  .content {
    flex: 1;
    padding: 20px 24px;
    max-width: var(--content-max);
    width: 100%;
    margin: 0 auto;
  }

  /* ── Connection overlay ──────────────────────────── */
  .conn-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 9000;
  }

  .conn-dialog {
    background: var(--glass-heavy);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    border-radius: var(--radius);
    border: 1px solid var(--border-glow);
    padding: 32px 24px;
    text-align: center;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
    max-width: 320px;
    width: 90%;
  }

  .conn-spinner {
    width: 32px;
    height: 32px;
    border: 3px solid var(--border);
    border-top-color: var(--accent);
    border-radius: 50%;
    animation: connSpin 0.8s linear infinite;
    margin: 0 auto 16px;
  }

  @keyframes connSpin {
    to { transform: rotate(360deg); }
  }

  .conn-text {
    font-size: 14px;
    color: var(--text-2);
    margin-bottom: 16px;
  }

  .conn-retry {
    background: var(--accent);
    color: #fff;
    border: none;
    border-radius: var(--radius-xs);
    padding: 10px 24px;
    font-size: 14px;
    font-weight: 600;
    font-family: var(--font-display);
    cursor: pointer;
    min-height: var(--touch-min);
    transition: opacity 0.15s ease;
  }

  .conn-retry:hover {
    opacity: 0.9;
  }

  /* ── Responsive ──────────────────────────────────── */
  @media (max-width: 1024px) {
    .sidebar {
      transform: translateX(-100%);
    }

    .sidebar.open {
      transform: translateX(0);
    }

    .main-area {
      margin-left: 0;
    }

    .hamburger {
      display: flex;
    }

    .ws-indicator {
      display: block;
    }

    .content {
      padding: 16px;
    }

    .topbar {
      padding: 0 16px;
    }

    .topbar-title {
      font-size: 15px;
    }
  }

  @media (min-width: 1025px) {
    .sidebar-overlay {
      display: none;
    }
  }
</style>
