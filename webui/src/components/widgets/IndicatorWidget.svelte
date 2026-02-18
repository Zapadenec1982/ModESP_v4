<script>
  export let config;
  export let value;

  $: isOn = !!value;
  $: color = isOn ? (config.on_color || 'var(--success)') : (config.off_color || 'var(--fg-muted)');
  $: label = isOn ? (config.on_label || 'ON') : (config.off_label || 'OFF');
</script>

<div class="widget-row">
  <span class="label">{config.description || config.key}</span>
  <div class="indicator">
    <span class="dot" class:on={isOn} style="background: {color}; {isOn ? `box-shadow: 0 0 8px ${color}` : ''}"></span>
    <span class="indicator-label">{label}</span>
  </div>
</div>

<style>
  .widget-row {
    display: flex; align-items: center; justify-content: space-between;
    min-height: 40px; padding: 4px 0;
  }
  .label { font-size: 14px; color: var(--fg-muted); }
  .indicator { display: flex; align-items: center; gap: 10px; }
  .dot {
    width: 12px; height: 12px; border-radius: 50%;
    transition: all 0.3s;
  }
  .indicator-label { font-size: 14px; font-weight: 500; }
</style>
