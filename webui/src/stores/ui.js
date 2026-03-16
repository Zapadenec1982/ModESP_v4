import { writable, derived, get } from 'svelte/store';
import { apiGet } from '../lib/api.js';
import { language } from './i18n.js';

/** Full UI config from /api/ui */
export const uiConfig = writable(null);

/** Raw pages from server (always Ukrainian) */
const rawPages = derived(uiConfig, $ui => $ui?.pages || []);

// ── Language pack (lazy-loaded from /i18n/{lang}.json) ──────────

/** Currently loaded language pack (null = Ukrainian default) */
const langPack = writable(null);

/** Reverse lookup: Ukrainian text → English translation (built from pack) */
let reverseLookup = {};

/** Load a language pack from ESP32 LittleFS */
export async function loadLanguagePack(lang) {
  if (lang === 'uk') {
    langPack.set(null);
    return;
  }
  try {
    const resp = await fetch(`/i18n/${lang}.json`);
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    const pack = await resp.json();
    // Build reverse lookup: scan all strings values and map UA originals
    // For card/page titles that don't have i18n_key, we need UA→EN mapping
    reverseLookup = {};
    const strings = pack.strings || {};
    for (const [key, val] of Object.entries(strings)) {
      // sys.{UA} = EN → map UA → EN
      if (key.startsWith('sys.')) {
        reverseLookup[key.slice(4)] = val;
      }
    }
    // Also build from structured keys: find UA text in rawPages and map
    const raw = get(uiConfig)?.pages || [];
    for (const page of raw) {
      // Page title
      if (page.title && strings[`page.${page.id}.title`]) {
        reverseLookup[page.title] = strings[`page.${page.id}.title`];
      }
      for (const card of (page.cards || [])) {
        const cardId = card.id || '';
        const modId = page.id || page.page_id || '';
        // Try card.{module}.{cardId}.title
        for (const field of ['title', 'subtitle']) {
          if (!card[field]) continue;
          const candidates = [
            `card.${modId}.${cardId}.${field}`,
            `card.${page.page_id || modId}.${cardId}.${field}`,
          ];
          for (const ck of candidates) {
            if (strings[ck]) { reverseLookup[card[field]] = strings[ck]; break; }
          }
        }
      }
    }
    langPack.set(pack);
  } catch (e) {
    console.warn(`Failed to load language pack '${lang}':`, e.message);
    langPack.set(null);  // fallback to Ukrainian
  }
}

// Auto-load language pack when language changes
language.subscribe(lang => {
  loadLanguagePack(lang);
});

// ── Translation helpers ─────────────────────────────────────────

/**
 * Translate a widget field using language pack.
 * Uses structured key (i18n_key + ".field") if available,
 * falls back to UA string lookup (backward compat with uiEn-style keys).
 */
function tr(text, i18nKey, field, pack) {
  if (!pack || !text) return text;
  const strings = pack.strings || {};
  // Try structured key first: state.thermostat.setpoint.description
  if (i18nKey) {
    const fullKey = `${i18nKey}.${field}`;
    if (strings[fullKey]) return strings[fullKey];
  }
  // Fallback: reverse lookup (UA text → EN from pre-built map)
  if (reverseLookup[text]) return reverseLookup[text];
  // Last resort: try UA text directly as key
  return strings[text] || text;
}

function trOpts(options, i18nKey, pack) {
  if (!options || !pack) return options;
  return options.map(o => {
    const optKey = i18nKey ? `${i18nKey}.options.${o.value}` : null;
    const hintKey = i18nKey ? `${i18nKey}.options.${o.value}.disabled_hint` : null;
    return {
      ...o,
      label: tr(o.label, optKey ? i18nKey : null, `options.${o.value}`, pack),
      disabled_hint: tr(o.disabled_hint, hintKey ? i18nKey : null, `options.${o.value}.disabled_hint`, pack),
    };
  });
}

// ── Translated pages store ──────────────────────────────────────

/** Pages array — auto-translated based on current language */
export const pages = derived([rawPages, language, langPack], ([$raw, $lang, $pack]) => {
  if ($lang === 'uk' || !$pack) return $raw;
  return $raw.map(page => ({
    ...page,
    title: tr(page.title, null, null, $pack),
    cards: (page.cards || []).map(card => ({
      ...card,
      title: tr(card.title, null, null, $pack),
      subtitle: tr(card.subtitle, null, null, $pack),
      widgets: (card.widgets || []).map(w => {
        const ik = w.i18n_key || null;
        return {
          ...w,
          description: tr(w.description, ik, 'description', $pack),
          label: tr(w.label, ik, 'label', $pack),
          unit: tr(w.unit, ik, 'unit', $pack),
          on_label: tr(w.on_label, ik, 'on_label', $pack),
          off_label: tr(w.off_label, ik, 'off_label', $pack),
          confirm: tr(w.confirm, ik, 'confirm', $pack),
          disabled_hint: tr(w.disabled_hint, ik, 'disabled_hint', $pack),
          disabled_reason: tr(w.disabled_reason, null, null, $pack),
          options: trOpts(w.options, ik, $pack),
          actions: w.actions ? w.actions.map(a => ({
            ...a,
            label: tr(a.label, null, null, $pack),
            confirm: tr(a.confirm, null, null, $pack),
          })) : w.actions,
        };
      })
    })),
    roles: (page.roles || []).map(r => ({ ...r, label: tr(r.label, null, null, $pack) })),
    hardware: (page.hardware || []).map(h => ({ ...h, label: tr(h.label, null, null, $pack) })),
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
