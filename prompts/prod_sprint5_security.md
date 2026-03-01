# Production Sprint 5 — Security Hardening (3 сесії)

## Контекст

MVP завершено. Для масового розгортання потрібна повна безпека.

## Сесія 5a: HTTPS з Self-Signed Certificates

### Задачі
- [ ] Генерація self-signed cert при першому boot (ESP-IDF mbedTLS)
- [ ] Зберігання cert/key в NVS або окремому partition
- [ ] `httpd_ssl_config_t` замість звичайного httpd
- [ ] sdkconfig: `CONFIG_HTTPD_SSL_USING_MBEDTLS=y`
- [ ] WebUI: обробка browser certificate warnings
- [ ] HTTP → HTTPS redirect (порт 80 → 443)

### Файли
- `components/modesp_net/src/http_service.cpp`
- `sdkconfig.defaults`
- `main/main.cpp`

## Сесія 5b: Secure Boot v2 + Flash Encryption

### Задачі
- [ ] sdkconfig: `CONFIG_SECURE_BOOT=y`, `CONFIG_SECURE_BOOT_V2_ENABLED=y`
- [ ] Генерація signing key pair
- [ ] Flash encryption: `CONFIG_FLASH_ENCRYPTION_ENABLED=y` (Development mode для тестування)
- [ ] NVS encryption для credentials (WiFi pass, MQTT pass, auth pass)
- [ ] Документація: workflow для burning keys до eFuse
- [ ] **УВАГА:** eFuse burn = одноразова операція! Тестувати спершу в Development mode

### Файли
- `sdkconfig.defaults`
- Документація: `docs/11_security.md`

## Сесія 5c: Security Audit

### Задачі
- [ ] CSRF protection: перевірка Origin header на POST requests
- [ ] Rate limiting: max 5 auth failures per minute (NVS counter)
- [ ] Path traversal: verify AUDIT-040 fix (block `..` in paths)
- [ ] WebSocket auth: require auth для WS connection (не тільки HTTP POST)
- [ ] Hardcoded default password warning: if still "modesp" → show warning in WebUI

### Критерії завершення
- [ ] HTTPS працює з self-signed cert
- [ ] Secure Boot: unsigned firmware rejected at boot
- [ ] Flash encryption: flash dump не readable
- [ ] CSRF: cross-origin POST blocked
- [ ] Rate limiting: 6th failed login → 429 Too Many Requests
