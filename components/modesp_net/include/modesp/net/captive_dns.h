/**
 * @file captive_dns.h
 * @brief Wildcard DNS server for captive portal (SoftAP mode)
 *
 * Відповідає IP-адресою AP-інтерфейсу пристрою на БУДЬ-який DNS A-запит
 * ("DNS hijack"). Це змушує OS-проби виявлення captive portal
 * (connectivitycheck.gstatic.com, captive.apple.com, msftconnecttest.com …)
 * резолвитись на наш HTTP-сервер, який далі віддає портал-сторінку.
 *
 * Запускається лише в режимі AP (керується з WiFiService). Обидві функції
 * ідемпотентні — повторний start/stop безпечний.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Підняти DNS-сервер на UDP:53 (no-op якщо вже запущений).
void captive_dns_start(void);

/// Зупинити DNS-сервер і звільнити сокет (no-op якщо не запущений).
void captive_dns_stop(void);

#ifdef __cplusplus
}
#endif
