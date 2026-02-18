<script>
  export let config;
  export let value;

  $: display = value === undefined || value === null
    ? '—'
    : typeof value === 'number'
      ? (Number.isInteger(value) ? value : value.toFixed(1))
      : String(value);
</script>

<div class="widget-row">
  <span class="label">{config.description || config.key}</span>
  <span class="value-group">
    <span class="value">{display}</span>
    {#if config.unit}
      <span class="unit">{config.unit}</span>
    {/if}
  </span>
</div>

<style>
  .widget-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    min-height: 40px;
    padding: 4px 0;
  }
  .label { font-size: 14px; color: var(--fg-muted); }
  .value { font-size: 16px; font-weight: 600; font-variant-numeric: tabular-nums; }
  .unit { font-size: 12px; color: var(--fg-muted); margin-left: 4px; }
  .value-group { display: flex; align-items: baseline; }
</style>
