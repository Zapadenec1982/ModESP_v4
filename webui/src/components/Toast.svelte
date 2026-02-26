<script>
  import { fly, fade } from 'svelte/transition';
  import { toasts } from '../stores/toast.js';

  function dismiss(id) {
    toasts.update(t => t.filter(x => x.id !== id));
  }
</script>

{#if $toasts.length}
  <div class="toast-container">
    {#each $toasts as toast (toast.id)}
      <div class="toast toast-{toast.type}"
           in:fly={{ y: -30, duration: 200 }}
           out:fade={{ duration: 150 }}
           on:click={() => dismiss(toast.id)}>
        <span class="toast-icon">
          {#if toast.type === 'success'}&#10003;{:else if toast.type === 'error'}&#10007;{:else}&#9888;{/if}
        </span>
        <span class="toast-msg">{toast.msg}</span>
      </div>
    {/each}
  </div>
{/if}

<style>
  .toast-container {
    position: fixed;
    top: 12px;
    right: 12px;
    z-index: 9999;
    display: flex;
    flex-direction: column;
    gap: 8px;
    max-width: 360px;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 10px 16px;
    border-radius: 8px;
    font-size: 13px;
    color: #fff;
    cursor: pointer;
    box-shadow: 0 4px 12px rgba(0,0,0,0.3);
  }

  .toast-success { background: var(--success, #22c55e); }
  .toast-error { background: var(--error, #ef4444); }
  .toast-warn { background: var(--warning, #f59e0b); color: #000; }

  .toast-icon {
    font-size: 16px;
    flex-shrink: 0;
  }

  .toast-msg {
    flex: 1;
    line-height: 1.3;
  }
</style>
