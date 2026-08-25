# Changelog

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

- Added typed single-process execution through `Runner<ErrorModel>`.
- Added bounded sequential workflows with a hard limit of 1024 steps.
- Added memory, CPU-time, wall-time, process-count, cancellation, and tree-cleanup controls.
- Added Windows Job Object and POSIX `setrlimit` backends.
- Added MCF error-model contracts, CMake package installation, examples, tests, CI, and an
  opt-in process round-trip benchmark.
- Documented the boundary between resource control and container-grade security isolation.
