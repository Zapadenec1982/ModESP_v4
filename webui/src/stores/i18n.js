import { writable, derived } from 'svelte/store';

const KEY = 'modesp-lang';

const uk = {
  // App
  'app.loading': 'Завантаження...',
  'app.error': 'Помилка з\'єднання',
  'app.retry': 'Повторити',

  // Layout
  'status.online': 'Онлайн',
  'status.offline': 'Офлайн',
  'alarm.banner': 'ТРИВОГА',

  // Dashboard
  'dash.setpoint': 'УСТАВКА',
  'dash.compressor': 'Компресор',
  'dash.fan': 'Вентилятор',
  'dash.heater': 'Нагрівач',
  'dash.door': 'Двері',
  'dash.condenser': 'Конденсатор',
  'state.idle': 'ОЧІКУВАННЯ',
  'state.cooling': 'ОХОЛОДЖЕННЯ',
  'state.safety_run': 'АВАРІЙНИЙ РЕЖИМ',
  'state.startup': 'ЗАПУСК',
  'state.defrost': 'РОЗМОРОЗКА',
  'state.night': 'НІЧ',
  'state.alarm': 'ТРИВОГА',

  // Defrost phases
  'defrost.idle': 'Очікування',
  'defrost.stabilize': 'Стабілізація',
  'defrost.valve_open': 'Клапан відкрито',
  'defrost.active': 'Активна',
  'defrost.equalize': 'Вирівнювання',
  'defrost.drip': 'Стікання',
  'defrost.fad': 'Охолодження',

  // Chart
  'chart.loading': 'Завантаження...',
  'chart.no_data': 'Немає даних',
  'chart.events': 'Події',
  'chart.ch_air': 'Камера',
  'chart.ch_evap': 'Випарник',
  'chart.ch_cond': 'Конденсатор',
  'chart.ch_setpoint': 'Уставка',
  'chart.ch_humidity': 'Вологість',
  'chart.legend_comp': 'Комп.',
  'chart.legend_defrost': 'Дефрост',
  'chart.legend_setpoint': 'Уставка',
  'event.1': 'Компресор ON',
  'event.2': 'Компресор OFF',
  'event.3': 'Дефрост старт',
  'event.4': 'Дефрост кінець',
  'event.5': 'Аварія: висока T',
  'event.6': 'Аварія: низька T',
  'event.7': 'Аварія знята',
  'event.8': 'Двері відкрито',
  'event.9': 'Двері зачинено',
  'event.10': 'Увімкнення',

  // Bindings
  'bind.status': 'Стан обладнання',
  'bind.loading': 'Завантаження...',
  'bind.saved': 'Збережено. Потрібен перезапуск.',
  'bind.restart': 'Перезапустити',
  'bind.required': 'Необхідно',
  'bind.optional': 'Опціонально',
  'bind.save': 'Зберегти',
  'bind.saving': 'Збереження...',
  'bind.scan': 'Сканувати шину',
  'bind.scanning': 'Сканування...',
  'bind.scan_hint': 'Натисніть "Сканувати шину" для пошуку',
  'bind.all_assigned': 'Всі пристрої вже призначені',
  'bind.unassigned': 'Не призначено',
  'bind.add': 'Додати',
  'page.not_found': 'Сторінку не знайдено',

  // Alerts
  'alert.saved': 'Збережено!',
  'alert.saved_restart': 'Збережено! Перезавантажте.',
  'alert.saved_mqtt': 'Збережено! MQTT перепідключається...',
  'alert.error': 'Помилка збереження',
  'alert.ssid_empty': 'SSID не може бути порожнім',
  'alert.pass_min8': 'Пароль мінімум 8 символів',
  'alert.confirm_ota': 'Оновити прошивку? Пристрій перезавантажиться.',
  'alert.only_bin': 'Тільки .bin файли',

  // OTA
  'ota.uploading': 'Завантаження...',
  'ota.done': 'Готово! Перезапуск...',
  'ota.upload': 'Оновити прошивку',

  // WiFi
  'wifi.scan': 'Сканувати',
  'wifi.scanning': 'Сканування...',

  // Defrost toggle
  'defrost.toggle': 'Розморозка',

  // Timezone
  'tz.label': 'Часовий пояс',

  // Password
  'pass.show': 'Показати',
  'pass.hide': 'Сховати',

  // Buttons
  'btn.action': 'Дія',
  'btn.save': 'Зберегти',
  'btn.save_ap': 'Зберегти AP',
  'btn.error': 'Помилка',
  'btn.remove': 'Видалити',

  // Bindings page
  'bind.sensors': 'Сенсори',
  'bind.actuators': 'Актуатори',
  'bind.onewire': 'Виявлення OneWire',
  'bind.add_equip': 'Додати обладнання',
  'bind.used': 'зайнято',
  'bind.role': '-- Роль --',
  'bind.found': 'Знайдено {0} пристроїв, {1} вже призначено',
  'bind.new_device': 'новий',

  // Chart
  'chart.title': 'Температура',
};

const en = {
  'app.loading': 'Loading...',
  'app.error': 'Connection Error',
  'app.retry': 'Retry',

  'status.online': 'Online',
  'status.offline': 'Offline',
  'alarm.banner': 'ALARM',

  'dash.setpoint': 'SETPOINT',
  'dash.compressor': 'Compressor',
  'dash.fan': 'Fan',
  'dash.heater': 'Heater',
  'dash.door': 'Door',
  'dash.condenser': 'Condenser',
  'state.idle': 'IDLE',
  'state.cooling': 'COOLING',
  'state.safety_run': 'SAFE MODE',
  'state.startup': 'STARTUP',
  'state.defrost': 'DEFROST',
  'state.night': 'NIGHT',
  'state.alarm': 'ALARM',

  'defrost.idle': 'Idle',
  'defrost.stabilize': 'Stabilize',
  'defrost.valve_open': 'Valve open',
  'defrost.active': 'Active',
  'defrost.equalize': 'Equalize',
  'defrost.drip': 'Drip',
  'defrost.fad': 'Fan cooling',

  'chart.loading': 'Loading...',
  'chart.no_data': 'No data',
  'chart.events': 'Events',
  'chart.ch_air': 'Chamber',
  'chart.ch_evap': 'Evaporator',
  'chart.ch_cond': 'Condenser',
  'chart.ch_setpoint': 'Setpoint',
  'chart.ch_humidity': 'Humidity',
  'chart.legend_comp': 'Comp.',
  'chart.legend_defrost': 'Defrost',
  'chart.legend_setpoint': 'Setpoint',
  'event.1': 'Compressor ON',
  'event.2': 'Compressor OFF',
  'event.3': 'Defrost start',
  'event.4': 'Defrost end',
  'event.5': 'Alarm: High T',
  'event.6': 'Alarm: Low T',
  'event.7': 'Alarm cleared',
  'event.8': 'Door opened',
  'event.9': 'Door closed',
  'event.10': 'Power on',

  'bind.status': 'Equipment Status',
  'bind.loading': 'Loading...',
  'bind.saved': 'Saved. Restart required.',
  'bind.restart': 'Restart',
  'bind.required': 'Required',
  'bind.optional': 'Optional',
  'bind.save': 'Save',
  'bind.saving': 'Saving...',
  'bind.scan': 'Scan Bus',
  'bind.scanning': 'Scanning...',
  'bind.scan_hint': 'Press "Scan Bus" to discover sensors',
  'bind.all_assigned': 'All devices already assigned',
  'bind.unassigned': 'Unassigned',
  'bind.add': 'Add',
  'page.not_found': 'Page not found',

  'alert.saved': 'Saved!',
  'alert.saved_restart': 'Saved! Restart to apply.',
  'alert.saved_mqtt': 'Saved! MQTT reconnecting...',
  'alert.error': 'Save error',
  'alert.ssid_empty': 'SSID cannot be empty',
  'alert.pass_min8': 'Password must be at least 8 characters',
  'alert.confirm_ota': 'Update firmware? Device will restart.',
  'alert.only_bin': 'Only .bin files',

  'ota.uploading': 'Uploading...',
  'ota.done': 'Done! Restarting...',
  'ota.upload': 'Upload firmware',

  'wifi.scan': 'Scan',
  'wifi.scanning': 'Scanning...',

  'defrost.toggle': 'Defrost',

  'tz.label': 'Timezone',

  // Password
  'pass.show': 'Show',
  'pass.hide': 'Hide',

  // Buttons
  'btn.action': 'Action',
  'btn.save': 'Save',
  'btn.save_ap': 'Save AP',
  'btn.error': 'Error',
  'btn.remove': 'Remove',

  // Bindings page
  'bind.sensors': 'Sensors',
  'bind.actuators': 'Actuators',
  'bind.onewire': 'OneWire Discovery',
  'bind.add_equip': 'Add Equipment',
  'bind.used': 'used',
  'bind.role': '-- Role --',
  'bind.found': 'Found {0} device(s), {1} already assigned',
  'bind.new_device': 'new',

  // Chart
  'chart.title': 'Temperature',
};

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
