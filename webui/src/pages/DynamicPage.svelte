<script>
  import { fly } from 'svelte/transition';
  import { pages } from '../stores/ui.js';
  import { state } from '../stores/state.js';
  import { t } from '../stores/i18n.js';
  import { isVisible } from '../lib/visibility.js';
  import Card from '../components/Card.svelte';
  import WidgetRenderer from '../components/WidgetRenderer.svelte';

  export let pageId;

  $: page = $pages.find(p => p.id === pageId);
</script>

{#if page}
  <div class="page-grid">
    {#each page.cards as card, i}
      {#if isVisible(card.visible_when, $state)}
        <div in:fly={{ y: 15, duration: 250, delay: i * 50 }}>
        <Card title={card.title} collapsible={card.collapsible || false}>
          {#each card.widgets as widget}
            {#if isVisible(widget.visible_when, $state)}
              <WidgetRenderer {widget} value={$state[widget.key]} />
            {/if}
          {/each}
        </Card>
        </div>
      {/if}
    {/each}
  </div>
{:else}
  <div class="not-found">{$t['page.not_found']}</div>
{/if}

<style>
  .page-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0;
    max-width: 640px;
    margin: 0 auto;
    width: 100%;
  }

  @media (min-width: 1025px) {
    .page-grid {
      grid-template-columns: repeat(2, 1fr);
      gap: 16px;
      max-width: none;
    }
  }

  .not-found {
    text-align: center;
    color: var(--fg-muted);
    padding: 40px;
    font-size: 16px;
  }
</style>
