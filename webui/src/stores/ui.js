import { writable, derived } from 'svelte/store';
import { apiGet } from '../lib/api.js';

/** Full UI config from /api/ui */
export const uiConfig = writable(null);

/** Pages array for navigation */
export const pages = derived(uiConfig, $ui => $ui?.pages || []);

/** State metadata for validation/display */
export const stateMeta = derived(uiConfig, $ui => $ui?.state_meta || {});

/** Device name */
export const deviceName = derived(uiConfig, $ui => $ui?.device_name || 'ModESP');

/** Loading state */
export const uiLoading = writable(true);

/** Error state */
export const uiError = writable(null);

export async function loadUiConfig() {
  try {
    uiLoading.set(true);
    uiError.set(null);
    const data = await apiGet('/api/ui');
    uiConfig.set(data);
  } catch (e) {
    uiError.set(e.message);
    console.error('Failed to load UI config', e);
  } finally {
    uiLoading.set(false);
  }
}
