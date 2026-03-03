<script>
  import { slide } from 'svelte/transition';
  import Icon from './Icon.svelte';

  export let title = '';
  export let collapsible = false;
  /** @type {'default' | 'status' | 'alarm'} */
  export let variant = 'default';

  // sessionStorage ключ для збереження collapsed стану
  const storageKey = title ? `card-collapsed-${title}` : null;

  // Ініціалізація: sessionStorage > mobile default
  const isMobile = typeof window !== 'undefined' && window.matchMedia('(max-width: 767px)').matches;

  function getInitialCollapsed() {
    if (!collapsible) return false;
    if (storageKey) {
      try {
        const saved = sessionStorage.getItem(storageKey);
        if (saved !== null) return saved === '1';
      } catch (e) {}
    }
    return isMobile;
  }

  let collapsed = getInitialCollapsed();

  function toggle() {
    if (!collapsible) return;
    collapsed = !collapsed;
    if (storageKey) {
      try { sessionStorage.setItem(storageKey, collapsed ? '1' : '0'); } catch (e) {}
    }
  }
</script>

<div class="card" class:card-status={variant === 'status'} class:card-alarm={variant === 'alarm'}>
  {#if title}
    <div
      class="card-title"
      class:collapsible
      on:click={toggle}
      on:keydown={e => e.key === 'Enter' && toggle()}
      role={collapsible ? 'button' : undefined}
      tabindex={collapsible ? 0 : undefined}
    >
      {#if collapsible}
        <span class="arrow" class:open={!collapsed}>
          <Icon name="chevron-right" size={14} />
        </span>
      {/if}
      {title}
    </div>
  {/if}
  {#if !collapsed}
    <div class="card-body" transition:slide={{ duration: 200 }}>
      <slot />
    </div>
  {/if}
</div>

<style>
  .card {
    background: var(--surface);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    margin-bottom: 16px;
    overflow: hidden;
    position: relative;
    transition: background 0.4s ease, border-color 0.4s ease, box-shadow 0.4s ease;
  }

  /* Top-line gradient accent */
  .card::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent 0%, var(--border-accent) 50%, transparent 100%);
    z-index: 1;
  }

  :global(:root[data-theme="light"]) .card {
    box-shadow: var(--shadow-xs);
  }

  .card-status {
    border-color: var(--accent);
  }

  .card-alarm {
    border-color: var(--alarm-border);
    background: var(--alarm-bg);
  }

  .card-title {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-2);
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 12px 16px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 6px;
    font-family: var(--font-display);
  }

  .card-alarm .card-title {
    color: var(--alarm-text);
    border-bottom-color: rgba(239, 68, 68, 0.2);
  }

  .card-title.collapsible {
    cursor: pointer;
    user-select: none;
  }

  .card-title.collapsible:hover {
    color: var(--text-1);
  }

  .card-alarm .card-title.collapsible:hover {
    color: var(--alarm-dark);
  }

  .arrow {
    display: inline-flex;
    transition: transform 0.2s ease;
  }

  .arrow.open {
    transform: rotate(90deg);
  }

  .card-body {
    padding: 12px 16px;
  }
</style>
