# Production Sprint 10 — Production WebUI (3 сесії)

## Сесія 10a: PWA + Service Worker

### Задачі
- [ ] `webui/public/manifest.json` — PWA manifest (name, icons, theme_color)
- [ ] Service Worker: cache UI shell (index.html, bundle.js.gz, bundle.css.gz)
- [ ] Offline fallback: show cached dashboard with "Offline" badge
- [ ] Install prompt: "Додати на головний екран" на mobile
- [ ] Background sync: queue settings changes while offline → send when online

## Сесія 10b: Accessibility (WCAG 2.1 AA)

### Задачі
- [ ] Keyboard navigation: Tab order through all interactive elements
- [ ] Focus rings: visible focus indicator on all interactive elements
- [ ] ARIA labels: role, aria-label, aria-describedby на widgets
- [ ] Screen reader: toast announcements via aria-live
- [ ] Contrast: verify all text meets 4.5:1 ratio (AA)
- [ ] Reduced motion: respect prefers-reduced-motion

## Сесія 10c: Performance Optimization

### Задачі
- [ ] Lazy-load ChartWidget (не потрібен на dashboard якщо не видимий)
- [ ] Virtual scrolling для event list (якщо >100 подій)
- [ ] Image optimization: SVG icons → sprite sheet
- [ ] CSS: purge unused styles
- [ ] Bundle analysis: identify largest modules
- [ ] Target: < 50KB gzipped
