# API contracts

## Error model

`Runner<Model>` and `Workflow<Model>` require an MCF-compatible owning error model. The
model provides `Error`, `Result<T>`, `OperationResult`, and `make_error`. MWF never stores a
reference to the model and does not require inheritance or a runtime adapter.

The selected `Result<T>` must be constructible from the requested value and from
`std::unexpected<Model::Error>` for both `ProcessResult` and `WorkflowReport`.

## Process specification

- `ProcessSpec::executable` must not be empty.
- Arguments and the optional working directory are owning values.
- Arguments are UTF-8 at the public boundary; the Windows backend converts them to UTF-16.
- The child inherits the parent environment in 0.1.1-beta.
- A nonzero child exit is a successful launch result, not an MWF framework error.
- Failure to create or execute the requested image uses `error_code::launch_failed`;
  failures in native supervision setup use `error_code::platform_failure`.

## Resource limits

All supplied numeric limits must be greater than zero. A failure to install a requested
native limit fails the launch instead of silently running without that limit.

| Limit | Contract |
| --- | --- |
| `memory_bytes` | Bounds one process on Windows and the address space on POSIX. |
| `cpu_time` | Bounds per-process user CPU time; it is not a throughput quota. |
| `wall_time` | Parent observes a monotonic deadline and terminates the process tree. |
| `process_count` | Uses a Windows Job limit or POSIX `RLIMIT_NPROC`; POSIX scope is account-dependent. |
| `terminate_descendants` | Places the child in a killable job/process group when true. |

`Capabilities` reports whether the compiled backend implements each category. It does not
elevate process privileges or prove that the host configuration permits every requested
limit.

## Lifecycle and cancellation

- `Runner::run()` is synchronous and owns the child until it has been reaped.
- `std::stop_token` cancellation is cooperative for the caller and forceful for the child.
- Timeout and cancellation wait for process termination before returning.
- POSIX children are always reaped; Windows process, thread, and Job handles use RAII.
- Closing a Windows Job with kill-on-close enabled terminates remaining descendants.
- MWF does not detach processes or leave background worker threads.
- Separate calls to one stateless `Runner` instance may execute concurrently. Each call owns
  its native handles and process lifecycle.

## Workflow

- Steps execute in insertion order.
- Names must be nonempty and unique within one workflow.
- Executables must be nonempty.
- `max_steps` is clamped to the hard limit of 1024.
- `add()` returns `false` for invalid, duplicate, or over-capacity steps.
- Fail-fast mode stops after the first nonzero, signaled, timed-out, or cancelled result.
- A framework error such as launch failure is returned through the configured error model.
- `Workflow` is not internally synchronized; one owner configures and executes an instance.

## Security non-goals

MWF 0.1.1-beta does not provide namespaces, containers, VM isolation, filesystem policy,
network policy, syscall filtering, credential dropping, secrets isolation, or protection
from a malicious child. These controls require a privileged platform sandbox backend and
must never be inferred from `ResourceLimits`.
