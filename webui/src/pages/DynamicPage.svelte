<script>
  import { pages } from '../stores/ui.js';
  import { state } from '../stores/state.js';
  import { isVisible } from '../lib/visibility.js';
  import Card from '../components/Card.svelte';
  import WidgetRenderer from '../components/WidgetRenderer.svelte';

  export let pageId;

  $: page = $pages.find(p => p.id === pageId);
  $: visibleCards = page ? page.cards.filter(c => isVisible(c.visible_when, $state)) : [];
  $: monitorCards = visibleCards.filter(c => c.group !== 'settings');
  $: settingsCards = visibleCards.filter(c => c.group === 'settings');
</script>

{#if page}
  <div class="page-grid">
    {#each monitorCards as card}
      <Card title={card.title} collapsible={card.collapsible || false}>
        {#each card.widgets as widget}
          {#if isVisible(widget.visible_when, $state)}
            <WidgetRenderer {widget} value={$state[widget.key]} />
          {/if}
        {/each}
      </Card>
    {/each}
  </div>
  {#if settingsCards.length > 0}
    <div class="settings-divider">
      <span class="divider-line"></span>
      <span class="divider-text">НАЛАШТУВАННЯ</span>
      <span class="divider-line"></span>
    </div>
    <div class="page-grid">
      {#each settingsCards as card}
        <Card title={card.title} collapsible={true}>
          {#each card.widgets as widget}
            {#if isVisible(widget.visible_when, $state)}
              <WidgetRenderer {widget} value={$state[widget.key]} />
            {/if}
          {/each}
        </Card>
      {/each}
    </div>
  {/if}
{:else}
  <div class="not-found">Page not found</div>
{/if}

<style>
  .page-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0;
  }
  @media (min-width: 768px) {
    .page-grid {
      grid-template-columns: repeat(2, 1fr);
      gap: 16px;
    }
  }
  .settings-divider {
    display: flex;
    align-items: center;
    gap: 12px;
    margin: 16px 0 8px;
    padding: 0 4px;
  }
  .divider-line {
    flex: 1;
    height: 1px;
    background: var(--border);
  }
  .divider-text {
    font-size: 11px;
    font-weight: 700;
    color: var(--fg-dim, #64748b);
    letter-spacing: 1.5px;
    white-space: nowrap;
  }
  .not-found {
    text-align: center;
    color: var(--fg-muted);
    padding: 40px;
    font-size: 16px;
  }
</style>
