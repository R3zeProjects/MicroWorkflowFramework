# Usage examples

## Error model

MWF accepts any direct implementation satisfying the MCF contract:

```cpp
struct AppModel {
    using Error = AppError;

    template <typename T>
    using Result = std::expected<T, Error>;

    using OperationResult = Result<void>;

    static Error make_error(std::uint32_t code, std::string message);
};
```

The concrete model may come from MEF. MWF does not need an adapter because it consumes the
contracted types directly.

## Run one bounded process

```cpp
vosp::ProcessRunner<AppModel> runner;

vosp::ProcessSpec process{
    .executable = "thumbnail-worker",
    .arguments = {"--input", "photo.raw"},
    .working_directory = "/srv/jobs/42"
};

vosp::ResourceLimits limits{
    .memory_bytes = 512ULL * 1024 * 1024,
    .cpu_time = std::chrono::seconds{30},
    .wall_time = std::chrono::seconds{45},
    .process_count = 2,
    .terminate_descendants = true
};

auto result = runner.run(process, limits);
if (!result) {
    report_framework_failure(result.error());
} else if (!result->succeeded()) {
    report_child_exit(result->exit_code, result->reason);
}
```

## Cancel a running process

```cpp
std::stop_source source;

std::jthread watchdog([&source] {
    if (service_is_shutting_down()) {
        source.request_stop();
    }
});

auto result = runner.run(process, limits, source.get_token());
```

The call returns only after the owned process has terminated and been reaped.

## Use the launch-only fast path

```cpp
vosp::ResourceLimits launch_only;
launch_only.terminate_descendants = false;

auto result = runner.run(process, launch_only);
```

This policy is appropriate only when the executable cannot create descendants that MWF
must clean up. With no pre-exec resource controls or working-directory change, it permits a
cheaper native launch path. Cancellation and wall deadlines still terminate and reap the
direct child, but not independently running descendants.

## Build a bounded workflow

```cpp
vosp::WorkflowPolicy policy{
    .max_steps = 64,
    .stop_on_failure = true
};

vosp::ProcessWorkflow<AppModel> workflow{policy};
workflow.add({"download", download_spec, network_worker_limits});
workflow.add({"transform", transform_spec, cpu_worker_limits});
workflow.add({"publish", publish_spec, publisher_limits});

auto report = workflow.run(stop_token);
```

Names are unique, execution order is stable, and capacity is hard-bounded at 1024 even if a
larger policy value is supplied.

## Continue after child failure

```cpp
vosp::ProcessWorkflow<AppModel> workflow({
    .max_steps = 16,
    .stop_on_failure = false
});
```

Framework launch errors still stop execution because no `ProcessResult` exists for that
step. Nonzero exits, signals, timeouts, and cancellations are recorded as process results.

## Inspect platform support

```cpp
const auto support = vosp::capabilities();
if (!support.process_count_limit) {
    refuse_configuration("process-count isolation is required");
}
```

Capability discovery reports implementation availability. Native policy can still reject a
specific launch, which is returned as a typed framework error.
