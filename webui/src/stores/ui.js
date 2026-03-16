import { writable, derived } from 'svelte/store';
import { apiGet } from '../lib/api.js';
import { language, uiEn } from './i18n.js';

/** Full UI config from /api/ui */
export const uiConfig = writable(null);

/** Raw pages from server (always Ukrainian) */
const rawPages = derived(uiConfig, $ui => $ui?.pages || []);

/** Translate Ukrainian text to English using uiEn map */
function tr(text) {
  return (text && uiEn[text]) || text;
}

/** Translate widget options labels */
function trOpts(options) {
  if (!options) return options;
  return options.map(o => ({ ...o, label: tr(o.label), disabled_hint: tr(o.disabled_hint) }));
}

/** Pages array — auto-translated based on current language */
export const pages = derived([rawPages, language], ([$raw, $lang]) => {
  if ($lang === 'uk') return $raw;
  return $raw.map(page => ({
    ...page,
    title: tr(page.title),
    cards: (page.cards || []).map(card => ({
      ...card,
      title: tr(card.title),
      subtitle: tr(card.subtitle),
      widgets: (card.widgets || []).map(w => ({
        ...w,
        description: tr(w.description),
        label: tr(w.label),
        unit: tr(w.unit),
        on_label: tr(w.on_label),
        off_label: tr(w.off_label),
        confirm: tr(w.confirm),
        disabled_hint: tr(w.disabled_hint),
        disabled_reason: tr(w.disabled_reason),
        options: trOpts(w.options),
        actions: w.actions ? w.actions.map(a => ({ ...a, label: tr(a.label), confirm: tr(a.confirm) })) : w.actions,
      }))
    })),
    roles: (page.roles || []).map(r => ({ ...r, label: tr(r.label) })),
    hardware: (page.hardware || []).map(h => ({ ...h, label: tr(h.label) })),
  }));
});

/** State metadata for validation/display */
export const stateMeta = derived(uiConfig, $ui => $ui?.state_meta || {});

/** Device name */
export const deviceName = derived(uiConfig, $ui => $ui?.device_name || 'ModESP');

/** Navigation request (set page id to navigate) */
export const navigateTo = writable(null);

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
