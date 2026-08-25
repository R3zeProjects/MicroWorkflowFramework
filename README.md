# MicroWorkflowFramework

**MicroWorkflowFramework (MWF)** is a C++23 micro-framework for running bounded process
workflows under explicit operating-system resource controls.

It provides one typed API for process launch, memory and CPU limits, wall-clock deadlines,
process-count limits, process-tree cleanup, cooperative cancellation, and ordered workflow
execution. MWF uses [MicroContractsFramework](https://github.com/R3zeProjects/MicroContractsFramework)
for replaceable error/result models and does not require MicroErrorFramework at runtime.

Current release: **0.1.0-beta**.

## What it provides

- Owning `ProcessSpec`, `ResourceLimits`, and result values.
- `Runner<ErrorModel>` for one isolated child process.
- `Workflow<ErrorModel>` for up to 1024 uniquely named ordered steps.
- Fail-fast or continue-on-failure workflow policy.
- `std::stop_token` cancellation and bounded wall-clock execution.
- Process-tree termination during timeout, cancellation, and runner cleanup.
- Windows Job Object limits for memory, CPU time, active processes, and tree lifetime.
- POSIX `setrlimit`, process groups, and Linux parent-death signaling.
- Compile-time MCF contracts instead of a mandatory error implementation.
- CMake installation package, compile-fail tests, sanitizers, static analysis, and benchmark
  smoke tests.

## Important isolation boundary

MWF is a process resource-control framework, not a container runtime. Version 0.1.0-beta
does **not** create Linux namespaces, a private root filesystem, seccomp filters, virtual
networks, Windows AppContainers, or a privilege boundary. Do not run hostile code under the
assumption that MWF provides Docker-equivalent security isolation.

Use MWF when the goal is bounded execution and deterministic cleanup of trusted or
semi-trusted worker processes. Use a container, VM, or operating-system sandbox when the
goal is adversarial isolation.

## Platform guarantees

| Capability | Windows | Linux | macOS/POSIX |
| --- | --- | --- | --- |
| Memory limit | Job Object process memory | `RLIMIT_AS` | `RLIMIT_AS` where supported |
| CPU-time limit | Job Object process time | `RLIMIT_CPU` | `RLIMIT_CPU` |
| Wall-time limit | Parent deadline + Job termination | Parent deadline + `SIGKILL` | Parent deadline + `SIGKILL` |
| Process-count limit | Job active-process limit | `RLIMIT_NPROC` | `RLIMIT_NPROC` where supported |
| Descendant cleanup | Kill-on-job-close | Process-group termination | Process-group termination |
| Parent-death cleanup | Job lifetime | `PR_SET_PDEATHSIG` + process group | Process group owned by runner |

`RLIMIT_NPROC` is account-scoped on common POSIX systems and is not equivalent to a cgroup
PID controller. Query `vosp::capabilities()` and read [API contracts](docs/API_CONTRACTS.md)
before depending on a platform-specific guarantee.

## Quick start

```cpp
#include <vosp/workflow.hpp>

vosp::ProcessWorkflow<MyErrorModel> workflow;

vosp::ResourceLimits limits;
limits.memory_bytes = 256ULL * 1024 * 1024;
limits.cpu_time = std::chrono::seconds{10};
limits.wall_time = std::chrono::seconds{15};
limits.process_count = 2;

if (!workflow.add({
        "index-assets",
        {.executable = "asset-indexer", .arguments = {"--input", "assets"}},
        limits})) {
    return 1;
}

auto report = workflow.run(stop_token);
if (!report) {
    handle_framework_error(report.error());
} else if (!report->succeeded()) {
    handle_process_failure(report->steps.back().process.exit_code);
}
```

`MyErrorModel` may be the MEF error model or any implementation satisfying
`vosp::contracts::ErrorModel` and the MWF result-construction requirements. No adapter or
base-class inheritance is required.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMWF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Without `MWF_CONTRACTS_SOURCE_DIR`, CMake first searches for `vosp_contracts 0.6` and then
fetches the pinned compatible MCF revision when `MWF_FETCH_CONTRACTS=ON`.

## Measured process-launch baseline

The opt-in benchmark launches the benchmark executable as a child, waits for a verified
zero exit, and reports complete native process round trips. It is not installed with the
package.

```text
Machine: AMD Ryzen 7 PRO 1700X, 8 cores / 16 threads, 31.95 GiB RAM
OS/toolchain: Windows 10 Pro, MSVC 19.51, Release
Workload: 100 sequential native process round trips
Result: 1.91971 s total, 52.0913 launches/s
```

Process creation is an operating-system operation; this number is a reproducible baseline,
not a claim that MWF makes process startup cheaper than the underlying OS. See
[benchmark methodology](docs/BENCHMARKS.md).

## Documentation

- [API contracts](docs/API_CONTRACTS.md)
- [Architecture and operating principles](docs/ARCHITECTURE.md)
- [Installation](docs/INSTALLATION.md)
- [Usage examples](docs/USAGE_EXAMPLES.md)
- [Benchmark methodology](docs/BENCHMARKS.md)

## Repository layout

```text
include/vosp/workflow/  Public values, Runner, Workflow, and version API
src/                    Windows and POSIX platform backends
tests/                  Runtime, header, compile-fail, and package tests
examples/               Compilable API example
benchmarks/             Opt-in process round-trip benchmark
docs/                   Contracts, architecture, setup, and operations
```

## License

MIT License. See [LICENSE](LICENSE).
