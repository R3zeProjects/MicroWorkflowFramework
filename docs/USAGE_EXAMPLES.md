# Примеры использования

## Модель ошибок

MWF принимает любую прямую реализацию, удовлетворяющую контракту MCF:

```cpp
struct AppModel {
    using Error = AppError;

    template <typename T>
    using Result = std::expected<T, Error>;

    using OperationResult = Result<void>;

    static Error make_error(std::uint32_t code, std::string message);
};
```

Конкретная модель может предоставляться MEF. MWF не требует адаптера, поскольку напрямую
использует типы, соответствующие контракту.

## Запуск одного ограниченного процесса

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

## Отмена выполняющегося процесса

```cpp
std::stop_source source;

std::jthread watchdog([&source] {
    if (service_is_shutting_down()) {
        source.request_stop();
    }
});

auto result = runner.run(process, limits, source.get_token());
```

Вызов возвращается только после завершения и сбора принадлежащего runner процесса.

## Использование быстрого пути без управления деревом

```cpp
vosp::ResourceLimits launch_only;
launch_only.terminate_descendants = false;

auto result = runner.run(process, launch_only);
```

Такая политика подходит только тогда, когда исполняемый файл не может создать дочерние
процессы, которые должен очищать MWF. При отсутствии управления ресурсами перед exec и
смены рабочего каталога она разрешает более дешёвый нативный путь запуска. Отмена и
deadline по-прежнему завершают и собирают непосредственный дочерний процесс, но не
независимо работающих потомков.

## Создание ограниченного рабочего процесса

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

Имена уникальны, порядок выполнения стабилен, а ёмкость жёстко ограничена 1024 шагами,
даже если в политике задано большее значение.

## Продолжение после ошибки дочернего процесса

```cpp
vosp::ProcessWorkflow<AppModel> workflow({
    .max_steps = 16,
    .stop_on_failure = false
});
```

Ошибки запуска фреймворка по-прежнему останавливают выполнение, поскольку для такого шага
не существует `ProcessResult`. Ненулевые коды, сигналы, превышения времени и отмены
записываются как результаты процессов.

## Проверка поддержки платформы

```cpp
const auto support = vosp::capabilities();
if (!support.process_count_limit) {
    refuse_configuration("process-count isolation is required");
}
```

Проверка возможностей сообщает о наличии реализации. Нативная политика всё ещё может
отклонить конкретный запуск, что будет возвращено как типизированная ошибка фреймворка.
