<script>
  import { pages } from '../stores/ui.js';
  import { state } from '../stores/state.js';
  import { isVisible } from '../lib/visibility.js';
  import Card from '../components/Card.svelte';
  import WidgetRenderer from '../components/WidgetRenderer.svelte';

  export let pageId;

  $: page = $pages.find(p => p.id === pageId);
</script>

{#if page}
  <div class="page-grid">
    {#each page.cards as card}
      {#if isVisible(card.visible_when, $state)}
        <Card title={card.title} collapsible={card.collapsible || false}>
          {#each card.widgets as widget}
            {#if isVisible(widget.visible_when, $state)}
              <WidgetRenderer {widget} value={$state[widget.key]} />
            {/if}
          {/each}
        </Card>
      {/if}
    {/each}
  </div>
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

  .not-found {
    text-align: center;
    color: var(--fg-muted);
    padding: 40px;
    font-size: 16px;
  }
</style>
