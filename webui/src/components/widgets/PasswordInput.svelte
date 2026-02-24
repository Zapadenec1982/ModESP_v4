<script>
  import { t } from '../../stores/i18n.js';

  export let config;
  export let value;

  export let inputValue = '';
  let revealed = false;

  $: if (value !== undefined && value !== null && !inputValue) {
    inputValue = String(value);
  }
</script>

<div class="input-widget">
  <div class="input-label">{config.description || config.key}</div>
  <div class="password-row">
    {#if revealed}
      <input
        type="text"
        class="text-input"
        placeholder={config.description || ''}
        bind:value={inputValue}
      />
    {:else}
      <input
        type="password"
        class="text-input"
        placeholder={config.description || ''}
        bind:value={inputValue}
      />
    {/if}
    <button class="reveal-btn" on:click={() => revealed = !revealed}>
      {revealed ? $t['pass.hide'] : $t['pass.show']}
    </button>
  </div>
</div>

<style>
  .input-widget { padding: 4px 0; }
  .input-label { font-size: 14px; color: var(--fg-muted); margin-bottom: 6px; }
  .password-row { display: flex; gap: 6px; }
  .text-input {
    flex: 1;
    background: var(--bg);
    border: 1px solid var(--border);
    color: var(--fg);
    border-radius: 8px;
    padding: 10px 12px;
    font-size: 14px;
    transition: border-color 0.2s;
  }
  .text-input:focus { border-color: var(--accent); outline: none; }
  .reveal-btn {
    width: 40px;
    border: 1px solid var(--border);
    background: var(--border);
    border-radius: 8px;
    cursor: pointer;
    font-size: 16px;
    display: flex; align-items: center; justify-content: center;
  }
  .reveal-btn:hover { background: var(--bg-hover); }
</style>
