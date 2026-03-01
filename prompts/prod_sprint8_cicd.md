# Production Sprint 8 — CI/CD Pipeline (1 сесія)

## Контекст

Тести розширені (Sprint 7). Потрібна автоматизація на кожен push.

## Задачі

### 1. GitHub Actions Workflow

Створити `.github/workflows/ci.yml`:

```yaml
name: ModESP CI
on: [push, pull_request]

jobs:
  python-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.11' }
      - run: pip install pytest
      - run: cd tools && python -m pytest tests/ -v

  host-cpp-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: |
          cd tests/host
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build
          cd build && ctest --output-on-failure

  webui-build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '20' }
      - run: cd webui && npm ci && npm run build
      - name: Check bundle size
        run: |
          SIZE=$(stat -f%z webui/dist/bundle.js.gz 2>/dev/null || stat -c%s webui/dist/bundle.js.gz)
          echo "Bundle size: $SIZE bytes"
          if [ $SIZE -gt 61440 ]; then echo "Bundle too large!" && exit 1; fi

  # Optional: ESP-IDF build (requires docker)
  # firmware-build:
  #   runs-on: ubuntu-latest
  #   container: espressif/idf:v5.5
  #   steps:
  #     - uses: actions/checkout@v4
  #     - run: idf.py build
```

### 2. Pre-commit Hook (optional)

`.pre-commit-config.yaml` або simple git hook:
- Run pytest on manifest changes
- Run eslint on webui changes

### Критерії завершення
- [ ] Push to main → all CI jobs green
- [ ] PR → CI runs automatically
- [ ] Bundle size > 60KB → CI fails
