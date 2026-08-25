# Журнал изменений

## 0.1.1-beta — 2026-08-25

- Added event-driven Windows waiting and a direct launch path when no Job Object control is
  requested.
- Added blocking POSIX completion for the common synchronous path and `posix_spawnp` for
  launch-only executions that need no pre-exec setup.
- Made POSIX waits resilient to `EINTR` and preserved launch-pipe errors across cleanup.
- Reject unsupported requested controls before launch through the typed error channel.
- Added 64-launch concurrency stress coverage and made Release tests independent of
  `assert`/`NDEBUG`.
- Added reproducible comparisons with Boost.Process 2.0 and libuv plus Linux v0.1.0 A/B
  measurements.

## 0.1.0-beta — 2026-08-25

- Добавлено типизированное выполнение одного процесса через `Runner<ErrorModel>`.
- Добавлены ограниченные последовательные рабочие процессы с жёстким пределом в 1024
  шага.
- Добавлены ограничения памяти, процессорного времени, времени выполнения и числа
  процессов, а также отмена и очистка дерева процессов.
- Добавлены backend на основе Windows Job Object и POSIX `setrlimit`.
- Добавлены контракты модели ошибок MCF, установка пакета CMake, примеры, тесты, CI и
  опциональный бенчмарк полного цикла процесса.
- Задокументирована граница между контролем ресурсов и безопасной изоляцией контейнерного
  уровня.
