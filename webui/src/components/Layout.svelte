<script>
  import { onDestroy } from 'svelte';
  import { fade } from 'svelte/transition';
  import { pages, deviceName } from '../stores/ui.js';
  import { wsConnected, state } from '../stores/state.js';
  import { theme, toggleTheme } from '../stores/theme.js';
  import { t, language, toggleLanguage } from '../stores/i18n.js';
  import { toastSuccess } from '../stores/toast.js';
  import Icon from './Icon.svelte';
  import Clock from './Clock.svelte';

  export let currentPage = 'dashboard';

  // AUDIT-009: alarm banner на всіх сторінках
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];

  function navigate(id) {
    currentPage = id;
  }

  $: currentTitle = $pages.find(p => p.id === currentPage)?.title || '';
  $: sortedPages = [...$pages].sort((a, b) => (a.order || 0) - (b.order || 0));

  // Mobile bottom tabs: max 4 visible + "More" if > 5 pages
  const MAX_TABS = 4;
  $: hasMore = sortedPages.length > MAX_TABS + 1;
  $: visibleTabs = hasMore ? sortedPages.slice(0, MAX_TABS) : sortedPages;
  $: moreTabs = hasMore ? sortedPages.slice(MAX_TABS) : [];

  // "More" overlay state
  let moreOpen = false;

  function navigateMore(id) {
    currentPage = id;
    moreOpen = false;
  }

  // Connection overlay — показувати через 5с після disconnect (уникнути flicker)
  let showOverlay = false;
  let overlayTimer = null;
  let wasDisconnected = false;

  const unsub = wsConnected.subscribe(connected => {
    if (!connected) {
      if (!overlayTimer) {
        wasDisconnected = true;
        overlayTimer = setTimeout(() => { showOverlay = true; }, 5000);
      }
    } else {
      if (overlayTimer) { clearTimeout(overlayTimer); overlayTimer = null; }
      // WS вже надсилає full state при підключенні — HTTP запит не потрібен
      // (apiGet('/api/state') конкурував з WS за httpd сокети → каскадний reconnect)
      if (showOverlay) toastSuccess($t['conn.restored']);
      showOverlay = false;
      wasDisconnected = false;
    }
  });

  onDestroy(() => {
    unsub();
    if (overlayTimer) clearTimeout(overlayTimer);
  });
</script>

<div class="layout">
  <!-- Sidebar (desktop) -->
  <aside class="sidebar">
    <div class="sidebar-header">
      <span class="logo">❄</span>
      <span class="logo-text">{$deviceName}</span>
    </div>
    <nav class="sidebar-nav">
      {#each sortedPages as page}
        <button
          class="nav-item"
          class:active={currentPage === page.id}
          on:click={() => navigate(page.id)}
        >
          <Icon name={page.icon || 'home'} size={20} />
          <span class="nav-label">{page.title}</span>
        </button>
      {/each}
    </nav>
    <div class="sidebar-footer">
      <div class="ws-status" class:connected={$wsConnected}>
        <span class="ws-dot"></span>
        {$wsConnected ? $t['status.online'] : $t['status.offline']}
      </div>
    </div>
  </aside>

  <!-- Main content -->
  <div class="main-area">
    <!-- AUDIT-009: alarm banner на всіх сторінках -->
    {#if alarmActive}
      <div class="alarm-banner" on:click={() => navigate('thermostat')}>
        {$t['alarm.banner']}: {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : ''}
      </div>
    {/if}
    <header class="topbar">
      <h1 class="topbar-title">{currentTitle}</h1>
      <div class="topbar-right">
        <Clock />
        <button class="topbar-btn" on:click={toggleLanguage} title="Language">
          {$language === 'uk' ? 'UA' : 'EN'}
        </button>
        <button class="topbar-btn" on:click={toggleTheme} title="Theme">
          <Icon name={$theme === 'dark' ? 'sun' : 'moon'} size={18} />
        </button>
        <div class="ws-badge" class:connected={$wsConnected}>
          <span class="ws-dot"></span>
        </div>
      </div>
    </header>
    <main class="content">
      <slot />
    </main>

    <!-- Connection overlay (після 5с disconnect) -->
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

  <!-- Bottom tabs (mobile) -->
  <nav class="bottom-tabs">
    {#each visibleTabs as page}
      <button
        class="tab-item"
        class:active={currentPage === page.id}
        on:click={() => navigate(page.id)}
      >
        <Icon name={page.icon || 'home'} size={20} />
        <span class="tab-label">{page.title}</span>
      </button>
    {/each}
    {#if hasMore}
      <button
        class="tab-item"
        class:active={moreTabs.some(p => p.id === currentPage)}
        on:click={() => moreOpen = !moreOpen}
      >
        <Icon name="more-horizontal" size={20} />
        <span class="tab-label">{$t['nav.more']}</span>
      </button>
    {/if}
  </nav>

  <!-- "More" overlay -->
  {#if moreOpen}
    <div class="more-overlay" transition:fade={{ duration: 150 }}>
      <div class="more-sheet">
        <div class="more-header">
          <span class="more-title">{$t['nav.more']}</span>
          <button class="more-close" on:click={() => moreOpen = false}>
            <Icon name="x" size={20} />
          </button>
        </div>
        {#each moreTabs as page}
          <button
            class="more-item"
            class:active={currentPage === page.id}
            on:click={() => navigateMore(page.id)}
          >
            <Icon name={page.icon || 'home'} size={22} />
            <span>{page.title}</span>
          </button>
        {/each}
      </div>
    </div>
  {/if}
</div>

<style>
  .layout {
    display: flex;
    min-height: 100vh;
    background: var(--bg);
    color: var(--fg);
  }

  /* === Sidebar (desktop) === */
  .sidebar {
    width: 220px;
    background: var(--bg2);
    border-right: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    position: fixed;
    top: 0;
    left: 0;
    bottom: 0;
    z-index: 20;
  }

  .sidebar-header {
    padding: 20px 16px;
    display: flex;
    align-items: center;
    gap: 10px;
    border-bottom: 1px solid var(--border);
  }

  .logo {
    font-size: 24px;
  }

  .logo-text {
    font-size: 16px;
    font-weight: 700;
    color: var(--fg);
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
    border: none;
    background: none;
    color: var(--fg-muted);
    font-size: 14px;
    font-weight: 500;
    border-radius: 8px;
    cursor: pointer;
    transition: all 0.15s;
    text-align: left;
    width: 100%;
  }

  .nav-item:hover {
    background: var(--bg-hover);
    color: var(--fg);
  }

  .nav-item.active {
    background: var(--accent-bg);
    color: var(--accent);
  }

  .nav-label {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .sidebar-footer {
    padding: 12px 16px;
    border-top: 1px solid var(--border);
  }

  .ws-status {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 12px;
    color: var(--fg-muted);
  }

  .ws-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--error);
    display: inline-block;
  }

  .ws-status.connected .ws-dot,
  .ws-badge.connected .ws-dot {
    background: var(--success);
    box-shadow: 0 0 6px var(--success);
  }

  /* === Alarm banner (AUDIT-009) === */
  .alarm-banner {
    background: var(--alarm-dark);
    color: #fff;
    text-align: center;
    padding: var(--sp-2) var(--sp-4);
    font-size: var(--text-base);
    font-weight: var(--fw-bold);
    letter-spacing: 1px;
    cursor: pointer;
    animation: alarm-flash 1.5s infinite;
  }

  @keyframes alarm-flash {
    0%, 100% { background: var(--alarm-dark); }
    50% { background: var(--alarm-text); }
  }

  /* === Connection overlay === */
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
    background: var(--card);
    border-radius: var(--radius-2xl);
    padding: var(--sp-8) var(--sp-6);
    text-align: center;
    box-shadow: var(--shadow-lg);
    max-width: 320px;
    width: 90%;
  }

  .conn-spinner {
    width: 32px;
    height: 32px;
    border: 3px solid var(--border);
    border-top-color: var(--accent);
    border-radius: var(--radius-full);
    animation: conn-spin 0.8s linear infinite;
    margin: 0 auto var(--sp-4);
  }

  @keyframes conn-spin {
    to { transform: rotate(360deg); }
  }

  .conn-text {
    font-size: var(--text-md);
    color: var(--fg-muted);
    margin-bottom: var(--sp-4);
  }

  .conn-retry {
    background: var(--accent);
    color: #fff;
    border: none;
    border-radius: var(--radius-lg);
    padding: var(--sp-2-5) var(--sp-6);
    font-size: var(--text-md);
    font-weight: var(--fw-semibold);
    cursor: pointer;
    min-height: var(--touch-min);
    transition: opacity var(--transition-fast);
  }

  .conn-retry:hover {
    opacity: 0.9;
  }

  /* === Main area === */
  .main-area {
    flex: 1;
    margin-left: 220px;
    display: flex;
    flex-direction: column;
    min-height: 100vh;
  }

  .topbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 24px;
    background: var(--bg2);
    border-bottom: 1px solid var(--border);
    position: sticky;
    top: 0;
    z-index: 10;
  }

  .topbar-title {
    font-size: 18px;
    font-weight: 600;
  }

  .topbar-right {
    display: flex;
    align-items: center;
    gap: 12px;
  }

  .topbar-btn {
    background: none;
    border: 1px solid var(--border);
    color: var(--fg-muted);
    width: 32px;
    height: 32px;
    border-radius: 6px;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 12px;
    font-weight: 600;
    transition: all 0.15s;
  }
  .topbar-btn:hover {
    background: var(--bg-hover);
    color: var(--fg);
    border-color: var(--accent);
  }

  .ws-badge {
    display: none;
  }

  .content {
    flex: 1;
    padding: 20px 24px;
    max-width: 960px;
    width: 100%;
    margin: 0 auto;
  }

  /* === Bottom tabs (mobile) === */
  .bottom-tabs {
    display: none;
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    background: var(--bg2);
    border-top: 1px solid var(--border);
    z-index: 20;
    padding-bottom: env(safe-area-inset-bottom, 0);
  }

  .tab-item {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    padding: 6px 4px 8px;
    border: none;
    border-top: 2px solid transparent;
    background: none;
    color: var(--fg-muted);
    font-size: 11px;
    cursor: pointer;
    min-width: 48px;
    min-height: 52px;
    transition: color 0.15s;
  }

  .tab-item.active {
    color: var(--accent);
    border-top-color: var(--accent);
  }

  .tab-label {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 72px;
  }

  /* === "More" overlay === */
  .more-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.4);
    z-index: 25;
    display: flex;
    align-items: flex-end;
    justify-content: center;
  }

  .more-sheet {
    background: var(--bg2);
    border-radius: 16px 16px 0 0;
    width: 100%;
    max-width: 480px;
    padding: 8px 0;
    padding-bottom: calc(60px + env(safe-area-inset-bottom, 0));
  }

  .more-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 12px 20px;
    border-bottom: 1px solid var(--border);
  }

  .more-title {
    font-size: 16px;
    font-weight: 600;
    color: var(--fg);
  }

  .more-close {
    background: none;
    border: none;
    color: var(--fg-muted);
    cursor: pointer;
    width: 36px;
    height: 36px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 8px;
  }

  .more-close:hover {
    background: var(--bg-hover);
  }

  .more-item {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 14px 20px;
    width: 100%;
    border: none;
    background: none;
    color: var(--fg-muted);
    font-size: 15px;
    cursor: pointer;
    transition: background 0.15s;
    text-align: left;
  }

  .more-item:hover {
    background: var(--bg-hover);
  }

  .more-item.active {
    color: var(--accent);
    background: var(--accent-bg);
  }

  /* === Responsive === */
  @media (max-width: 768px) {
    .sidebar { display: none; }
    .main-area { margin-left: 0; }
    .bottom-tabs { display: flex; }
    .ws-badge { display: block; }
    .content {
      padding: 16px;
      padding-bottom: calc(72px + env(safe-area-inset-bottom, 0));
    }
    .topbar {
      padding: 12px 16px;
    }
    .topbar-title {
      font-size: 16px;
    }
  }

  @media (min-width: 769px) and (max-width: 1024px) {
    .sidebar { display: none; }
    .main-area { margin-left: 0; }
    .bottom-tabs { display: flex; }
    .ws-badge { display: block; }
    .content {
      padding: 20px 24px;
      padding-bottom: calc(72px + env(safe-area-inset-bottom, 0));
      max-width: 640px;
    }
    .topbar {
      padding: 14px 20px;
    }
  }
</style>
