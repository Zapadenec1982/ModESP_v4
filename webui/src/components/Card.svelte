<script>
  import Icon from './Icon.svelte';

  export let title = '';
  export let collapsible = false;

  let collapsed = false;

  function toggle() {
    if (collapsible) collapsed = !collapsed;
  }
</script>

<div class="card">
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
    <div class="card-body">
      <slot />
    </div>
  {/if}
</div>

<style>
  .card {
    background: var(--card);
    border-radius: 12px;
    border: 1px solid var(--border);
    margin-bottom: 16px;
    overflow: hidden;
  }

  .card-title {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 1px;
    padding: 14px 18px;
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 6px;
  }

  .card-title.collapsible {
    cursor: pointer;
    user-select: none;
  }

  .card-title.collapsible:hover {
    color: var(--fg);
  }

  .arrow {
    display: inline-flex;
    transition: transform 0.2s;
  }

  .arrow.open {
    transform: rotate(90deg);
  }

  .card-body {
    padding: 14px 18px;
  }
</style>
