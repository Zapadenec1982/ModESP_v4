export default {
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
  'dash.valve': 'Клапан ГГ',
  'dash.door': 'Двері',
  'dash.condenser': 'Конденсатор',
  'dash.comp': 'КОМП',
  'dash.evap_fan': 'ВЕНТ',
  'dash.cond_fan': 'КОНД',
  'dash.defrost_pill': 'ВІДТ',
  'dash.door_open': 'ДВЕРІ',
  'dash.evap_temp': 'Випарник',
  'dash.cond_temp': 'Конденсатор',
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

  // Connection
  'conn.lost': 'З\'єднання втрачено. Перепідключення...',
  'conn.retry': 'Перепідключити',
  'conn.restored': 'З\'єднано',

  // Alerts
  'alert.saved': 'Збережено!',
  'alert.saved_restart': 'Збережено! Перезавантажте.',
  'alert.saved_mqtt': 'Збережено! MQTT перепідключується...',
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
  'bind.choose_hw': 'Оберіть обладнання',
  'bind.role': '-- Роль --',
  'bind.found': 'Знайдено {0} пристроїв, {1} вже призначено',
  'bind.found_total': 'Знайдено на шині:',
  'bind.new_device': 'новий',
  'bind.unbind': 'Відкріпити',
  'bind.no_free_roles': 'Немає вільних ролей',
  'bind.confirm_missing': 'Відсутні обов\'язкові ролі',
  'bind.confirm_alarm': 'Система запуститься в аварійному режимі. Продовжити?',
  'bind.pick': 'Обрати',
  'bind.in_use': 'зайнято',
  'bind.selected': 'обрано',
  'bind.no_devices': 'Пристроїв не знайдено',

  // Equipment / driver settings
  'eq.filter': 'Цифровий фільтр',
  'eq.offset': 'Корекція °C',
  'eq.ntc_beta': 'B-коефіцієнт',
  'eq.ntc_series': 'Послідовний резистор',
  'eq.ntc_nominal': 'Номінальний опір (25°C)',

  // Chart
  'chart.title': 'Температура',
  'chart.min_ago': 'хв тому',
  'chart.detail': 'Детальний графік',
};
