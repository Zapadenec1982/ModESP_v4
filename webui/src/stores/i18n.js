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

/**
 * UI text translations: Ukrainian (from ui.json) → English.
 * Used to translate page titles, card titles, widget descriptions,
 * select option labels, indicator labels, button labels, confirm messages.
 */
export const uiEn = {
  // Page titles
  'Dashboard': 'Dashboard',
  'Холодильна камера': 'Refrigeration',
  'Розморозка': 'Defrost',
  'Захист': 'Protection',
  'Графік': 'Chart',
  'Датчики': 'Sensors',
  'Обладнання': 'Equipment',
  'Мережа': 'Network',
  'Система': 'System',

  // Card titles
  'Температура': 'Temperature',
  'Стан системи': 'System Status',
  'Захист компресора': 'Compressor Protection',
  'Вентиляція': 'Ventilation',
  'Нічний режим': 'Night Mode',
  'Стан': 'Status',
  'Налаштування': 'Settings',
  'Гарячий газ': 'Hot Gas',
  'Стан аварій': 'Alarm Status',
  'Фільтр': 'Filter',
  'Статистика': 'Statistics',
  'Канали логування': 'Logging Channels',
  'Налаштування WiFi': 'WiFi Settings',
  'Точка доступу': 'Access Point',
  'Налаштування MQTT': 'MQTT Settings',
  'Інформація': 'Information',
  'Прошивка': 'Firmware',
  'Оновлення прошивки': 'Firmware Update',
  'Налаштування часу': 'Time Settings',
  'Бекап налаштувань': 'Settings Backup',
  'Дії': 'Actions',

  // Equipment / Sensors descriptions
  'Температура камери': 'Chamber temperature',
  'Температура випарника': 'Evaporator temperature',
  'Датчик камери справний': 'Chamber sensor OK',
  'Датчик випарника справний': 'Evaporator sensor OK',
  'Фактичний стан компресора': 'Compressor state',
  'Фактичний стан тена': 'Heater state',
  'Фактичний стан вент. випарника': 'Evaporator fan state',
  'Фактичний стан вент. конденсатора': 'Condenser fan state',
  'Фактичний стан клапана ГГ': 'HG valve state',
  'Температура конденсатора': 'Condenser temperature',
  'Стан дверей': 'Door state',
  'Дискретний вхід нічного режиму': 'Night mode input',
  'Вент. конденсатора доступний': 'Condenser fan available',
  'Контакт дверей доступний': 'Door contact available',
  'Датчик випарника доступний': 'Evaporator sensor available',
  'Датчик конденсатора доступний': 'Condenser sensor available',
  'Цифровий фільтр': 'Digital filter',

  // Thermostat descriptions
  'Поточна температура камери': 'Current chamber temperature',
  'Уставка температури': 'Temperature setpoint',
  'Диференціал (вгору від уставки)': 'Differential (above setpoint)',
  'Запит на компресор (до Equipment Manager)': 'Compressor request',
  'Запит на вент. випарника (до Equipment Manager)': 'Evap fan request',
  'Запит на вент. конденсатора (до Equipment Manager)': 'Cond fan request',
  'Стан модуля': 'Module state',
  'Час роботи компресора': 'Compressor on time',
  'Час простою компресора': 'Compressor off time',
  'Мін. час паузи компресора': 'Min compressor off time',
  'Мін. час роботи компресора': 'Min compressor on time',
  'Затримка після подачі живлення': 'Power-on delay',
  'Режим вент. випарника': 'Evaporator fan mode',
  'T зупинки вент. випарника (для режиму 2)': 'Fan stop temp (mode 2)',
  'Гістерезис T зупинки вент. (для режиму 2)': 'Fan stop hysteresis (mode 2)',
  'Затримка вимк. вент. конденсатора': 'Condenser fan off delay',
  'Час роботи в Safety Run': 'Safety Run on time',
  'Час паузи в Safety Run': 'Safety Run off time',
  'Зміщення уставки вночі': 'Night setback offset',
  'Активація нічного режиму': 'Night mode activation',
  'Година початку нічного режиму': 'Night mode start hour',
  'Година закінчення нічного режиму': 'Night mode end hour',
  'Нічний режим активний': 'Night mode active',
  'Ефективна уставка (з урахуванням нічного зміщення)': 'Effective setpoint',
  'Температура для відображення (може бути заморожена під час відтайки)': 'Display temperature',
  'Що показувати на дисплеі під час відтайки': 'Display during defrost',

  // Thermostat labels
  'Час роботи': 'On time',
  'Час простою': 'Off time',

  // Thermostat select options
  'Постійно': 'Always on',
  'З компресором': 'With compressor',
  'За температурою випарника': 'By evaporator temp',
  'Вимкнено': 'Disabled',
  'За розкладом': 'By schedule',
  'Через DI': 'Via DI',
  'Вручну': 'Manual',
  'Реальна T': 'Real temperature',
  'Заморожена T': 'Frozen temperature',
  'Показати -d-': 'Show -d-',

  // Thermostat indicator labels
  'Працює': 'Running',

  // Defrost descriptions
  'Головний сигнал: відтайка активна': 'Defrost active signal',
  'Поточна фаза відтайки': 'Current defrost phase',
  'Людино-читабельний стан для UI': 'Defrost status',
  'Тривалість відтайки': 'Defrost duration',
  'Час до наступної розморозки': 'Time to next defrost',
  'Кількість виконаних відтайок (скидається при ребуті)': 'Defrost count',
  'Причина останнього завершення (temp/timeout)': 'Last termination reason',
  'Послідовні завершення по таймеру': 'Consecutive timeouts',
  'Запит на тен (до Equipment Manager)': 'Heater request',
  'Запит на клапан ГГ (до Equipment Manager)': 'HG valve request',
  'Ручний запуск розморозки (trigger)': 'Manual defrost start',
  'Ручна зупинка розморозки (trigger)': 'Manual defrost stop',
  'Тип відтайки': 'Defrost type',
  'Інтервал між відтайками': 'Defrost interval',
  'Режим лічильника інтервалу': 'Interval counter mode',
  'Ініціація розморозки': 'Defrost initiation',
  'T завершення відтайки': 'Defrost end temperature',
  'Макс. тривалість активної фази': 'Max active phase duration',
  'Поріг запуску відтайки за температурою': 'Defrost demand temperature',
  'Час дренажу': 'Drip time',
  'Затримка вент. після відтайки': 'Fan delay after defrost',
  'T завершення охолодження після відтайки': 'FAD end temperature',
  'Час стабілізації тиску (гарячий газ)': 'Pressure stabilization time',
  'Затримка клапана ГГ (гарячий газ)': 'HG valve delay',
  'Час вирівнювання тиску (гарячий газ)': 'Pressure equalization time',

  // Defrost labels
  'Фаза': 'Phase',
  'Відтайок': 'Defrosts',

  // Defrost select options
  'За часом (зупинка компресора)': 'Timer (compressor stop)',
  'Електрична (тен)': 'Electric (heater)',
  'Реальний час': 'Real time',
  'За таймером': 'By timer',
  'Комбінований': 'Combined',

  // Protection descriptions
  'Аварійна зупинка (зарезервовано, завжди false)': 'Emergency stop (reserved)',
  'Є хоча б одна активна аварія': 'At least one alarm active',
  'Код найвищої аварії (high_temp, low_temp, err1, err2, door, none)': 'Active alarm code',
  'Аварія верхньої температури': 'High temperature alarm',
  'Аварія нижньої температури': 'Low temperature alarm',
  'Обрив датчика камери': 'Chamber sensor failure',
  'Обрив датчика випарника': 'Evaporator sensor failure',
  'Двері відкриті занадто довго': 'Door open too long',
  'Верхня межа температури': 'High temperature limit',
  'Нижня межа температури': 'Low temperature limit',
  'Затримка аварії високої T': 'High temp alarm delay',
  'Затримка аварії низької T': 'Low temp alarm delay',
  'Затримка аварії відкритих дверей': 'Door alarm delay',
  'Ручне скидання аварій (false=auto, true=manual)': 'Manual alarm reset',
  'Затримка аварії високої T після відтайки': 'Post-defrost alarm delay',
  'Команда скидання аварій (write-only trigger)': 'Reset alarms',

  // Protection indicator labels
  'Аварія': 'Alarm',
  'АВАРІЯ': 'ALARM',
  'Норма': 'Normal',
  'Висока T': 'High T',
  'Низька T': 'Low T',
  'Датчик камери': 'Chamber sensor',
  'Датчик випарника': 'Evaporator sensor',
  'Двері': 'Door',
  'Код аварії': 'Alarm code',
  'Скинути аварії': 'Reset Alarms',

  // DataLogger descriptions
  'Логування увімкнено': 'Logging enabled',
  'Глибина зберігання (год)': 'Retention (hours)',
  'Інтервал семплювання (с)': 'Sample interval (s)',
  'Записів температури': 'Temperature records',
  'Подій': 'Events',
  'Логувати T випарника': 'Log evaporator temp',
  'Логувати T конденсатора': 'Log condenser temp',
  'Логувати уставку': 'Log setpoint',
  'Логувати вологість': 'Log humidity',
  'Графік температури': 'Temperature chart',
  'Записів': 'Records',

  // Network descriptions
  'IP адреса': 'IP Address',
  'Сигнал': 'Signal',
  'Пароль': 'Password',
  'Зберегти': 'Save',
  'SSID точки доступу': 'AP SSID',
  'Пароль (мін. 8 символів або порожній)': 'Password (min 8 chars or empty)',
  'Канал': 'Channel',
  'Зберегти AP': 'Save AP',
  'Підключення': 'Connection',
  'Підключено': 'Connected',
  'Відключено': 'Disconnected',
  'Адреса брокера': 'Broker address',
  'Порт': 'Port',
  'Логін': 'Login',
  'Префікс топіків': 'Topic prefix',
  'Увімкнути MQTT': 'Enable MQTT',

  // System descriptions
  'Версія': 'Version',
  'Дата збірки': 'Build date',
  'Причина завантаження': 'Boot reason',
  'Вільна RAM': 'Free RAM',
  'Мінімум RAM': 'Minimum RAM',
  'Вибрати .bin файл': 'Select .bin file',
  'Завантажити нову прошивку': 'Upload new firmware',
  'NTP синхронізація': 'NTP synchronization',
  'Часовий пояс': 'Timezone',
  'Встановити вручну': 'Set manually',
  'Завантажити бекап': 'Download backup',
  'Відновити з бекапу': 'Restore from backup',
  'Перезавантажити': 'Restart',
  'Скинути до заводських': 'Factory Reset',

  // Confirm messages
  'Перезавантажити пристрій?': 'Restart device?',
  'Всі налаштування будуть скинуті до заводських значень. Продовжити?': 'All settings will be reset. Continue?',
  'Відновити налаштування з файлу? Пристрій перезавантажиться.': 'Restore from file? Device will restart.',

  // Equipment bindings
  'Призначене обладнання': 'Assigned Equipment',
  'Вільне обладнання': 'Free Equipment',
  'Немає налаштованих підключень': 'No configured connections',
  'не призначено': 'not assigned',

  // Equipment role labels
  'Темп. камери': 'Chamber temp',
  'Компресор': 'Compressor',
  'Темп. випарника': 'Evaporator temp',
  'Темп. конденсатора': 'Condenser temp',
  'Тен відтайки': 'Defrost heater',
  'Вент. випарника': 'Evaporator fan',
  'Вент. конденсатора': 'Condenser fan',
  'Клапан гарячого газу': 'Hot gas valve',
  'Контакт дверей': 'Door contact',
  'Вхід нічного режиму': 'Night mode input',

  // Hardware labels
  'Реле 1': 'Relay 1',
  'Реле 2': 'Relay 2',
  'Реле 3': 'Relay 3',
  'Реле 4': 'Relay 4',
  'Цифровий вхід 1': 'Digital input 1',
  'OneWire шина 1': 'OneWire bus 1',
  'ADC 1': 'ADC 1',
  'ADC 2': 'ADC 2',

  // DataLogger labels
  'Flash (KB)': 'Flash (KB)',
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
