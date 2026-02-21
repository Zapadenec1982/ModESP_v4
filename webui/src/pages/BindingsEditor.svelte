<script>
  import { onMount } from 'svelte';
  import { apiGet, apiPost } from '../lib/api.js';
  import { pages } from '../stores/ui.js';
  import { state } from '../stores/state.js';
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
  <div class="center-msg">Loading...</div>
{:else if error && !bindings.length}
  <div class="center-msg error">{error}</div>
{:else}
  {#if needsRestart}
    <div class="restart-banner">
      Bindings saved. Restart required.
      <button class="restart-btn" on:click={restart}>Restart now</button>
    </div>
  {/if}

  {#if error}
    <div class="error-banner">{error}</div>
  {/if}

  {#if missingRequired.length > 0}
    <div class="warning-banner">
      Required: {missingRequired.map(r => r.label).join(', ')}
    </div>
  {/if}

  <!-- Live equipment status -->
  {#if assignedRoles.size > 0}
    <Card title="Стан обладнання">
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
      {saving ? 'Saving...' : 'Save'}
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
</style>
