# Установка

## Требования

- CMake 3.25 или новее.
- Компилятор и стандартная библиотека C++23 с `std::expected` и `std::stop_token`.
- Windows 10 или новее, Linux либо POSIX-платформа с `fork`, `exec` и `setrlimit`.
- MicroContractsFramework 0.6.

## Сборка из дерева исходного кода

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DMWF_CONTRACTS_SOURCE_DIR=/path/to/MicroContractsFramework
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

## Установленный MCF или загрузка закреплённой ревизии

MWF разрешает зависимости в следующем порядке:

1. Существующая цель CMake `vosp::contracts`.
2. `MWF_CONTRACTS_SOURCE_DIR`.
3. Установленный пакет `vosp_contracts 0.6`.
4. Закреплённая Git-ревизия при `MWF_FETCH_CONTRACTS=ON`.

Для воспроизводимых офлайн-сборок отключите сетевой fallback с помощью
`-DMWF_FETCH_CONTRACTS=OFF`.

## Установка и подключение

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

Установленный пакет содержит публичные заголовочные файлы, одну нативную статическую
библиотеку, экспортируемые файлы CMake и лицензию. Тесты, примеры, бенчмарки и каталоги
сборки не устанавливаются.

## Параметры разработки

| Параметр | По умолчанию | Назначение |
| --- | ---: | --- |
| `BUILD_TESTING` | Значение CTest | Runtime-, header- и compile-fail тесты. |
| `MWF_BUILD_EXAMPLES` | `ON` | Собрать базовый исполняемый пример. |
| `MWF_BUILD_BENCHMARKS` | `OFF` | Собрать опциональный бенчмарк процессов. |
| `MWF_BUILD_EXTERNAL_COMPARISON_BENCHMARKS` | `OFF` | Собрать сравнение с Boost.Process и libuv, если доступны оба пакета. |
| `MWF_BUILD_MEF_INTEGRATION` | `OFF` | Собрать прямую интеграцию с закреплённой ревизией MEF. |
| `MWF_ENABLE_SANITIZERS` | `OFF` | Включить ASan и UBSan там, где они поддерживаются. |
| `MWF_ENABLE_THREAD_SANITIZER` | `OFF` | Включить TSan в отдельной сборке. |

## Устранение неполадок

- Ошибка запуска означает, что результата пользовательского процесса нет; проверьте
  сообщение типизированной ошибки.
- Ненулевой код завершения означает, что запуск состоялся, а дочерний процесс сообщил об
  ошибке.
- Если нативное ограничение отклонено, проверьте привилегии учётной записи и политику
  хоста вместо незаметного отключения ограничения.
- Среды выполнения санитайзеров резервируют большие диапазоны виртуальных адресов;
  слишком малый лимит памяти может помешать запуску инструментированного процесса.
