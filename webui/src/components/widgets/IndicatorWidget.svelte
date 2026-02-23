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
    <span class="dot-ring" class:on={isOn}>
      <span class="dot" style="background: {color}"></span>
    </span>
    <span class="indicator-label">{label}</span>
  </div>
</div>

<style>
  .widget-row {
    display: flex; align-items: center; justify-content: space-between;
    min-height: var(--widget-min-h, 44px); padding: var(--widget-pad, 6px 0);
  }
  .label { font-size: 14px; color: var(--fg-muted); }
  .indicator { display: flex; align-items: center; gap: 10px; }
  .dot-ring {
    width: 18px; height: 18px;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    border: 2px solid var(--border);
    transition: all 0.3s;
  }
  .dot-ring.on {
    border-color: var(--success);
    box-shadow: 0 0 8px var(--success);
  }
  .dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    transition: all 0.3s;
  }
  .indicator-label { font-size: 14px; font-weight: 500; }
</style>
