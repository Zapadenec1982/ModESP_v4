import { writable, derived } from 'svelte/store';
import uk from '../i18n/uk.js';
import en from '../i18n/en.js';

export { default as uiEn } from '../i18n/uiEn.js';

const KEY = 'modesp-lang';
const dicts = { uk, en };

function getInitial() {
  try {
    const s = localStorage.getItem(KEY);
    if (s === 'uk' || s === 'en') return s;
  } catch (e) {}
  const nav = navigator.language || '';
  if (nav.startsWith('uk') || nav.startsWith('ru')) return 'uk';
  return 'en';
}

export const language = writable(getInitial());

language.subscribe(v => {
  document.documentElement.lang = v;
  try { localStorage.setItem(KEY, v); } catch (e) {}
});

export const t = derived(language, $lang => dicts[$lang] || dicts.uk);

export function toggleLanguage() {
  language.update(l => l === 'uk' ? 'en' : 'uk');
}
