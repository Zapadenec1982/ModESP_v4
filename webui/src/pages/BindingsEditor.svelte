<script>
  import { onMount } from 'svelte';
  import { apiGet, apiPost } from '../lib/api.js';
  import { pages } from '../stores/ui.js';
  import { state } from '../stores/state.js';
  import { t } from '../stores/i18n.js';
  import Card from '../components/Card.svelte';

  // Roles/hardware metadata з ui.json bindings page
  $: bindingsPage = $pages.find(p => p.id === 'bindings') || {};
  $: roles = bindingsPage.roles || [];
  $: hwInventory = bindingsPage.hardware || [];

  // Поточні bindings (завантажуються з /api/bindings)
  let bindings = [];
  let loading = true;
  let error = null;
  let saving = false;
  let needsRestart = false;

  onMount(async () => {
    try {
      const data = await apiGet('/api/bindings');
      bindings = data.bindings || [];
    } catch (e) {
      error = e.message;
    } finally {
      loading = false;
    }
  });

  // ── OneWire Discovery ──
  let scanning = false;
  let scanResults = [];
  let selectedBus = '';

  // OneWire шини з hardware inventory
  $: owBuses = hwInventory.filter(h => h.hw_type === 'onewire_bus');
  $: if (owBuses.length > 0 && !selectedBus) selectedBus = owBuses[0].id;

  // Адреси вже присвоєні в локальних bindings
  $: assignedAddresses = new Set(
    bindings.filter(b => b.address).map(b => b.address)
  );

  // Вільні ролі з requires_address (ds18b20 sensors, ще не assigned)
  $: freeAddrRoles = roles
    .filter(r => r.requires_address && !assignedRoles.has(r.role));

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
    // Знайти roleDef для визначення module та driver
    const roleDef = roles.find(r => r.role === device.selectedRole);
    bindings = [...bindings, {
      hardware: selectedBus,
      driver: roleDef ? roleDef.driver : 'ds18b20',
      role: device.selectedRole,
      module: 'equipment',
      address: device.address,
    }];
    // Оновити scan результат
    scanResults = scanResults.map(d =>
      d.address === device.address
        ? {...d, status: 'assigned', role: device.selectedRole}
        : d
    );
  }

  // Знайти binding для ролі
  function getBinding(role) {
    return bindings.find(b => b.role === role);
  }

  // Сумісне hardware для ролі (по hw_type)
  function compatibleHw(roleDef) {
    return hwInventory.filter(h => h.hw_type === roleDef.hw_type);
  }

  // Які hardware вже зайняті (крім поточної ролі)
  function usedHwIds(excludeRole) {
    return new Set(bindings
      .filter(b => b.role !== excludeRole)
      .map(b => b.hardware));
  }

  // Доступне hardware для вибору (сумісне + незайняте)
  function availableHw(roleDef) {
    const used = usedHwIds(roleDef.role);
    return compatibleHw(roleDef).filter(h => !used.has(h.id));
  }

  // Додати роль з першим доступним hardware
  function addRole(roleDef) {
    const hw = availableHw(roleDef);
    if (hw.length === 0) return;
    bindings = [...bindings, {
      hardware: hw[0].id,
      driver: roleDef.driver,
      role: roleDef.role,
      module: 'equipment',
      ...(roleDef.requires_address ? { address: '' } : {}),
    }];
  }

  // Видалити роль
  function removeRole(role) {
    bindings = bindings.filter(b => b.role !== role);
  }

  // Оновити hardware для ролі
  function setHardware(role, hwId) {
    bindings = bindings.map(b =>
      b.role === role ? { ...b, hardware: hwId } : b
    );
  }

  // Оновити адресу для ролі
  function setAddress(role, addr) {
    bindings = bindings.map(b =>
      b.role === role ? { ...b, address: addr } : b
    );
  }

  // Валідація: всі required ролі мають binding
  $: requiredRoles = roles.filter(r => !r.optional);
  $: assignedRoles = new Set(bindings.map(b => b.role));
  $: missingRequired = requiredRoles.filter(r => !assignedRoles.has(r.role));
  $: canSave = missingRequired.length === 0 && !saving;

  // Unassigned optional ролі
  $: unassignedOptional = roles
    .filter(r => r.optional && !assignedRoles.has(r.role))
    .filter(r => availableHw(r).length > 0);

  // State key mapping: role → equipment.* state key
  const ROLE_STATE_KEY = {
    air_temp: 'equipment.air_temp', evap_temp: 'equipment.evap_temp',
    condenser_temp: 'equipment.condenser_temp',
    compressor: 'equipment.compressor', heater: 'equipment.heater',
    evap_fan: 'equipment.evap_fan', cond_fan: 'equipment.cond_fan',
    hg_valve: 'equipment.hg_valve', door_contact: 'equipment.door_open',
  };
  const ROLE_OK_KEY = {
    air_temp: 'equipment.sensor1_ok', evap_temp: 'equipment.sensor2_ok',
  };

  function formatValue(role, val) {
    if (val === undefined || val === null) return '--';
    if (typeof val === 'number') return val.toFixed(1) + ' °C';
    return val ? 'ON' : 'OFF';
  }

  // Розділити assigned ролі на sensors і actuators
  $: assignedSensors = roles
    .filter(r => r.type === 'sensor' && assignedRoles.has(r.role));
  $: assignedActuators = roles
    .filter(r => r.type === 'actuator' && assignedRoles.has(r.role));

  async function save() {
    saving = true;
    error = null;
    try {
      const payload = {
        manifest_version: 1,
        bindings: bindings,
      };
      const res = await apiPost('/api/bindings', payload);
      if (res.needs_restart) {
        needsRestart = true;
      }
    } catch (e) {
      error = e.message;
    } finally {
      saving = false;
    }
  }

  async function restart() {
    try {
      await apiPost('/api/restart', {});
    } catch (_) {
      // Connection lost is expected during restart
    }
    // Wait then reload
    setTimeout(() => location.reload(), 5000);
  }
</script>

{#if loading}
  <div class="center-msg">{$t['bind.loading']}</div>
{:else if error && !bindings.length}
  <div class="center-msg error">{error}</div>
{:else}
  {#if needsRestart}
    <div class="restart-banner">
      {$t['bind.saved']}
      <button class="restart-btn" on:click={restart}>{$t['bind.restart']}</button>
    </div>
  {/if}

  {#if error}
    <div class="error-banner">{error}</div>
  {/if}

  {#if missingRequired.length > 0}
    <div class="warning-banner">
      {$t['bind.required']}: {missingRequired.map(r => r.label).join(', ')}
    </div>
  {/if}

  <!-- Live equipment status -->
  {#if assignedRoles.size > 0}
    <Card title={$t['bind.status']}>
      <div class="status-grid">
        {#each [...assignedSensors, ...assignedActuators] as roleDef}
          {@const stKey = ROLE_STATE_KEY[roleDef.role]}
          {@const val = stKey ? $state[stKey] : undefined}
          {@const okKey = ROLE_OK_KEY[roleDef.role]}
          {@const ok = okKey ? $state[okKey] : undefined}
          <div class="status-item">
            <span class="status-label">{roleDef.label}</span>
            <span class="status-value" class:on={val === true} class:off={val === false}
                  class:err={ok === false}>
              {formatValue(roleDef.role, val)}
            </span>
          </div>
        {/each}
      </div>
    </Card>
  {/if}

  <!-- Sensors card -->
  {#if assignedSensors.length > 0}
    <Card title="Sensors">
      {#each assignedSensors as roleDef}
        {@const binding = getBinding(roleDef.role)}
        {#if binding}
          <div class="binding-row">
            <div class="role-info">
              <span class="role-label">{roleDef.label}</span>
              {#if roleDef.optional}
                <button class="remove-btn" on:click={() => removeRole(roleDef.role)}
                        title="Remove">&#x2715;</button>
              {/if}
            </div>
            <select class="hw-select"
                    value={binding.hardware}
                    on:change={e => setHardware(roleDef.role, e.target.value)}>
              {#each compatibleHw(roleDef) as hw}
                {@const used = usedHwIds(roleDef.role).has(hw.id)}
                <option value={hw.id} disabled={used}>
                  {hw.label}{hw.gpio !== undefined ? ` (GPIO ${hw.gpio})` : ''}{used ? ' — used' : ''}
                </option>
              {/each}
            </select>
            {#if roleDef.requires_address}
              <input class="addr-input"
                     type="text"
                     placeholder="28:FF:AA:BB:CC:DD:EE:01"
                     value={binding.address || ''}
                     on:input={e => setAddress(roleDef.role, e.target.value)} />
            {/if}
          </div>
        {/if}
      {/each}
    </Card>
  {/if}

  <!-- Actuators card -->
  {#if assignedActuators.length > 0}
    <Card title="Actuators">
      {#each assignedActuators as roleDef}
        {@const binding = getBinding(roleDef.role)}
        {#if binding}
          <div class="binding-row">
            <div class="role-info">
              <span class="role-label">{roleDef.label}</span>
              {#if roleDef.optional}
                <button class="remove-btn" on:click={() => removeRole(roleDef.role)}
                        title="Remove">&#x2715;</button>
              {/if}
            </div>
            <select class="hw-select"
                    value={binding.hardware}
                    on:change={e => setHardware(roleDef.role, e.target.value)}>
              {#each compatibleHw(roleDef) as hw}
                {@const used = usedHwIds(roleDef.role).has(hw.id)}
                <option value={hw.id} disabled={used}>
                  {hw.label}{hw.gpio !== undefined ? ` (GPIO ${hw.gpio})` : ''}{used ? ' — used' : ''}
                </option>
              {/each}
            </select>
          </div>
        {/if}
      {/each}
    </Card>
  {/if}

  <!-- OneWire Discovery -->
  {#if owBuses.length > 0}
    <Card title="OneWire Discovery">
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

      {#if scanResults.length > 0}
        {@const newDevices = scanResults.filter(d =>
          d.status !== 'assigned' && !assignedAddresses.has(d.address)
        )}
        {@const assignedCount = scanResults.length - newDevices.length}
        {#if assignedCount > 0}
          <div class="scan-summary">Found {scanResults.length} device(s), {assignedCount} already assigned</div>
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
                    <option value="">-- Role --</option>
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
                  <span class="device-role new">new</span>
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
  {/if}

  <!-- Add optional roles -->
  {#if unassignedOptional.length > 0}
    <Card title="Add Equipment">
      {#each unassignedOptional as roleDef}
        <button class="add-role-btn" on:click={() => addRole(roleDef)}>
          + {roleDef.label}
        </button>
      {/each}
    </Card>
  {/if}

  <!-- Save button -->
  <div class="save-area">
    <button class="save-btn" disabled={!canSave} on:click={save}>
      {saving ? $t['bind.saving'] : $t['bind.save']}
    </button>
  </div>
{/if}

<style>
  .center-msg {
    text-align: center;
    color: var(--fg-muted);
    padding: 40px;
    font-size: 16px;
  }
  .center-msg.error { color: var(--error); }

  .restart-banner {
    background: rgba(34, 197, 94, 0.15);
    border: 1px solid var(--success);
    border-radius: 8px;
    padding: 12px 16px;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 14px;
    color: var(--success);
  }
  .restart-btn {
    padding: 6px 16px;
    border-radius: 6px;
    border: 1px solid var(--success);
    background: transparent;
    color: var(--success);
    cursor: pointer;
    font-size: 13px;
  }
  .restart-btn:hover { background: rgba(34, 197, 94, 0.2); }

  .error-banner {
    background: rgba(239, 68, 68, 0.15);
    border: 1px solid var(--error);
    border-radius: 8px;
    padding: 10px 16px;
    margin-bottom: 16px;
    font-size: 13px;
    color: var(--error);
  }

  .warning-banner {
    background: rgba(245, 158, 11, 0.15);
    border: 1px solid var(--warning);
    border-radius: 8px;
    padding: 10px 16px;
    margin-bottom: 16px;
    font-size: 13px;
    color: var(--warning);
  }

  .binding-row {
    padding: 10px 0;
    border-bottom: 1px solid var(--border);
  }
  .binding-row:last-child { border-bottom: none; }

  .role-info {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 8px;
  }

  .role-label {
    font-size: 14px;
    font-weight: 500;
    color: var(--fg);
  }

  .remove-btn {
    background: none;
    border: none;
    color: var(--fg-muted);
    cursor: pointer;
    font-size: 16px;
    padding: 2px 6px;
    border-radius: 4px;
  }
  .remove-btn:hover { color: var(--error); background: rgba(239, 68, 68, 0.1); }

  .hw-select {
    width: 100%;
    padding: 8px 12px;
    font-size: 14px;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--fg);
    cursor: pointer;
    appearance: auto;
  }
  .hw-select:focus { outline: none; border-color: var(--accent); }

  .addr-input {
    width: 100%;
    margin-top: 8px;
    padding: 8px 12px;
    font-size: 13px;
    font-family: monospace;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    color: var(--fg);
  }
  .addr-input:focus { outline: none; border-color: var(--accent); }
  .addr-input::placeholder { color: var(--fg-muted); opacity: 0.6; }

  .add-role-btn {
    display: block;
    width: 100%;
    padding: 10px;
    margin-bottom: 8px;
    border-radius: 6px;
    border: 1px dashed var(--border);
    background: transparent;
    color: var(--accent);
    cursor: pointer;
    font-size: 14px;
    text-align: left;
  }
  .add-role-btn:last-child { margin-bottom: 0; }
  .add-role-btn:hover { background: var(--accent-bg); border-color: var(--accent); }

  .save-area {
    padding: 16px 0;
  }

  .save-btn {
    width: 100%;
    padding: 12px;
    border-radius: 8px;
    border: none;
    background: var(--accent);
    color: white;
    font-size: 15px;
    font-weight: 600;
    cursor: pointer;
  }
  .save-btn:hover { opacity: 0.9; }
  .save-btn:disabled { opacity: 0.4; cursor: not-allowed; }

  .status-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(140px, 1fr));
    gap: 8px;
  }

  .status-item {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 8px 10px;
    background: var(--bg);
    border-radius: 6px;
    border: 1px solid var(--border);
  }

  .status-label {
    font-size: 11px;
    color: var(--fg-muted);
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }

  .status-value {
    font-size: 16px;
    font-weight: 600;
    color: var(--fg);
    font-variant-numeric: tabular-nums;
  }
  .status-value.on { color: var(--success); }
  .status-value.off { color: var(--fg-muted); }
  .status-value.err { color: var(--error); }

  /* OneWire Discovery */
  .scan-controls {
    display: flex;
    gap: 8px;
    align-items: center;
    margin-bottom: 12px;
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
