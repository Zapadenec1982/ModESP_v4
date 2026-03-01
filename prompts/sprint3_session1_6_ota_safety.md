# Sprint 3 / Сесія 1.6 — OTA Safety

## Контекст

HTTP auth додано (сесія 1.5). Тепер hardening OTA process.
**Проблеми:**
1. Немає перевірки розміру firmware vs partition size
2. Немає перевірки magic byte (будь-який файл приймається)
3. `esp_ota_mark_app_valid_cancel_rollback()` викликається одразу при boot — немає timeout

## Задачі

### 1. OTA Size Validation

Файл: `components/modesp_net/src/http_service.cpp` (рядок ~882)

```cpp
// Before esp_ota_begin():
if (req->content_len > update_partition->size) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware too large");
    return ESP_FAIL;
}
if (req->content_len < 256) {  // Min reasonable firmware size
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too small");
    return ESP_FAIL;
}
```

### 2. Magic Byte Verification

Файл: `components/modesp_net/src/http_service.cpp`

Після отримання першого chunk:
```cpp
if (buf[0] != 0xE9) {  // ESP_IMAGE_HEADER_MAGIC
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not a valid ESP32 firmware");
    esp_ota_abort(update_handle);
    return ESP_FAIL;
}
```

### 3. Rollback Timeout

Файл: `main/main.cpp` (рядок ~114-117)

Зараз: `esp_ota_mark_app_valid_cancel_rollback()` викликається безумовно при boot.

**Fix:**
- НЕ маркувати valid одразу
- Запустити таймер 60 секунд
- Маркувати valid тільки після:
  - WiFi connected (або AP mode active)
  - HTTP server started
  - Перший успішний WS connection або API request
- Якщо таймер вичерпався без маркування → при наступному reboot ESP-IDF автоматично rollback
- Log: "OTA validation: waiting for network..." → "OTA validated after Xs"

```cpp
// main.cpp — замість миттєвого маркування:
static bool ota_validated = false;
static uint32_t ota_validation_start = 0;

// В boot sequence:
if (esp_ota_get_state(...) == ESP_OTA_IMG_PENDING_VERIFY) {
    ota_validation_start = xTaskGetTickCount();
    ESP_LOGI(TAG, "OTA: validating new firmware (60s timeout)...");
}

// В main loop (або в callback після WiFi + HTTP ready):
if (!ota_validated && ota_validation_start > 0) {
    if (wifi_connected && http_started && ws_has_clients) {
        esp_ota_mark_app_valid_cancel_rollback();
        ota_validated = true;
        ESP_LOGI(TAG, "OTA: firmware validated successfully");
    } else if (elapsed > 60000) {
        ESP_LOGE(TAG, "OTA: validation timeout — will rollback on next reboot");
        esp_restart();
    }
}
```

### 4. WebUI OTA Improvements

Файл: `webui/src/components/widgets/FirmwareUpload.svelte`

- Показати розмір файлу перед upload
- Warning якщо розмір > 1.4MB (partition = 1.5MB, залишити margin)
- Після upload: "Firmware завантажено. Перезавантаження через 5с..."
- Countdown timer → auto-refresh page

## Файли

| Файл | Дія |
|------|-----|
| `components/modesp_net/src/http_service.cpp` | РЕДАГУВАТИ (size + magic byte) |
| `main/main.cpp` | РЕДАГУВАТИ (rollback timeout) |
| `webui/src/components/widgets/FirmwareUpload.svelte` | РЕДАГУВАТИ (UX) |

## Критерії завершення

- [ ] `idf.py build` без помилок
- [ ] Upload > 1.5MB → 400 "Firmware too large"
- [ ] Upload non-firmware file → 400 "Not a valid ESP32 firmware"
- [ ] Rollback: crash перед validation → rollback при reboot
- [ ] WebUI: file size displayed, countdown after upload
- [ ] `npm run build` < 55KB gz

## Після завершення

```bash
idf.py build
cd webui && npm run build && npm run deploy
git add components/modesp_net/src/http_service.cpp main/main.cpp \
  webui/src/components/widgets/FirmwareUpload.svelte
git commit -m "fix(ota): safety — size validation, magic byte check, 60s rollback timeout

- Reject firmware larger than partition (1.5MB)
- Verify ESP32 magic byte 0xE9 in first chunk
- Rollback timeout: 60s to validate (WiFi + HTTP + WS)
- WebUI: file size display, upload countdown"
git push origin main
```

Оновити ACTION_PLAN.md — Sprint 3 завершено.
