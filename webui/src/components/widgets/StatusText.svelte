<script>
  export let config;
  export let value;

  const statusColors = {
    idle: 'var(--fg-muted)',
    cooling: 'var(--accent)',
    safe_mode: 'var(--warning)',
    connected: 'var(--success)',
    disconnected: 'var(--error)',
    error: 'var(--error)'
  };

  $: display = value !== undefined && value !== null ? String(value) : '—';
  $: badgeColor = statusColors[display] || 'var(--fg-muted)';
</script>

<div class="widget-row">
  <span class="label">{config.description || config.key}</span>
  <span class="badge" style="color: {badgeColor}; border-color: {badgeColor}">{display}</span>
</div>

<style>
  .widget-row {
    display: flex; align-items: center; justify-content: space-between;
    min-height: 40px; padding: 4px 0;
  }
  .label { font-size: 14px; color: var(--fg-muted); }
  .badge {
    font-size: 13px; font-weight: 600;
    padding: 3px 12px;
    border: 1px solid;
    border-radius: 20px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }
</style>
