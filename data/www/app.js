'use strict';
let UI = null;
const state = {};
let currentPage = null;
let ws = null;
let wsRetry = 1000;

function setState(newState) {
  Object.assign(state, newState);
  renderValues();
}

function connectWs() {
  if (ws && ws.readyState < 2) return;
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = () => { wsRetry = 1000; updateStatus(true); };
  ws.onmessage = (e) => {
    try { setState(JSON.parse(e.data)); }
    catch(err) { console.warn('WS parse error', err); }
  };
  ws.onclose = () => {
    updateStatus(false);
    setTimeout(connectWs, wsRetry);
    wsRetry = Math.min(wsRetry * 2, 30000);
  };
}

function updateStatus(connected) {
  const el = document.getElementById('ws-status');
  if (!el) return;
  el.innerHTML = connected
    ? '<span class="status-dot ok"></span>Connected'
    : '<span class="status-dot err"></span>Disconnected';
}

async function apiGet(url) { const r = await fetch(url); return r.json(); }
async function apiPost(url, data) {
  const r = await fetch(url, {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  }); return r.json();
}

function gaugeArc(mn, mx, zones) {
  if (!zones || !zones.length) return '';
  const w = 180, h = 100, r = 80, cx = 90, cy = 95, sw = 10;
  let svg = '<svg viewBox="0 0 ' + w + ' ' + h + '" xmlns="http://www.w3.org/2000/svg">';
  svg += '<path d="M ' + (cx - r) + ' ' + cy + ' A ' + r + ' ' + r + ' 0 0 1 ' + (cx + r) + ' ' + cy + '" '
       + 'fill="none" stroke="var(--bg3)" stroke-width="' + sw + '" stroke-linecap="round"/>';
  const lo = mn ?? zones[0].to - 10, hi = mx ?? zones[zones.length-1].to + 10;
  const range = hi - lo || 1;
  let prev = 0;
  for (const z of zones) {
    const frac = Math.max(0, Math.min(1, (z.to - lo) / range));
    const a1 = Math.PI * (1 - prev), a2 = Math.PI * (1 - frac);
    const x1 = cx - r * Math.cos(a1), y1 = cy - r * Math.sin(a1);
    const x2 = cx - r * Math.cos(a2), y2 = cy - r * Math.sin(a2);
    const big = (prev - frac) > 0.5 ? 1 : 0;
    svg += '<path d="M ' + x1.toFixed(1) + ' ' + y1.toFixed(1)
         + ' A ' + r + ' ' + r + ' 0 ' + big + ' 0 '
         + x2.toFixed(1) + ' ' + y2.toFixed(1) + '" '
         + 'fill="none" stroke="' + z.color + '" stroke-width="' + sw + '" '
         + 'stroke-linecap="round" opacity="0.35"/>';
    prev = frac;
  }
  svg += '</svg>';
  return svg;
}

function numStep(id, mod, key, delta, min, max) {
  const cur = state[key] ?? 0;
  const nv = Math.round(Math.max(min, Math.min(max, cur + delta)) * 100) / 100;
  state[key] = nv;
  const el = document.getElementById(id);
  if (el) el.textContent = nv;
  sendSetting(mod, key, nv);
}

async function sendSetting(module, key, value) {
  try { await apiPost('/api/settings', {[key]: value}); }
  catch(e) { console.error('Setting failed', e); }
}

function renderNav() {
  if (!UI) return;
  const nav = document.getElementById('nav');
  nav.innerHTML = UI.pages.map(p =>
    '<button data-page="' + p.id + '"' +
    (p.id === currentPage ? ' class="active"' : '') +
    '>' + p.title + '</button>'
  ).join('');
  nav.querySelectorAll('button').forEach(btn => {
    btn.onclick = () => showPage(btn.dataset.page);
  });
}

function showPage(id) { currentPage = id; renderNav(); renderPage(); }

function renderPage() {
  const main = document.getElementById('main');
  if (!UI) { main.innerHTML = '<div class="loading">Завантаження...</div>'; return; }
  const page = UI.pages.find(p => p.id === currentPage);
  if (!page) { main.innerHTML = ''; return; }
  let html = '';
  for (const card of page.cards) {
    const coll = card.collapsible;
    html += '<div class="card">';
    html += '<div class="card-title' + (coll ? ' collapsible' : '') + '"'
         + (coll ? ' onclick="toggleCard(this)"' : '') + '>';
    if (coll) html += '<span class="arrow">\u25b6</span> ';
    html += card.title + '</div>';
    html += '<div class="card-body' + (coll ? ' collapsed' : '') + '">';
    for (const w of card.widgets) html += renderWidget(w);
    html += '</div></div>';
  }
  html += '<div class="status-bar"><span id="ws-status"></span></div>';
  main.innerHTML = html;
  renderValues();
  if (currentPage === 'firmware') loadOtaInfo();
  if (currentPage === 'network') loadMqttInfo();
  if (currentPage === 'system') loadTimeInfo();
}

function toggleCard(el) {
  const arrow = el.querySelector('.arrow');
  const body = el.nextElementSibling;
  body.classList.toggle('collapsed');
  arrow && arrow.classList.toggle('open');
}

function renderWidget(w) {
  const id = 'w-' + w.key.replace(/\./g, '-');
  const desc = w.description || w.key.split('.').pop();
  switch (w.widget) {
    case 'gauge': {
      const sz = w.size === 'large' ? ' large' : '';
      return '<div class="widget gauge" data-key="' + w.key + '">'
        + '<div class="gauge-ring' + sz + '">'
        + gaugeArc(w.min, w.max, w.color_zones)
        + '</div>'
        + '<div class="gauge-value' + sz
        + '" id="' + id + '" data-zones=\'' + JSON.stringify(w.color_zones || []) + '\''
        + ' data-min="' + (w.min ?? '') + '" data-max="' + (w.max ?? '') + '">—</div>'
        + '<div class="gauge-label">' + desc + (w.unit ? ' (' + w.unit + ')' : '') + '</div></div>';}
    case 'value':
      return '<div class="widget"><div class="widget-row">'
        + '<span class="widget-label">' + desc + '</span>'
        + '<span><span class="widget-value" id="' + id + '">—</span>'
        + (w.unit ? '<span class="widget-unit">' + w.unit + '</span>' : '')
        + '</span></div></div>';
    case 'indicator':
      return '<div class="widget"><div class="widget-row">'
        + '<span class="widget-label">' + desc + '</span>'
        + '<div class="indicator" id="' + id + '"'
        + ' data-on-label="' + (w.on_label || 'ON') + '"'
        + ' data-off-label="' + (w.off_label || 'OFF') + '"'
        + ' data-on-color="' + (w.on_color || 'var(--green)') + '"'
        + ' data-off-color="' + (w.off_color || 'var(--fg3)') + '">'
        + '<span class="indicator-dot"></span>'
        + '<span class="indicator-label">—</span>'
        + '</div></div></div>';
    case 'status_text':
      return '<div class="widget"><div class="widget-row">'
        + '<span class="widget-label">' + desc + '</span>'
        + '<span class="widget-value" id="' + id + '">—</span>'
        + '</div></div>';
    case 'slider': {
      const min = w.min ?? -35, max = w.max ?? 0, step = w.step ?? 0.5;
      const mod = w.key.split('.')[0];
      return '<div class="widget slider-wrap">'
        + '<div class="slider-header"><span class="widget-label">' + desc
        + (w.unit ? ' (' + w.unit + ')' : '') + '</span>'
        + '<span class="slider-val" id="' + id + '-val">—</span></div>'
        + '<input type="range" id="' + id + '"'
        + ' min="' + min + '" max="' + max + '" step="' + step + '"'
        + ' oninput="document.getElementById(\'' + id + '-val\').textContent=this.value"'
        + ' onchange="sendSetting(\'' + mod + '\',\'' + w.key + '\',parseFloat(this.value))">'
        + '</div>';}
    case 'number_input': {
      const min = w.min ?? 0, max = w.max ?? 100, step = w.step ?? 1;
      const inputId = id + '-input';
      return '<div class="widget">'
        + '<div class="widget-label" style="margin-bottom:6px">' + desc + '</div>'
        + '<input type="number" class="text-input" id="' + inputId + '"'
        + ' min="' + min + '" max="' + max + '" step="' + step + '"'
        + ' placeholder="' + desc + '">'
        + '</div>';}
    case 'text_input':
    case 'password_input': {
      const inputType = w.widget === 'password_input' ? 'password' : 'text';
      const inputId = id + '-input';
      return '<div class="widget">'
        + '<div class="widget-label" style="margin-bottom:6px">' + desc + '</div>'
        + '<input type="' + inputType + '" class="text-input" id="' + inputId + '"'
        + ' placeholder="' + desc + '">'
        + '</div>';}
    case 'wifi_save':
      return '<div class="widget" style="margin-top:6px">'
        + '<button class="action-btn" onclick="sendWifi()">'
        + (w.label || 'Save') + '</button></div>';
    case 'mqtt_save':
      return '<div class="widget" style="margin-top:6px">'
        + '<button class="action-btn" onclick="sendMqtt()">'
        + (w.label || 'Save') + '</button></div>';
    case 'datetime_input': {
      const inputId = id + '-input';
      return '<div class="widget">'
        + '<div class="widget-label" style="margin-bottom:6px">' + desc + '</div>'
        + '<input type="datetime-local" class="text-input" id="' + inputId + '">'
        + '</div>';}
    case 'time_save':
      return '<div class="widget" style="margin-top:6px">'
        + '<button class="action-btn" onclick="sendTime()">'
        + (w.label || 'Save') + '</button></div>';
    case 'firmware_upload':
      return '<div class="widget">'
        + '<div class="ota-info" id="ota-info"></div>'
        + '<input type="file" id="ota-file" accept=".bin" style="display:none"'
        + ' onchange="otaUpload(this)">'
        + '<button class="action-btn" onclick="document.getElementById(\'ota-file\').click()">'
        + (w.label || 'Upload') + '</button>'
        + '<div class="ota-progress" id="ota-progress" style="display:none">'
        + '<div class="progress-bar"><div class="progress-fill" id="ota-fill"></div></div>'
        + '<span id="ota-status">Uploading...</span></div></div>';
    case 'button':
      return '<div class="widget">'
        + '<button class="action-btn' + (w.confirm ? ' danger' : '') + '"'
        + ' onclick="actionBtn(\'' + (w.api_endpoint || '') + '\','
        + '\'' + (w.confirm || '') + '\')">' + (w.label || 'Action') + '</button></div>';
    case 'toggle': {
      const mod = w.key.split('.')[0];
      return '<div class="widget"><div class="widget-row">'
        + '<span class="widget-label">' + desc + '</span>'
        + '<button class="toggle-btn off" id="' + id + '"'
        + ' onclick="toggleState(\'' + mod + '\',\'' + w.key + '\')">' + '—</button></div></div>';}
    default:
      return '<div class="widget"><div class="widget-row">'
        + '<span class="widget-label">' + w.key + '</span>'
        + '<span class="widget-value" id="' + id + '">—</span>'
        + '</div></div>';
  }
}

function renderValues() {
  for (const [key, val] of Object.entries(state)) {
    const id = 'w-' + key.replace(/\./g, '-');
    const el = document.getElementById(id);
    if (!el) continue;
    if (el.classList.contains('gauge-value')) {
      el.textContent = typeof val === 'number' ? val.toFixed(1) : val;
      try {
        const zones = JSON.parse(el.dataset.zones || '[]');
        let color = 'var(--fg)';
        for (const z of zones) { if (val <= z.to) { color = z.color; break; } color = z.color; }
        el.style.color = color;
      } catch(e) {}
      continue;
    }
    if (el.classList.contains('indicator')) {
      const dot = el.querySelector('.indicator-dot');
      const lbl = el.querySelector('.indicator-label');
      const isOn = !!val;
      dot.style.background = isOn ? el.dataset.onColor : el.dataset.offColor;
      if (isOn) dot.classList.add('on'); else dot.classList.remove('on');
      lbl.textContent = isOn ? el.dataset.onLabel : el.dataset.offLabel;
      continue;
    }
    const sliderVal = document.getElementById(id + '-val');
    if (sliderVal) sliderVal.textContent = val;
    if (el.type === 'range') { el.value = val; continue; }
    if (el.classList.contains('toggle-btn')) {
      const isOn = !!val;
      el.textContent = isOn ? 'ON' : 'OFF';
      el.className = 'toggle-btn ' + (isOn ? 'on' : 'off');
      continue;
    }
    if (el.classList.contains('num-display')) {
      el.textContent = typeof val === 'number' ? (Number.isInteger(val) ? val : val.toFixed(1)) : val;
      continue;
    }
    if (typeof val === 'boolean') el.textContent = val ? 'ON' : 'OFF';
    else if (typeof val === 'number') el.textContent = Number.isInteger(val) ? val : val.toFixed(1);
    else el.textContent = val;
  }
  updateStatus(ws && ws.readyState === 1);
}

async function actionBtn(endpoint, confirmMsg) {
  if (confirmMsg && !confirm(confirmMsg)) return;
  if (endpoint) { try { await apiPost(endpoint, {}); } catch(e) { console.error('Action failed', e); } }
}

async function toggleState(module, key) {
  const current = state[key];
  await sendSetting(module, key, !current);
}

async function sendWifi() {
  const ssidEl = document.getElementById('w-wifi-ssid-input');
  const passEl = document.getElementById('w-wifi-password-input');
  if (!ssidEl) return;
  const ssid = ssidEl.value.trim();
  if (!ssid) { alert('SSID не може бути порожнім'); return; }
  try {
    const r = await apiPost('/api/wifi', {ssid: ssid, password: passEl ? passEl.value : ''});
    if (r.ok) alert('Збережено! Перезавантажте для застосування.');
    else alert('Помилка збереження');
  } catch(e) { alert('Помилка: ' + e.message); }
}

async function loadMqttInfo() {
  try {
    const d = await apiGet('/api/mqtt');
    setState({
      'mqtt.connected': d.connected || false,
      'mqtt.status': d.status || 'unknown',
      'mqtt.broker': d.broker || '',
    });
    // Заповнюємо форму поточними значеннями
    const broker = document.getElementById('w-mqtt-broker-input');
    const user = document.getElementById('w-mqtt-user-input');
    const prefix = document.getElementById('w-mqtt-prefix-input');
    const port = document.getElementById('w-mqtt-port-input');
    if (broker) broker.value = d.broker || '';
    if (user) user.value = d.user || '';
    if (prefix) prefix.value = d.prefix || '';
    if (port) port.value = d.port || 1883;
    // toggle enabled
    const enabled = document.getElementById('w-mqtt-enabled');
    if (enabled) {
      enabled.textContent = d.enabled ? 'ON' : 'OFF';
      enabled.className = 'toggle-btn ' + (d.enabled ? 'on' : 'off');
    }
  } catch(e) { console.warn('MQTT info load failed', e); }
}

async function sendMqtt() {
  const broker = document.getElementById('w-mqtt-broker-input');
  const port = document.getElementById('w-mqtt-port-input');
  const user = document.getElementById('w-mqtt-user-input');
  const pass = document.getElementById('w-mqtt-password-input');
  const prefix = document.getElementById('w-mqtt-prefix-input');
  const enabled = document.getElementById('w-mqtt-enabled');
  const data = {};
  if (broker) data.broker = broker.value.trim();
  if (port) data.port = parseInt(port.value) || 1883;
  if (user) data.user = user.value.trim();
  if (pass) data.password = pass.value;
  if (prefix) data.prefix = prefix.value.trim();
  if (enabled) data.enabled = enabled.classList.contains('on');
  try {
    const r = await apiPost('/api/mqtt', data);
    if (r.ok) alert('Збережено! MQTT перепідключається...');
    else alert('Помилка збереження');
  } catch(e) { alert('Помилка: ' + e.message); }
}

async function loadTimeInfo() {
  try {
    const d = await apiGet('/api/time');
    setState({
      'system.time': d.time || '--:--:--',
      'system.date': d.date || '--.--.----',
    });
    // Заповнюємо форму налаштувань
    const ntpEl = document.getElementById('w-time-ntp_enabled');
    if (ntpEl) {
      ntpEl.textContent = d.ntp_enabled ? 'ON' : 'OFF';
      ntpEl.className = 'toggle-btn ' + (d.ntp_enabled ? 'on' : 'off');
    }
    const tzEl = document.getElementById('w-time-timezone-input');
    if (tzEl) tzEl.value = d.timezone || '';
    // Якщо час синхронізовано — підставляємо в datetime-local
    const dtEl = document.getElementById('w-time-manual_datetime-input');
    if (dtEl && d.synced && d.unix > 1700000000) {
      const dt = new Date(d.unix * 1000);
      // datetime-local формат: YYYY-MM-DDTHH:MM
      const pad = n => String(n).padStart(2, '0');
      dtEl.value = dt.getFullYear() + '-' + pad(dt.getMonth() + 1) + '-'
        + pad(dt.getDate()) + 'T' + pad(dt.getHours()) + ':' + pad(dt.getMinutes());
    }
  } catch(e) { console.warn('Time info load failed', e); }
}

async function sendTime() {
  const ntpEl = document.getElementById('w-time-ntp_enabled');
  const tzEl = document.getElementById('w-time-timezone-input');
  const dtEl = document.getElementById('w-time-manual_datetime-input');
  const data = {};
  const ntpOn = ntpEl && ntpEl.classList.contains('on');
  data.mode = ntpOn ? 'ntp' : 'manual';
  if (tzEl && tzEl.value.trim()) data.timezone = tzEl.value.trim();
  if (!ntpOn && dtEl && dtEl.value) {
    data.time = Math.floor(new Date(dtEl.value).getTime() / 1000);
  }
  try {
    const r = await apiPost('/api/time', data);
    if (r.ok) alert('Збережено!');
    else alert('Помилка: ' + (r.error || 'unknown'));
  } catch(e) { alert('Помилка: ' + e.message); }
}

async function loadOtaInfo() {
  try {
    const r = await fetch('/api/ota');
    const d = await r.json();
    const el = document.getElementById('ota-info');
    if (el) el.innerHTML = 'v' + d.version + ' | ' + d.partition
      + ' | ' + d.date + ' ' + d.time;
    setState({
      '_ota.version': d.version || '',
      '_ota.partition': d.partition || '',
      '_ota.idf': d.idf || '',
      '_ota.date': (d.date || '') + ' ' + (d.time || '')
    });
  } catch(e) {}
}

function otaUpload(input) {
  const file = input.files[0];
  if (!file) return;
  if (!file.name.endsWith('.bin')) { alert('Only .bin files'); return; }
  if (!confirm('Оновити прошивку? Пристрій перезавантажиться.')) {
    input.value = ''; return;
  }
  const prog = document.getElementById('ota-progress');
  const fill = document.getElementById('ota-fill');
  const st = document.getElementById('ota-status');
  prog.style.display = '';
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/ota');
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      fill.style.width = pct + '%';
      st.textContent = pct + '% (' + Math.round(e.loaded / 1024) + ' KB)';
    }
  };
  xhr.onload = () => {
    if (xhr.status === 200) {
      st.textContent = 'Готово! Перезавантаження...';
      fill.style.width = '100%';
      setTimeout(() => location.reload(), 5000);
    } else {
      st.textContent = 'Помилка: ' + xhr.responseText;
      fill.style.background = '#c00';
    }
  };
  xhr.onerror = () => { st.textContent = 'Помилка з\'єднання'; };
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.send(file);
  input.value = '';
}

async function init() {
  try {
    UI = await apiGet('/api/ui');
    document.title = UI.device_name || 'ModESP';
  } catch(e) {
    console.error('Failed to load UI schema', e);
    document.getElementById('main').innerHTML =
      '<div class="card"><div class="card-title">Помилка</div>'
      + '<div class="card-body">Не вдалося завантажити UI. Перевірте /api/ui</div></div>';
    connectWs();
    return;
  }
  try { const s = await apiGet('/api/state'); setState(s); }
  catch(e) { console.warn('Initial state load failed', e); }
  currentPage = UI.pages[0]?.id || 'dashboard';
  renderNav(); renderPage(); connectWs();
}
init();
