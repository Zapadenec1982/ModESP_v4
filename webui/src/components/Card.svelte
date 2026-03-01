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
    background: var(--card);
    border-radius: var(--radius-2xl);
    border: 1px solid var(--border);
    margin-bottom: var(--sp-4);
    overflow: hidden;
    transition: box-shadow var(--transition-slow);
  }

  :global(:root[data-theme="light"]) .card {
    box-shadow: var(--shadow-xs);
  }

  /* Variant: status — subtle accent bg */
  .card-status {
    border-color: var(--accent, var(--border));
  }

  /* Variant: alarm — red border + subtle red bg */
  .card-alarm {
    border-color: var(--alarm-border);
    background: var(--alarm-bg);
  }

  .card-title {
    font-size: var(--text-sm);
    font-weight: var(--fw-bold);
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: var(--sp-3) var(--sp-4);
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: var(--sp-1-5);
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
    color: var(--fg);
  }

  .card-alarm .card-title.collapsible:hover {
    color: var(--alarm-dark);
  }

  .arrow {
    display: inline-flex;
    transition: transform var(--transition-normal);
  }

  .arrow.open {
    transform: rotate(90deg);
  }

  .card-body {
    padding: var(--sp-3) var(--sp-4);
  }
</style>
