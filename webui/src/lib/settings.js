import { writable, get } from 'svelte/store';
import { apiPost } from './api.js';
import { state, setStateKey } from '../stores/state.js';
import { toastError } from '../stores/toast.js';

/**
 * Debounced setting sender with optimistic update + visual feedback.
 * @param {string} key - State key (e.g. 'thermostat.setpoint')
 * @param {object} opts - { debounceMs, endpoint }
 * @returns {{ send, pending, flashOk, cleanup }}
 */
export function createSettingSender(key, { debounceMs = 500, endpoint = '/api/settings' } = {}) {
  let timer;
  let flashTimer;
  let prevValue;          // серверне значення до оптимістичного burst (для rollback)
  let bursting = false;
  const pending = writable(false);
  const flashOk = writable(false);

  function send(value) {
    clearTimeout(timer);
    // Захоплюємо справжнє попереднє значення один раз на debounce-burst
    if (!bursting) {
      prevValue = get(state)[key];
      bursting = true;
    }
    setStateKey(key, value);
    pending.set(true);

    const doPost = async () => {
      try {
        await apiPost(endpoint, { [key]: value });
        flashOk.set(true);
        clearTimeout(flashTimer);
        flashTimer = setTimeout(() => flashOk.set(false), 400);
      } catch (e) {
        // Rollback оптимістичного оновлення (H2-fix): інакше при delta-publish
        // пристрій не перешле незмінений ключ і UI лишиться з відхиленим значенням.
        setStateKey(key, prevValue);
        toastError(e.message);
      } finally {
        pending.set(false);
        bursting = false;
      }
    };

    if (debounceMs > 0) {
      timer = setTimeout(doPost, debounceMs);
    } else {
      doPost();
    }
  }

  function cleanup() {
    clearTimeout(timer);
    clearTimeout(flashTimer);
  }

  return { send, pending, flashOk, cleanup };
}
