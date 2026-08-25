# Installation

## Requirements

- CMake 3.25 or newer.
- A C++23 compiler and standard library with `std::expected` and `std::stop_token`.
- Windows 10 or newer, Linux, or a POSIX platform with `fork`, `exec`, and `setrlimit`.
- MicroContractsFramework 0.6.

## Source-tree build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMWF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

## Installed MCF or pinned fetch

MWF resolves dependencies in this order:

1. Existing CMake target `vosp::contracts`.
2. `MWF_CONTRACTS_SOURCE_DIR`.
3. Installed package `vosp_contracts 0.6`.
4. Pinned Git revision when `MWF_FETCH_CONTRACTS=ON`.

Disable network fallback with `-DMWF_FETCH_CONTRACTS=OFF` for reproducible offline builds.

## Install and consume

```bash
cmake --install build --prefix /opt/vosp
```

```cmake
find_package(mwf 0.1 REQUIRED CONFIG)
target_link_libraries(my_service PRIVATE vosp::workflow)
```

```cpp
#include <vosp/workflow.hpp>
```

The installed package contains the public headers, one native static library, CMake export
files, and the license. Tests, examples, benchmarks, and build directories are not
installed.

## Development options

| Option | Default | Purpose |
| --- | ---: | --- |
| `BUILD_TESTING` | CTest default | Runtime, header, and compile-fail tests. |
| `MWF_BUILD_EXAMPLES` | `ON` | Build the basic executable example. |
| `MWF_BUILD_BENCHMARKS` | `OFF` | Build the opt-in process benchmark. |
| `MWF_BUILD_MEF_INTEGRATION` | `OFF` | Build direct integration with pinned MEF. |
| `MWF_ENABLE_SANITIZERS` | `OFF` | Enable ASan and UBSan where supported. |
| `MWF_ENABLE_THREAD_SANITIZER` | `OFF` | Enable TSan in a separate build. |

## Troubleshooting

- A launch error means no user process result exists; inspect the typed error message.
- A nonzero exit code means launch succeeded and the child reported failure.
- If a native limit is denied, check account privileges and host policy rather than
  disabling the limit silently.
- Sanitizer runtimes reserve large virtual address ranges; very small memory limits may
  prevent an instrumented child from starting.
