import { writable } from 'svelte/store';

export const toasts = writable([]);

let nextId = 0;

function addToast(msg, type, duration) {
  const id = ++nextId;
  toasts.update(t => [...t, { id, msg, type }]);
  setTimeout(() => {
    toasts.update(t => t.filter(x => x.id !== id));
  }, duration);
}

export function toastSuccess(msg) { addToast(msg, 'success', 3000); }
export function toastError(msg) { addToast(msg, 'error', 5000); }
export function toastWarn(msg) { addToast(msg, 'warn', 4000); }
