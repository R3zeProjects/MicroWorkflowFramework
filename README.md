# MicroWorkflowFramework

**MicroWorkflowFramework (MWF)** — микро-фреймворк C++23 для запуска ограниченных
процессных рабочих процессов с явным контролем ресурсов операционной системы.

Он предоставляет единый типизированный API для запуска процессов, ограничения памяти и
процессорного времени, ограничения времени выполнения, ограничения числа процессов,
очистки дерева процессов, совместной отмены и последовательного выполнения рабочих
процессов. MWF использует
[MicroContractsFramework](https://github.com/R3zeProjects/MicroContractsFramework) для
заменяемых моделей ошибок и результатов и не требует MicroErrorFramework во время
выполнения.

Текущая версия: **0.1.0-beta**.

## Возможности

- Владеющие значения `ProcessSpec`, `ResourceLimits` и результатов.
- `Runner<ErrorModel>` для одного изолированного дочернего процесса.
- `Workflow<ErrorModel>` для последовательности не более чем из 1024 шагов с уникальными
  именами.
- Политика немедленной остановки или продолжения после ошибки.
- Отмена через `std::stop_token` и ограничение времени выполнения.
- Завершение дерева процессов при превышении времени, отмене и очистке runner.
- Ограничения Windows Job Object для памяти, процессорного времени, активных процессов и
  времени жизни дерева.
- POSIX `setrlimit`, группы процессов и Linux-сигнал о завершении родительского процесса.
- Проверяемые во время компиляции контракты MCF вместо обязательной реализации ошибок.
- Устанавливаемый пакет CMake, compile-fail тесты, санитайзеры, статический анализ и
  smoke-тесты бенчмарка.

## Важная граница изоляции

MWF — фреймворк контроля ресурсов процессов, а не контейнерная среда выполнения. Версия
0.1.0-beta **не** создаёт пространства имён Linux, отдельную корневую файловую систему,
фильтры seccomp, виртуальные сети, Windows AppContainer или границу привилегий. Нельзя
запускать враждебный код, предполагая, что MWF обеспечивает изоляцию безопасности уровня
Docker.

Используйте MWF для ограниченного выполнения и детерминированной очистки доверенных или
частично доверенных worker-процессов. Для изоляции от враждебного кода используйте
контейнер, виртуальную машину или системную песочницу.

## Гарантии платформ

| Возможность | Windows | Linux | macOS/POSIX |
| --- | --- | --- | --- |
| Ограничение памяти | Память процесса в Job Object | `RLIMIT_AS` | `RLIMIT_AS`, если поддерживается |
| Ограничение процессорного времени | Время процесса в Job Object | `RLIMIT_CPU` | `RLIMIT_CPU` |
| Ограничение времени выполнения | Deadline родителя + завершение Job | Deadline родителя + `SIGKILL` | Deadline родителя + `SIGKILL` |
| Ограничение числа процессов | Лимит активных процессов Job | `RLIMIT_NPROC` | `RLIMIT_NPROC`, если поддерживается |
| Очистка дочерних процессов | Завершение при закрытии Job | Завершение группы процессов | Завершение группы процессов |
| Очистка при завершении родителя | Время жизни Job | `PR_SET_PDEATHSIG` + группа процессов | Группа процессов, принадлежащая runner |

В распространённых POSIX-системах `RLIMIT_NPROC` действует на учётную запись и не
эквивалентен PID-контроллеру cgroup. Перед использованием платформенной гарантии вызовите
`vosp::capabilities()` и прочитайте [контракты API](docs/API_CONTRACTS.md).

## Быстрый старт

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

`MyErrorModel` может быть моделью ошибок MEF или любой реализацией, удовлетворяющей
`vosp::contracts::ErrorModel` и требованиям MWF к созданию результатов. Адаптер и
наследование от базового класса не требуются.

## Сборка и тестирование

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMWF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Если `MWF_CONTRACTS_SOURCE_DIR` не задан, CMake сначала ищет `vosp_contracts 0.6`, а затем
при `MWF_FETCH_CONTRACTS=ON` загружает закреплённую совместимую ревизию MCF.

## Измеренная базовая производительность запуска процессов

Опциональный бенчмарк запускает собственный исполняемый файл как дочерний процесс,
ожидает проверенный нулевой код завершения и измеряет полный цикл нативного процесса. Он
не устанавливается вместе с пакетом.

On an Ubuntu 24.04 container using GCC 13.3 Release, seven rounds of 200 launches produced
these medians on the same host and child executable:

| Scenario | Median launches/s | Change from v0.1.0 |
| --- | ---: | ---: |
| v0.1.0 supervised baseline | 181.34 | Baseline |
| v0.1.1 supervised | 205.35 | +13.2% |
| v0.1.1 launch-only | 215.85 | +19.0% |

Создание процесса выполняется операционной системой. Этот показатель является
воспроизводимой базовой линией, а не утверждением, что MWF запускает процессы дешевле
базовой ОС. См. [методику бенчмарка](docs/BENCHMARKS.md).

## Документация

- [Контракты API](docs/API_CONTRACTS.md)
- [Архитектура и принципы работы](docs/ARCHITECTURE.md)
- [Установка](docs/INSTALLATION.md)
- [Примеры использования](docs/USAGE_EXAMPLES.md)
- [Методика бенчмарка](docs/BENCHMARKS.md)

## Структура репозитория

```text
include/vosp/workflow/  Public values, Runner, Workflow, and version API
src/                    Windows and POSIX platform backends
tests/                  Runtime, header, compile-fail, and package tests
examples/               Compilable API example
benchmarks/             Opt-in process round-trip benchmark
docs/                   Contracts, architecture, setup, and operations
```

## Лицензия

Лицензия MIT. См. [LICENSE](LICENSE).
