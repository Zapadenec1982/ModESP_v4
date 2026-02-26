<script>
  import { createEventDispatcher } from 'svelte';
  import { apiGet } from '../../lib/api.js';
  import { t } from '../../stores/i18n.js';
  import Card from '../../components/Card.svelte';

  export let owBuses = [];
  export let assignedAddresses = new Set();
  export let freeAddrRoles = [];

  const dispatch = createEventDispatcher();

  let scanning = false;
  let scanResults = [];
  let selectedBus = '';
  let error = null;

  $: if (owBuses.length > 0 && !selectedBus) selectedBus = owBuses[0].id;

  async function scanBus() {
    if (!selectedBus) return;
    scanning = true;
    error = null;
    scanResults = [];
    try {
      const data = await apiGet(`/api/onewire/scan?bus=${selectedBus}`);
      scanResults = (data.devices || []).map(d => ({...d, selectedRole: ''}));
    } catch (e) {
      error = e.message;
    } finally {
      scanning = false;
    }
  }

  function assignDevice(device) {
    if (!device.selectedRole) return;
    dispatch('assign', {
      bus: selectedBus,
      address: device.address,
      role: device.selectedRole,
    });
    scanResults = scanResults.map(d =>
      d.address === device.address
        ? {...d, status: 'assigned', role: device.selectedRole}
        : d
    );
  }
</script>

<Card title={$t['bind.onewire']}>
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
    {@const newDevices = scanResults.filter(d =>
      d.status !== 'assigned' && !assignedAddresses.has(d.address)
    )}
    {@const assignedCount = scanResults.length - newDevices.length}
    {#if assignedCount > 0}
      <div class="scan-summary">{$t['bind.found'].replace('{0}', scanResults.length).replace('{1}', assignedCount)}</div>
    {/if}
    <div class="scan-results">
      {#each newDevices as device}
        <div class="device-row">
          <div class="device-info">
            <span class="device-addr">{device.address}</span>
            {#if device.temperature !== undefined}
              <span class="device-temp">{device.temperature.toFixed(1)} °C</span>
            {/if}
          </div>
          <div class="device-action">
            {#if freeAddrRoles.length > 0}
              <select class="role-select" bind:value={device.selectedRole}>
                <option value="">{$t['bind.role']}</option>
                {#each freeAddrRoles as r}
                  <option value={r.role}>{r.label}</option>
                {/each}
              </select>
              <button class="assign-btn"
                      disabled={!device.selectedRole}
                      on:click={() => assignDevice(device)}>
                {$t['bind.add']}
              </button>
            {:else}
              <span class="device-role new">{$t['bind.new_device']}</span>
            {/if}
          </div>
        </div>
      {/each}
      {#if newDevices.length === 0}
        <div class="scan-hint">{$t['bind.all_assigned']}</div>
      {/if}
    </div>
  {:else if !scanning}
    <div class="scan-hint">{$t['bind.scan_hint']}</div>
  {/if}
</Card>

<style>
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
  .device-action {
    display: flex;
    gap: 6px;
    align-items: center;
    flex-shrink: 0;
  }
  .device-role {
    font-size: 13px;
    color: var(--success);
    font-weight: 500;
  }
  .device-role.new {
    color: var(--fg-muted);
    font-style: italic;
  }
  .role-select {
    padding: 4px 8px;
    font-size: 13px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 4px;
    color: var(--fg);
    max-width: 120px;
  }
  .assign-btn {
    padding: 4px 12px;
    border-radius: 4px;
    border: 1px solid var(--accent);
    background: var(--accent);
    color: white;
    cursor: pointer;
    font-size: 12px;
  }
  .assign-btn:hover { opacity: 0.9; }
  .assign-btn:disabled { opacity: 0.4; cursor: not-allowed; }
  .scan-hint {
    font-size: 13px;
    color: var(--fg-muted);
    text-align: center;
    padding: 16px 0;
  }
</style>
