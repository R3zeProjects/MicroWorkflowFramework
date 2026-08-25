# Changelog

## 0.1.0-beta — 2026-08-25

- Added typed single-process execution through `Runner<ErrorModel>`.
- Added bounded sequential workflows with a hard limit of 1024 steps.
- Added memory, CPU-time, wall-time, process-count, cancellation, and tree-cleanup controls.
- Added Windows Job Object and POSIX `setrlimit` backends.
- Added MCF error-model contracts, CMake package installation, examples, tests, CI, and an
  opt-in process round-trip benchmark.
- Documented the boundary between resource control and container-grade security isolation.
