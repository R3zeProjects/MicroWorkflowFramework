# Benchmark methodology

The opt-in benchmark measures complete process round trips through the public
`ProcessRunner` API. Each iteration creates the benchmark executable as a child, executes a
minimal child path, waits for exit code zero, closes native resources, and validates each
result inside the measured round trip.

## Build and run

```bash
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DMWF_BUILD_EXAMPLES=OFF -DMWF_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --parallel 2
./build-bench/MicroWorkflowFrameworkBenchmark 100
```

On multi-config Windows generators, run
`build-bench/Release/MicroWorkflowFrameworkBenchmark.exe`.

## Recorded baseline

Date: 2026-08-25.

| Field | Value |
| --- | --- |
| CPU | AMD Ryzen 7 PRO 1700X, 8 cores / 16 logical processors |
| Memory | 31.95 GiB |
| OS | Windows 10 Pro |
| Compiler | MSVC 19.51, Release |
| Iterations | 100 sequential launches |
| Total time | 1.91971 s |
| Throughput | 52.0913 launches/s |

## Interpretation

The benchmark includes native process creation and teardown, which dominate the cost. It
does not compare MWF with an in-process thread pool, because those mechanisms provide
different failure and resource boundaries. CI runs a shorter smoke workload and uploads the
raw CSV-style output; published comparisons must use the same executable, host, limits,
iteration count, and delivered work.

Benchmarks are development evidence and are never installed with the package.
