# Sprint 3 / Сесія 1.5 — HTTP Basic Auth

## Контекст

WebUI UX оновлено (Sprints 1-2). Тепер базова безпека перед field deployment.
**Проблема:** будь-хто в мережі може OTA, factory-reset, змінити WiFi — без авторизації.

## Задачі

### 1. HTTP Auth Middleware (ESP32 firmware)

Файл: `components/modesp_net/src/http_service.cpp` + `http_service.h`

**Два рівні доступу:**
- **Public (без auth):** GET /api/state, GET /api/ui, GET /api/board, GET /api/modules, /ws, static files (/, /bundle.js.gz, etc.)
- **Protected (потребує auth):** POST /api/settings, POST /api/wifi, POST /api/mqtt, POST /api/ota, POST /api/factory-reset, POST /api/restart, POST /api/bindings, GET /api/wifi/scan

**Реалізація:**
```cpp
// http_service.h
char auth_user_[32] = "admin";
char auth_pass_[32] = "modesp";  // default password
bool auth_enabled_ = true;

static bool check_auth(httpd_req_t* req);
void load_auth_from_nvs();
void save_auth_to_nvs();
```

- HTTP Basic Auth (RFC 7617): перевірити `Authorization: Basic base64(user:pass)` header
- Якщо missing або invalid → 401 + `WWW-Authenticate: Basic realm="ModESP"`
- NVS namespace "auth", keys: "user", "pass"
- Default credentials: admin / modesp
- Додати POST /api/auth/password endpoint для зміни пароля

### 2. WebUI Auth Integration

Файл: `webui/src/lib/api.js`

- Додати auth header до всіх POST requests:
  ```javascript
  function getAuthHeader() {
    const creds = sessionStorage.getItem('modesp_auth');
    return creds ? { 'Authorization': `Basic ${creds}` } : {};
  }
  ```
- При 401 response → показати login modal
- Credentials зберігати в `sessionStorage` (NOT localStorage — clear on tab close)

### 3. Login Modal Component

Створити: `webui/src/components/LoginModal.svelte`

- Username + Password inputs
- "Увійти" button
- Error message при невірних credentials
- Показується при першому 401 або при навігації до protected page
- Після успішного login → зберегти в sessionStorage → retry failed request

### 4. Change Password Card

На System page (через manifest або hardcoded):
- Current password + New password + Confirm password
- POST /api/auth/password

## Файли

| Файл | Дія |
|------|-----|
| `components/modesp_net/src/http_service.cpp` | РЕДАГУВАТИ (check_auth, NVS) |
| `components/modesp_net/include/modesp/net/http_service.h` | РЕДАГУВАТИ |
| `webui/src/lib/api.js` | РЕДАГУВАТИ (auth header, 401 handling) |
| `webui/src/components/LoginModal.svelte` | СТВОРИТИ |
| `webui/src/App.svelte` | РЕДАГУВАТИ (mount LoginModal) |

## Критерії завершення

- [ ] `idf.py build` без помилок
- [ ] POST /api/settings без auth → 401
- [ ] POST /api/settings з правильним auth → 200
- [ ] GET /api/state, GET /api/ui → 200 без auth (public)
- [ ] WebUI: login modal при першому 401
- [ ] Зміна пароля працює через POST /api/auth/password
- [ ] `npm run build` < 55KB gz
- [ ] Default: admin/modesp → prompt to change

## Після завершення

```bash
idf.py build
cd webui && npm run build && npm run deploy
git add components/modesp_net/ webui/src/
git commit -m "feat(auth): HTTP Basic Auth — protected POST endpoints, WebUI login modal

- Public: GET /api/state, /api/ui, /ws, static files
- Protected: POST /api/settings, /api/ota, /api/factory-reset, etc.
- NVS storage for credentials (namespace 'auth')
- WebUI: LoginModal component, sessionStorage creds
- Default: admin/modesp (AUDIT-030)"
git push origin main
```
