# Production Sprint 11 — Fleet Management (5+ сесій)

## Контекст

Це фінальна фаза для масового розгортання 100+ пристроїв.
Потребує backend infrastructure (окремий від ESP32).

## Сесія 11a: Backend Architecture

### Задачі
- [ ] Вибір стеку: Node.js/Python + PostgreSQL + MQTT broker (Mosquitto/EMQX)
- [ ] MQTT bridge: subscribe to modesp/+/# → store in DB
- [ ] REST API: /devices, /devices/:id/state, /devices/:id/settings
- [ ] WebSocket: real-time device status to dashboard

## Сесія 11b: Web Dashboard

### Задачі
- [ ] Device list: name, IP, last seen, temperature, alarm status
- [ ] Device detail: live state, settings editor, event history
- [ ] Map view: devices on floor plan (optional)
- [ ] Alarm aggregation: unified alarm list across all devices

## Сесія 11c: Group OTA

### Задачі
- [ ] Firmware upload to backend → stored in S3/local storage
- [ ] Device groups (by location, firmware version, etc.)
- [ ] Staged rollout: 10% → 50% → 100% (canary deployment)
- [ ] OTA status tracking: pending/uploading/validating/done/failed
- [ ] Rollback trigger: if >10% devices fail → auto-rollback group

## Сесія 11d: Notifications

### Задачі
- [ ] Alarm notifications: email (SendGrid/SMTP)
- [ ] Telegram bot: alarm messages to group chat
- [ ] Notification rules: per-device, per-alarm-type, schedule (mute at night)
- [ ] Escalation: if alarm unacknowledged >30min → escalate to manager

## Сесія 11e: Device Provisioning

### Задачі
- [ ] QR code на пристрої → scan → auto-register in fleet
- [ ] First boot wizard: WiFi → MQTT broker → fleet registration
- [ ] Configuration templates: "Морозильна камера -18°C", "Холодильна вітрина +4°C"
- [ ] Bulk configuration: apply template to group of devices
