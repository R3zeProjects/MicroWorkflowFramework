# Benchmark methodology

The opt-in benchmark measures complete process round trips through the public
`ProcessRunner` API. Each iteration creates the benchmark executable as a child, executes a
minimal child path, waits for exit code zero, closes native resources, and validates each
result inside the measured round trip. It reports supervised execution and the explicit
launch-only policy separately.

## Build and run

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DMWF_BUILD_EXAMPLES=OFF -DMWF_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --parallel 2
./build-bench/MicroWorkflowFrameworkBenchmark 100
```

On multi-config Windows generators, run
`build-bench/Release/MicroWorkflowFrameworkBenchmark.exe`.

The 2026-08-26 Clang 22.1.6 Release verification used seven independent
processes with 100 launches per scenario. Median complete round trips were
51.96 launches/s in supervised mode and 51.45 launches/s in launch-only mode.
Raw samples are stored in
[`current-native-raw-2026-08-26.csv`](../benchmark-results/current-native-raw-2026-08-26.csv).

## Linux v0.1.0 to v0.1.1 A/B

Date: 2026-08-25.

| Field | Value |
| --- | --- |
| CPU | AMD Ryzen 7 PRO 1700X, 8 cores / 16 logical processors |
| Memory | 31.95 GiB |
| Environment | Ubuntu 24.04 container on Windows 10 Pro |
| Compiler | GCC 13.3, Release |
| Workload | 7 rounds × 200 sequential launches per scenario |

| Scenario | Median launches/s | Change from v0.1.0 |
| --- | ---: | ---: |
| v0.1.0 supervised baseline | 181.34 | Baseline |
| v0.1.1 supervised fast wait | 205.35 | +13.2% |
| v0.1.1 launch-only `posix_spawnp` | 215.85 | +19.0% |

The old revision and v0.1.1 were compiled in the same container and launched their own
respective benchmark executable. Supervised mode retains process-group and Linux
parent-death guarantees. Launch-only mode deliberately disables descendant ownership and
uses the cheaper platform path.

## Windows external comparison

The external benchmark uses Boost.Process 2.0 from Boost 1.90 and libuv 1.52.1. All four
scenarios launch the same executable with the same argument and wait for verified exit code
zero. Seven rotated rounds of 50 launches produced these medians:

| Scenario | Median launches/s |
| --- | ---: |
| libuv | 28.31 |
| MWF supervised | 28.14 |
| MWF launch-only | 26.58 |
| Boost.Process 2.0 | 26.82 |

Machine: AMD Ryzen 7 PRO 1700X, Windows 10 Pro, MSVC 19.51 Release. The spread between
rounds was large because executable startup, endpoint inspection, and scheduler activity
dominate framework overhead. Treat the values as same-machine evidence, not universal
rankings. These 2026-08-26 values come from seven rotated rounds of 50 launches
per scenario; every exit code was validated. Raw rows are stored in
[`external-process-raw-2026-08-26.csv`](../benchmark-results/external-process-raw-2026-08-26.csv).

Build this optional target after providing Boost.Process and libuv CMake packages:

```bash
cmake -S . -B build-compare -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DMWF_BUILD_EXAMPLES=OFF \
  -DMWF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS=ON
cmake --build build-compare --config Release --parallel 2
./build-compare/MicroWorkflowFrameworkExternalBenchmark 50 7
```

## Interpretation

The benchmark includes native process creation and teardown, which dominate the cost. It
does not compare MWF with an in-process thread pool, because those mechanisms provide
different failure and resource boundaries. CI runs a shorter smoke workload and uploads the
raw CSV-style output; published comparisons must use the same executable, host, limits,
iteration count, delivered work, warm-up, and scenario ordering.

Benchmarks are development evidence and are never installed with the package.
