<script>
  import { onMount } from 'svelte';
  import { apiGet, apiPost } from '../lib/api.js';
  import { pages } from '../stores/ui.js';
  import { t } from '../stores/i18n.js';
  import Card from '../components/Card.svelte';
  import EquipmentStatus from './bindings/EquipmentStatus.svelte';
  import BindingCard from './bindings/BindingCard.svelte';
  import OneWireDiscovery from './bindings/OneWireDiscovery.svelte';

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

  // ── Hardware helpers ──
  function compatibleHw(roleDef) {
    return hwInventory.filter(h => h.hw_type === roleDef.hw_type);
  }

  function usedHwIds(excludeRole) {
    return new Set(bindings
      .filter(b => b.role !== excludeRole)
      .map(b => b.hardware));
  }

  function availableHw(roleDef) {
    const used = usedHwIds(roleDef.role);
    return compatibleHw(roleDef).filter(h => !used.has(h.id));
  }

  // ── Binding mutations ──
  function getBinding(role) {
    return bindings.find(b => b.role === role);
  }

  function setHardware(role, hwId) {
    bindings = bindings.map(b =>
      b.role === role ? { ...b, hardware: hwId } : b
    );
  }

  function setAddress(role, addr) {
    bindings = bindings.map(b =>
      b.role === role ? { ...b, address: addr } : b
    );
  }

  function removeRole(role) {
    bindings = bindings.filter(b => b.role !== role);
  }

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

  function handleAssign(e) {
    const { bus, address, role } = e.detail;
    const roleDef = roles.find(r => r.role === role);
    bindings = [...bindings, {
      hardware: bus,
      driver: roleDef ? roleDef.driver : 'ds18b20',
      role: role,
      module: 'equipment',
      address: address,
    }];
  }

  // ── Derived state ──
  $: assignedRoles = new Set(bindings.map(b => b.role));
  $: assignedAddresses = new Set(bindings.filter(b => b.address).map(b => b.address));
  $: requiredRoles = roles.filter(r => !r.optional);
  $: missingRequired = requiredRoles.filter(r => !assignedRoles.has(r.role));
  $: canSave = missingRequired.length === 0 && !saving;

  $: assignedSensors = roles.filter(r => r.type === 'sensor' && assignedRoles.has(r.role));
  $: assignedActuators = roles.filter(r => r.type === 'actuator' && assignedRoles.has(r.role));

  $: owBuses = hwInventory.filter(h => h.hw_type === 'onewire_bus');
  $: freeAddrRoles = roles.filter(r => r.requires_address && !assignedRoles.has(r.role));
  $: unassignedOptional = roles
    .filter(r => r.optional && !assignedRoles.has(r.role))
    .filter(r => availableHw(r).length > 0);

  // ── Save / Restart ──
  async function save() {
    saving = true;
    error = null;
    try {
      const res = await apiPost('/api/bindings', {
        manifest_version: 1,
        bindings: bindings,
      });
      if (res.needs_restart) needsRestart = true;
    } catch (e) {
      error = e.message;
    } finally {
      saving = false;
    }
  }

  async function restart() {
    try { await apiPost('/api/restart', {}); } catch (_) {}
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
    <EquipmentStatus sensors={assignedSensors} actuators={assignedActuators} />
  {/if}

  <!-- Sensors card -->
  {#if assignedSensors.length > 0}
    <Card title={$t['bind.sensors']}>
      {#each assignedSensors as roleDef}
        {@const binding = getBinding(roleDef.role)}
        {#if binding}
          <BindingCard {roleDef} {binding}
            hwList={compatibleHw(roleDef)}
            usedIds={usedHwIds(roleDef.role)}
            on:changeHw={e => setHardware(e.detail.role, e.detail.hw)}
            on:changeAddr={e => setAddress(e.detail.role, e.detail.addr)}
            on:remove={e => removeRole(e.detail)} />
        {/if}
      {/each}
    </Card>
  {/if}

  <!-- Actuators card -->
  {#if assignedActuators.length > 0}
    <Card title={$t['bind.actuators']}>
      {#each assignedActuators as roleDef}
        {@const binding = getBinding(roleDef.role)}
        {#if binding}
          <BindingCard {roleDef} {binding}
            hwList={compatibleHw(roleDef)}
            usedIds={usedHwIds(roleDef.role)}
            on:changeHw={e => setHardware(e.detail.role, e.detail.hw)}
            on:remove={e => removeRole(e.detail)} />
        {/if}
      {/each}
    </Card>
  {/if}

  <!-- OneWire Discovery -->
  {#if owBuses.length > 0}
    <OneWireDiscovery {owBuses} {assignedAddresses} {freeAddrRoles}
      on:assign={handleAssign} />
  {/if}

  <!-- Add optional roles -->
  {#if unassignedOptional.length > 0}
    <Card title={$t['bind.add_equip']}>
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
</style>
