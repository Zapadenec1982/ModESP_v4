<script>
  import { apiGet } from '../../lib/api.js';
  import { state } from '../../stores/state.js';
  import { t } from '../../stores/i18n.js';
  import Card from '../../components/Card.svelte';
  import NumberInput from '../../components/widgets/NumberInput.svelte';

  export let owBuses = [];
  export let assignedAddresses = new Set();

  let scanning = false;
  let scanResults = [];
  let selectedBus = '';
  let error = null;

  $: hasDs18b20 = !!$state['equipment.has_ds18b20_driver'];

  $: if (owBuses.length > 0 && !selectedBus) selectedBus = owBuses[0].id;

  async function scanBus() {
    if (!selectedBus) return;
    scanning = true;
    error = null;
    scanResults = [];
    try {
      const data = await apiGet(`/api/onewire/scan?bus=${selectedBus}`);
      scanResults = data.devices || [];
    } catch (e) {
      error = e.message;
    } finally {
      scanning = false;
    }
  }
</script>

<Card title="DS18B20 / OneWire">
  <!-- DS18B20 settings -->
  {#if hasDs18b20}
    <div class="settings-section">
      <NumberInput
        config={{ key: 'equipment.filter_coeff', description: $t['eq.filter'] || 'Цифровий фільтр', min: 0, max: 10, step: 1 }}
        value={$state['equipment.filter_coeff']}
      />
      <NumberInput
        config={{ key: 'equipment.ds18b20_offset', description: $t['eq.offset'] || 'Корекція °C', unit: '°C', min: -5, max: 5, step: 0.1 }}
        value={$state['equipment.ds18b20_offset']}
      />
    </div>
    <div class="divider"></div>
  {/if}

  <!-- Діагностичний scan -->
  <div class="scan-controls">
    {#if owBuses.length > 1}
      <select class="hw-select" bind:value={selectedBus}>
        {#each owBuses as bus}
          <option value={bus.id}>{bus.id}{bus.gpio !== undefined ? ` (GPIO ${bus.gpio})` : ''}</option>
        {/each}
      </select>
    {:else}
      <span class="bus-label">{owBuses[0].id}{owBuses[0].gpio !== undefined ? ` (GPIO ${owBuses[0].gpio})` : ''}</span>
    {/if}
    <button class="scan-btn" on:click={scanBus} disabled={scanning}>
      {scanning ? $t['bind.scanning'] : $t['bind.scan']}
    </button>
  </div>

  {#if error}
    <div class="scan-error">{error}</div>
  {/if}

  {#if scanResults.length > 0}
    <div class="scan-summary">
      {$t['bind.found_total'] || 'Знайдено на шині:'} {scanResults.length}
    </div>
    <div class="scan-results">
      {#each scanResults as device}
        {@const isAssigned = assignedAddresses.has(device.address)}
        <div class="device-row" class:device-assigned={isAssigned}>
          <div class="device-info">
            <span class="device-addr">{device.address}</span>
            {#if device.temperature !== undefined}
              <span class="device-temp">{device.temperature.toFixed(1)} °C</span>
            {/if}
          </div>
          {#if isAssigned}
            <span class="device-badge">{$t['bind.in_use'] || 'зайнято'}</span>
          {/if}
        </div>
      {/each}
    </div>
  {:else if !scanning}
    <div class="scan-hint">{$t['bind.scan_hint']}</div>
  {/if}
</Card>

<style>
  .settings-section {
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  .divider {
    height: 1px;
    background: var(--border);
    margin: 12px 0;
  }
  .scan-controls {
    display: flex;
    gap: 8px;
    align-items: center;
    margin-bottom: 12px;
  }
  .hw-select {
    flex: 1;
    padding: 8px 12px;
    font-size: 14px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--fg);
    cursor: pointer;
    appearance: auto;
  }
  .bus-label {
    font-size: 14px;
    color: var(--fg);
    font-family: monospace;
  }
  .scan-btn {
    padding: 8px 16px;
    border-radius: 6px;
    border: 1px solid var(--accent);
    background: transparent;
    color: var(--accent);
    cursor: pointer;
    font-size: 13px;
    white-space: nowrap;
  }
  .scan-btn:hover { background: var(--accent-bg); }
  .scan-btn:disabled { opacity: 0.4; cursor: not-allowed; }
  .scan-error {
    font-size: 13px;
    color: var(--error);
    margin-bottom: 8px;
  }
  .scan-results {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .scan-summary {
    font-size: 12px;
    color: var(--fg-muted);
    margin-bottom: 8px;
  }
  .device-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 8px;
    padding: 8px 10px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
  }
  .device-info {
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
  }
  .device-addr {
    font-size: 12px;
    font-family: monospace;
    color: var(--fg);
  }
  .device-temp {
    font-size: 14px;
    font-weight: 600;
    color: var(--accent);
    font-variant-numeric: tabular-nums;
  }
  .device-assigned {
    border-color: var(--accent);
    background: var(--accent-bg);
  }
  .device-badge {
    font-size: 12px;
    color: var(--fg-muted);
  }
  .scan-hint {
    font-size: 13px;
    color: var(--fg-muted);
    text-align: center;
    padding: 16px 0;
  }
</style>
