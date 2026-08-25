# Framework comparison

MWF overlaps with process libraries and sandbox runtimes, but it does not replace either
category completely. This comparison separates process orchestration, resource supervision,
and adversarial isolation so unlike products are not ranked by one misleading number.

## Capability matrix

| Capability | MWF 0.1.1 | Boost.Process 2.0 | libuv 1.52 | Sandbox2 | OCI runtime |
| --- | --- | --- | --- | --- | --- |
| Primary role | Bounded process workflows | General C++ process API | Event-loop process and I/O API | Linux syscall/namespace sandbox | Container lifecycle specification |
| Windows and POSIX | Yes | Yes | Yes | No, Linux | Runtime-dependent |
| Typed non-throwing domain result | MCF `Result<T>` | Error-code and throwing APIs | Integer error codes/callbacks | Status-oriented API | Runtime protocol |
| Cancellation/deadline | `std::stop_token` + wall deadline | Asio cancellation/timers | Event-loop callbacks and kill | Monitor policy | Runtime lifecycle operations |
| Memory/CPU/process-count envelope | Built in | Not its primary API | Not its primary API | Sandbox policy/resource mechanisms | cgroups and rlimits |
| Process-tree cleanup | Job Object/process group | Platform process primitives | Process handle and signals | Sandbox monitor | Container/cgroup lifecycle |
| Environment and stdio routing | Not yet | Built in | Built in | Sandbox-specific | Built in to container configuration |
| Syscall/filesystem/network security boundary | No | No | No | Linux seccomp and namespaces | Runtime-dependent namespaces, mounts and policy |
| External runtime required | No | No | No | Yes, sandbox infrastructure | Yes |

Boost.Process 2.0 is fully Asio-based, uses `pidfd_open` where available, supports UTF-8,
and closes non-whitelisted POSIX descriptors by default according to its
[official design notes](https://www.boost.org/doc/libs/latest/libs/process/doc/html/index.html#boost_process.design).
It is the stronger choice when asynchronous pipes and broad process composition are the
main requirement.

libuv exposes process launch through `uv_spawn`, environment and working-directory fields,
stdio routing, exit callbacks, and signal-like termination through its
[process API](https://docs.libuv.org/en/v1.x/process.html). It is the stronger choice inside
an existing libuv event loop.

Sandbox2 implements Linux seccomp policy and namespace-oriented isolation through its
[policy layer](https://github.com/google/sandboxed-api/blob/main/sandboxed_api/sandbox2/policy.h).
OCI runtimes add a substantially larger container boundary; the
[OCI Linux configuration](https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md)
defines cgroups for memory, CPU, PIDs, I/O and other resources.

MWF is strongest when an application needs a small C++23 API, replaceable MCF error models,
synchronous cancellation, bounded ordered workflows, cross-platform resource controls, and
deterministic process-tree cleanup without adopting an event loop or container runtime.

## Performance comparison

The same-machine benchmark in [BENCHMARKS.md](BENCHMARKS.md) compares only equivalent
synchronous launch-and-wait work against Boost.Process and libuv. Sandbox2 and OCI runtimes
are intentionally excluded: namespace, policy, filesystem, and container setup provide a
different security boundary and therefore a different workload.

The measured Windows medians place MWF in the same launch-throughput band as Boost.Process
and libuv. The medians ranged from 24.67 to 31.92 launches/s, while individual rounds varied
far more widely and even reversed the order of MWF's supervised and launch-only modes. The
defensible conclusion is parity under dominant operating-system launch noise, not a
universal lead for any library.

## Missing capabilities and roadmap

- Add explicit environment and stdio policies without changing the owning `ProcessSpec`
  model.
- Add an asynchronous execution surface only with an explicit scheduler and allocation
  model; do not hide a thread per process.
- Add cgroup v2, namespaces, seccomp, and AppContainer as separate capability-bearing
  backends before claiming hostile-code isolation.
- Preserve the launch-only fast path for callers that explicitly opt out of descendant and
  pre-exec controls.
