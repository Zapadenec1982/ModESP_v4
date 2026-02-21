<script>
  import { onMount, onDestroy } from 'svelte';
  import { pages, deviceName } from '../stores/ui.js';
  import { wsConnected, state } from '../stores/state.js';
  import Icon from './Icon.svelte';

  export let currentPage = 'dashboard';

  // AUDIT-009: alarm banner на всіх сторінках
  $: alarmActive = $state['protection.alarm_active'];
  $: alarmCode = $state['protection.alarm_code'];

  // Годинник в topbar — тікає щосекунди локально,
  // синхронізується з system.time через WebSocket (кожні 5с)
  let clockTime = '';
  let clockDate = '';
  let tickTimer = null;
  let serverSeconds = 0; // секунди з останнього sync

  // Парсимо "HH:MM:SS" → секунди від початку доби
  function parseHMS(s) {
    if (!s || s === '--:--:--') return -1;
    const p = s.split(':');
    return (+p[0]) * 3600 + (+p[1]) * 60 + (+p[2] || 0);
  }

  // Форматуємо секунди → "HH:MM:SS"
  function fmtHMS(sec) {
    const s = ((sec % 86400) + 86400) % 86400;
    const h = String(Math.floor(s / 3600)).padStart(2, '0');
    const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    const ss = String(s % 60).padStart(2, '0');
    return `${h}:${m}:${ss}`;
  }

  // Sync з WebSocket state
  $: {
    const st = $state['system.time'];
    const sd = $state['system.date'];
    if (st && st !== '--:--:--') {
      serverSeconds = parseHMS(st);
      clockTime = st;
    }
    if (sd && sd !== '--.--.----') clockDate = sd;
  }

  onMount(() => {
    tickTimer = setInterval(() => {
      if (serverSeconds >= 0) {
        serverSeconds = (serverSeconds + 1) % 86400;
        clockTime = fmtHMS(serverSeconds);
      }
    }, 1000);
  });

  onDestroy(() => {
    if (tickTimer) clearInterval(tickTimer);
  });

  function navigate(id) {
    currentPage = id;
  }

  $: currentTitle = $pages.find(p => p.id === currentPage)?.title || '';
  $: sortedPages = [...$pages].sort((a, b) => (a.order || 0) - (b.order || 0));
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
        {$wsConnected ? 'Online' : 'Offline'}
      </div>
    </div>
  </aside>

  <!-- Main content -->
  <div class="main-area">
    <!-- AUDIT-009: alarm banner на всіх сторінках -->
    {#if alarmActive}
      <div class="alarm-banner" on:click={() => navigate('protection')}>
        ALARM: {alarmCode ? String(alarmCode).toUpperCase().replace('_', ' ') : 'ACTIVE'}
      </div>
    {/if}
    <header class="topbar">
      <h1 class="topbar-title">{currentTitle}</h1>
      <div class="topbar-right">
        {#if clockTime}
          <div class="topbar-clock">
            <Icon name="clock" size={14} />
            <span class="clock-time">{clockTime}</span>
            <span class="clock-date">{clockDate}</span>
          </div>
        {/if}
        <div class="ws-badge" class:connected={$wsConnected}>
          <span class="ws-dot"></span>
        </div>
      </div>
    </header>
    <main class="content">
      <slot />
    </main>
  </div>

  <!-- Bottom tabs (mobile) -->
  <nav class="bottom-tabs">
    {#each sortedPages as page}
      <button
        class="tab-item"
        class:active={currentPage === page.id}
        on:click={() => navigate(page.id)}
      >
        <Icon name={page.icon || 'home'} size={20} />
        <span class="tab-label">{page.title}</span>
      </button>
    {/each}
  </nav>
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
    background: #991b1b;
    color: #fff;
    text-align: center;
    padding: 8px 16px;
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 1px;
    cursor: pointer;
    animation: alarm-flash 1.5s infinite;
  }

  @keyframes alarm-flash {
    0%, 100% { background: #991b1b; }
    50% { background: #dc2626; }
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

  .topbar-clock {
    display: flex;
    align-items: center;
    gap: 6px;
    color: var(--fg-muted);
    font-size: 13px;
    font-variant-numeric: tabular-nums;
  }

  .clock-time {
    font-weight: 500;
    color: var(--fg);
  }

  .clock-date {
    opacity: 0.7;
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
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
    scrollbar-width: none;
  }

  .bottom-tabs::-webkit-scrollbar { display: none; }

  .tab-item {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    padding: 8px 4px 10px;
    border: none;
    background: none;
    color: var(--fg-muted);
    font-size: 10px;
    cursor: pointer;
    min-width: 56px;
    transition: color 0.15s;
  }

  .tab-item.active {
    color: var(--accent);
  }

  .tab-label {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 64px;
  }

  /* === Responsive === */
  @media (max-width: 768px) {
    .sidebar { display: none; }
    .main-area { margin-left: 0; }
    .bottom-tabs { display: flex; }
    .ws-badge { display: block; }
    .content {
      padding: 16px;
      padding-bottom: 72px;
    }
    .topbar {
      padding: 12px 16px;
    }
    .topbar-title {
      font-size: 16px;
    }
  }

  @media (min-width: 769px) and (max-width: 1024px) {
    .sidebar { width: 64px; }
    .sidebar-header { justify-content: center; padding: 16px 8px; }
    .logo-text { display: none; }
    .nav-item { justify-content: center; padding: 12px; }
    .nav-label { display: none; }
    .sidebar-footer { text-align: center; padding: 12px 8px; }
    .ws-status { justify-content: center; font-size: 0; }
    .main-area { margin-left: 64px; }
  }
</style>
