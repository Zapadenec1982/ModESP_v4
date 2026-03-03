<script>
  import { fly, fade } from 'svelte/transition';
  import { toasts, dismissToast } from '../stores/toast.js';
</script>

{#if $toasts.length}
  <div class="toast-container">
    {#each $toasts as toast (toast.id)}
      <div class="toast toast-{toast.type}"
           in:fly={{ y: 30, duration: 200 }}
           out:fade={{ duration: 150 }}>
        <span class="toast-icon">
          {#if toast.type === 'success'}&#10003;{:else if toast.type === 'error'}&#10007;{:else}&#9888;{/if}
        </span>
        <span class="toast-msg">{toast.msg}</span>
        <button class="toast-close" on:click|stopPropagation={() => dismissToast(toast.id)} aria-label="Close">
          &times;
        </button>
      </div>
    {/each}
  </div>
{/if}

<style>
  .toast-container {
    position: fixed;
    bottom: 16px;
    left: 50%;
    transform: translateX(-50%);
    z-index: 10000;
    display: flex;
    flex-direction: column;
    gap: 8px;
    max-width: var(--toast-width);
    width: calc(100% - 32px);
    pointer-events: none;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 10px 16px;
    border-radius: var(--radius-sm);
    font-size: 13px;
    font-weight: 500;
    color: #fff;
    backdrop-filter: blur(16px);
    -webkit-backdrop-filter: blur(16px);
    border: 1px solid var(--border-glow);
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.4);
    pointer-events: auto;
  }

  .toast-success { background: rgba(52, 211, 153, 0.9); }
  .toast-error { background: rgba(248, 113, 113, 0.9); }
  .toast-warn { background: rgba(251, 191, 36, 0.9); color: #000; }

  .toast-icon {
    font-size: 16px;
    flex-shrink: 0;
  }

  .toast-msg {
    flex: 1;
    line-height: 1.3;
  }

  .toast-close {
    background: none;
    border: none;
    color: inherit;
    font-size: 18px;
    cursor: pointer;
    padding: 0 4px;
    opacity: 0.7;
    line-height: 1;
    flex-shrink: 0;
  }

  .toast-close:hover {
    opacity: 1;
  }

  .toast-warn .toast-close {
    color: #000;
  }
</style>
