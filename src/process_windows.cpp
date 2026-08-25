#include <vosp/workflow/detail/backend.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vosp::workflow::detail
{
namespace
{
class Handle
{
  public:
    Handle() = default;
    explicit Handle(HANDLE value) noexcept : value_{value} {}
    ~Handle()
    {
        if (value_ && value_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(value_);
        }
    }

    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    Handle(Handle &&other) noexcept : value_{std::exchange(other.value_, nullptr)} {}
    Handle &operator=(Handle &&other) noexcept
    {
        if (this != &other)
        {
            Handle temporary{std::move(other)};
            std::swap(value_, temporary.value_);
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return value_ && value_ != INVALID_HANDLE_VALUE;
    }

  private:
    HANDLE value_{};
};

[[nodiscard]] NativeError windows_error(std::uint32_t code, std::string_view operation,
                                        std::uint32_t framework_code = error_code::platform_failure)
{
    LPWSTR buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::string message{operation};
    message += " failed";
    if (length != 0 && buffer)
    {
        const int required = WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length),
                                                 nullptr, 0, nullptr, nullptr);
        if (required > 0)
        {
            std::string detail(static_cast<std::size_t>(required), '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer, static_cast<int>(length), detail.data(),
                                required, nullptr, nullptr);
            message += ": ";
            message += detail;
        }
        LocalFree(buffer);
    }
    return NativeError{framework_code, std::move(message)};
}

[[nodiscard]] std::expected<std::wstring, NativeError> utf8_to_wide(std::string_view text)
{
    if (text.empty())
    {
        return std::wstring{};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return std::unexpected{
            NativeError{error_code::invalid_specification, "argument is too large"}};
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0)
    {
        return std::unexpected{windows_error(GetLastError(), "UTF-8 conversion")};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                        result.data(), size);
    return result;
}

[[nodiscard]] std::wstring quote_argument(std::wstring_view argument)
{
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos)
    {
        return std::wstring{argument};
    }

    std::wstring quoted{L'\"'};
    std::size_t backslashes = 0;
    for (const wchar_t character : argument)
    {
        if (character == L'\\')
        {
            ++backslashes;
            continue;
        }
        if (character == L'\"')
        {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(character);
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

[[nodiscard]] std::expected<std::wstring, NativeError>
command_line(const ProcessSpec &specification)
{
    std::wstring command = quote_argument(specification.executable.wstring());
    for (const auto &argument : specification.arguments)
    {
        auto wide = utf8_to_wide(argument);
        if (!wide)
        {
            return std::unexpected{std::move(wide.error())};
        }
        command.push_back(L' ');
        command += quote_argument(*wide);
    }
    return command;
}

void terminate_owned_process(HANDLE job, HANDLE process, bool descendants,
                             std::uint32_t exit_code) noexcept
{
    if (descendants)
    {
        static_cast<void>(TerminateJobObject(job, exit_code));
    }
    else
    {
        static_cast<void>(TerminateProcess(process, exit_code));
    }
}

[[nodiscard]] DWORD wait_timeout(std::chrono::steady_clock::time_point started,
                                 const std::optional<std::chrono::milliseconds> &wall_time)
{
    if (!wall_time)
    {
        return INFINITE;
    }
    const auto remaining = started + *wall_time - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::steady_clock::duration::zero())
    {
        return 0;
    }
    const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
    return static_cast<DWORD>(
        std::min<std::int64_t>(milliseconds, static_cast<std::int64_t>(INFINITE) - 1));
}
} // namespace

Capabilities platform_capabilities() noexcept
{
    return Capabilities{true, true, true, true, true, true};
}

std::expected<ProcessResult, NativeError> run_process(const ProcessSpec &specification,
                                                      const ResourceLimits &limits,
                                                      const std::stop_token &stop_token)
{
    using clock = std::chrono::steady_clock;
    const auto started = clock::now();
    if (stop_token.stop_requested())
    {
        return ProcessResult{1, StopReason::cancelled, std::chrono::milliseconds::zero()};
    }

    auto command = command_line(specification);
    if (!command)
    {
        return std::unexpected{std::move(command.error())};
    }
    std::vector<wchar_t> mutable_command(command->begin(), command->end());
    mutable_command.push_back(L'\0');

    const bool requires_job = limits.terminate_descendants || limits.memory_bytes ||
                              limits.cpu_time || limits.process_count;
    Handle job;
    if (requires_job)
    {
        job = Handle{CreateJobObjectW(nullptr, nullptr)};
        if (!job)
        {
            return std::unexpected{windows_error(GetLastError(), "CreateJobObjectW")};
        }
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
    if (limits.terminate_descendants)
    {
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    }
    if (limits.memory_bytes)
    {
        if (*limits.memory_bytes > std::numeric_limits<SIZE_T>::max())
        {
            return std::unexpected{NativeError{error_code::invalid_specification,
                                               "memory limit exceeds platform range"}};
        }
        job_limits.ProcessMemoryLimit = static_cast<SIZE_T>(*limits.memory_bytes);
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    }
    if (limits.cpu_time)
    {
        constexpr auto ticks_per_second = 10'000'000LL;
        if (limits.cpu_time->count() > std::numeric_limits<LONGLONG>::max() / ticks_per_second)
        {
            return std::unexpected{NativeError{error_code::invalid_specification,
                                               "CPU-time limit exceeds platform range"}};
        }
        job_limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart =
            limits.cpu_time->count() * ticks_per_second;
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_TIME;
    }
    if (limits.process_count)
    {
        job_limits.BasicLimitInformation.ActiveProcessLimit = *limits.process_count;
        job_limits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    }
    if (requires_job && !SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation,
                                                 &job_limits, sizeof(job_limits)))
    {
        return std::unexpected{windows_error(GetLastError(), "SetInformationJobObject")};
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION information{};
    const wchar_t *working_directory =
        specification.working_directory ? specification.working_directory->c_str() : nullptr;
    const DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT | (requires_job ? CREATE_SUSPENDED : 0);
    if (!CreateProcessW(specification.executable.c_str(), mutable_command.data(), nullptr, nullptr,
                        FALSE, creation_flags, nullptr, working_directory, &startup, &information))
    {
        return std::unexpected{
            windows_error(GetLastError(), "CreateProcessW", error_code::launch_failed)};
    }

    Handle process{information.hProcess};
    Handle thread{information.hThread};
    if (requires_job && !AssignProcessToJobObject(job.get(), process.get()))
    {
        const DWORD error = GetLastError();
        terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
        WaitForSingleObject(process.get(), INFINITE);
        return std::unexpected{windows_error(error, "AssignProcessToJobObject")};
    }
    if (requires_job && ResumeThread(thread.get()) == static_cast<DWORD>(-1))
    {
        const DWORD error = GetLastError();
        terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
        WaitForSingleObject(process.get(), INFINITE);
        return std::unexpected{windows_error(error, "ResumeThread")};
    }

    Handle cancellation_event;
    if (stop_token.stop_possible())
    {
        cancellation_event = Handle{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        if (!cancellation_event)
        {
            const DWORD error = GetLastError();
            terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
            WaitForSingleObject(process.get(), INFINITE);
            return std::unexpected{windows_error(error, "CreateEventW")};
        }
    }
    const auto request_cancellation = [event = cancellation_event.get()]() noexcept
    {
        if (event)
        {
            static_cast<void>(SetEvent(event));
        }
    };
    std::stop_callback cancellation{stop_token, request_cancellation};

    StopReason reason = StopReason::exited;
    while (true)
    {
        const DWORD timeout = wait_timeout(started, limits.wall_time);
        const HANDLE handles[]{process.get(), cancellation_event.get()};
        const DWORD wait_result = cancellation_event
                                      ? WaitForMultipleObjects(2, handles, FALSE, timeout)
                                      : WaitForSingleObject(process.get(), timeout);
        if (wait_result == WAIT_OBJECT_0)
        {
            break;
        }
        if (wait_result == WAIT_FAILED)
        {
            const DWORD error = GetLastError();
            terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
            WaitForSingleObject(process.get(), INFINITE);
            return std::unexpected{windows_error(error, "process wait")};
        }
        if (cancellation_event && wait_result == WAIT_OBJECT_0 + 1)
        {
            reason = StopReason::cancelled;
            terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
            WaitForSingleObject(process.get(), INFINITE);
            break;
        }
        if (wait_result == WAIT_TIMEOUT && limits.wall_time &&
            clock::now() - started >= *limits.wall_time)
        {
            reason = StopReason::timed_out;
            terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
            WaitForSingleObject(process.get(), INFINITE);
            break;
        }
        if (wait_result == WAIT_TIMEOUT)
        {
            continue;
        }
        terminate_owned_process(job.get(), process.get(), limits.terminate_descendants, 1);
        WaitForSingleObject(process.get(), INFINITE);
        return std::unexpected{
            NativeError{error_code::platform_failure, "process wait returned an invalid state"}};
    }

    DWORD exit_code = 1;
    if (!GetExitCodeProcess(process.get(), &exit_code))
    {
        return std::unexpected{windows_error(GetLastError(), "GetExitCodeProcess")};
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - started);
    return ProcessResult{exit_code, reason, elapsed};
}
} // namespace vosp::workflow::detail
