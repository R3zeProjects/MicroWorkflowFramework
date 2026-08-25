# Architecture

## Scope and goal

MWF owns bounded process execution and ordered process workflows. It separates portable
workflow policy from native process creation so applications can use one C++23 API without
scattering Win32 or POSIX branches through domain code.

## Components

```text
Application ErrorModel (MCF contract)
                 |
        Runner / Workflow
                 |
     owning specifications and reports
                 |
       detail::run_process seam
          /                 \
 Windows Job Objects     POSIX fork/exec or posix_spawn
                         setrlimit/process group
```

- `types.hpp` owns stable data values and contains no native handles.
- `Runner<Model>` validates inputs, invokes one backend, and converts native failures into
  the selected error model.
- `Workflow<Model>` owns at most 1024 steps and applies ordering/fail-fast policy.
- `detail/backend.hpp` is the only link between templates and platform translation units.
- `process_windows.cpp` owns Win32 Job, process, and thread handles through RAII.
- `process_posix.cpp` installs limits before `exec`, reports setup failure over a close-on-
  exec pipe, and reaps every child.

## Execution flow

1. The caller creates owning process and resource values.
2. `Runner` rejects empty executables and zero-valued limits.
3. The backend creates the process in a controllable state.
4. Native resource controls are installed before application work begins.
5. The parent observes completion, wall time, and the stop token.
6. Timeout or cancellation terminates the owned process tree.
7. The backend reaps the process and returns an owning `ProcessResult`.
8. `Workflow` records the result and applies its failure policy.

## Invariants

- A returned launched process has already terminated and been reaped.
- Native handles do not cross the public API.
- Requested limits are never silently ignored.
- No detached thread or process is created by the framework.
- Workflow storage is bounded and preserves insertion order.
- Recoverable framework failures cross the API through the selected MCF error model.

## Portability decisions

Windows uses suspended process creation when a configured Job is required and native wait
objects for completion, cancellation, and deadlines. Launch-only execution skips Job setup.
POSIX uses a close-on-exec error pipe so pre-exec and `execvp` failures are distinguishable
from an application exit code. Launch-only execution with no working-directory or pre-exec
resource setup uses `posix_spawnp`; supervised execution retains the `fork` path and its
parent-death/process-group guarantees.

The platform backends intentionally expose the same categories rather than pretending the
semantics are identical. For example, Windows active-process limits are job-scoped while
`RLIMIT_NPROC` is commonly user-scoped.

## Extension points

Future backends may add Linux cgroup v2, namespaces, seccomp, Windows AppContainer, output
capture, explicit environment maps, dependency graphs, parallel scheduling, persistence,
telemetry, and resilience policies. Those features should extend the narrow backend or
compose with MPF/MTF/MRF; they must not introduce adapter objects into application code.

## Risks and open questions

- POSIX operations between `fork` and `exec` must remain async-signal-safe.
- Host policy can reject limits even when the API is compiled for that platform.
- Process-count behavior differs materially across operating systems.
- Secure hostile-code isolation requires a privileged sandbox design and dedicated threat
  model before it can be claimed.
