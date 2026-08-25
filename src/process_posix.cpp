#include <vosp/workflow/detail/backend.hpp>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <spawn.h>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace vosp::workflow::detail
{
namespace
{
[[nodiscard]] NativeError posix_error(std::string operation, int code = errno)
{
    operation += ": ";
    operation += std::strerror(code);
    return NativeError{error_code::platform_failure, std::move(operation)};
}

struct ChildFailure
{
    int native_code;
    std::uint32_t framework_code;
};

[[noreturn]] void child_failure(int descriptor, int native_code,
                                std::uint32_t framework_code) noexcept
{
    const ChildFailure failure{native_code, framework_code};
    const auto *data = reinterpret_cast<const char *>(&failure);
    std::size_t remaining = sizeof(failure);
    while (remaining > 0)
    {
        const ssize_t written = ::write(descriptor, data, remaining);
        if (written > 0)
        {
            data += written;
            remaining -= static_cast<std::size_t>(written);
        }
        else if (written < 0 && errno != EINTR)
        {
            break;
        }
    }
    _exit(127);
}

[[nodiscard]] bool set_limit(int resource, std::uint64_t value) noexcept
{
    const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<rlim_t>::max());
    if (value > maximum)
    {
        errno = EOVERFLOW;
        return false;
    }
    const rlimit limit{static_cast<rlim_t>(value), static_cast<rlim_t>(value)};
    return ::setrlimit(resource, &limit) == 0;
}

void terminate_process(pid_t process, bool descendants) noexcept
{
    if (descendants)
    {
        static_cast<void>(::kill(-process, SIGKILL));
    }
    else
    {
        static_cast<void>(::kill(process, SIGKILL));
    }
}

[[nodiscard]] pid_t wait_for_process(pid_t process, int &status, int options) noexcept
{
    pid_t waited = 0;
    do
    {
        waited = ::waitpid(process, &status, options);
    } while (waited < 0 && errno == EINTR);
    return waited;
}

[[nodiscard]] std::expected<ProcessResult, NativeError>
complete_process(pid_t process, const ResourceLimits &limits, const std::stop_token &stop_token,
                 std::chrono::steady_clock::time_point started)
{
    using clock = std::chrono::steady_clock;
    StopReason reason = StopReason::exited;
    int status = 0;
    if (!limits.wall_time && !stop_token.stop_possible())
    {
        if (wait_for_process(process, status, 0) < 0)
        {
            return std::unexpected{posix_error("waitpid")};
        }
    }
    else
    {
        while (true)
        {
            const pid_t waited = wait_for_process(process, status, WNOHANG);
            if (waited == process)
            {
                break;
            }
            if (waited < 0)
            {
                return std::unexpected{posix_error("waitpid")};
            }
            if (stop_token.stop_requested())
            {
                reason = StopReason::cancelled;
                terminate_process(process, limits.terminate_descendants);
                static_cast<void>(wait_for_process(process, status, 0));
                break;
            }
            if (limits.wall_time && clock::now() - started >= *limits.wall_time)
            {
                reason = StopReason::timed_out;
                terminate_process(process, limits.terminate_descendants);
                static_cast<void>(wait_for_process(process, status, 0));
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    }

    std::uint32_t exit_code = 1;
    if (WIFEXITED(status))
    {
        exit_code = static_cast<std::uint32_t>(WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        exit_code = static_cast<std::uint32_t>(128 + WTERMSIG(status));
        if (reason == StopReason::exited)
        {
            reason = StopReason::signaled;
        }
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - started);
    return ProcessResult{exit_code, reason, elapsed};
}
} // namespace

Capabilities platform_capabilities() noexcept
{
#if defined(RLIMIT_NPROC)
    constexpr bool process_count_limit = true;
#else
    constexpr bool process_count_limit = false;
#endif
    return Capabilities{true, true, true, process_count_limit, true, true};
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

    const std::string executable = specification.executable.string();
    const std::optional<std::string> working_directory =
        specification.working_directory ? std::optional{specification.working_directory->string()}
                                        : std::nullopt;
    std::vector<std::string> owned_arguments;
    owned_arguments.reserve(specification.arguments.size() + 1);
    owned_arguments.push_back(executable);
    owned_arguments.insert(owned_arguments.end(), specification.arguments.begin(),
                           specification.arguments.end());
    std::vector<char *> arguments;
    arguments.reserve(owned_arguments.size() + 1);
    for (auto &argument : owned_arguments)
    {
        arguments.push_back(argument.data());
    }
    arguments.push_back(nullptr);

    const bool can_use_posix_spawn = !limits.terminate_descendants && !limits.memory_bytes &&
                                     !limits.cpu_time && !limits.process_count &&
                                     !working_directory;
    if (can_use_posix_spawn)
    {
        pid_t process = 0;
        const int error = ::posix_spawnp(&process, executable.c_str(), nullptr, nullptr,
                                         arguments.data(), environ);
        if (error != 0)
        {
            auto failure = posix_error("posix_spawnp", error);
            failure.code = error_code::launch_failed;
            return std::unexpected{std::move(failure)};
        }
        return complete_process(process, limits, stop_token, started);
    }

    int error_pipe[2]{};
    if (::pipe(error_pipe) != 0)
    {
        return std::unexpected{posix_error("pipe")};
    }
    if (::fcntl(error_pipe[1], F_SETFD, FD_CLOEXEC) == -1)
    {
        const auto error = posix_error("fcntl");
        ::close(error_pipe[0]);
        ::close(error_pipe[1]);
        return std::unexpected{error};
    }

    const pid_t process = ::fork();
    if (process < 0)
    {
        const auto error = posix_error("fork");
        ::close(error_pipe[0]);
        ::close(error_pipe[1]);
        return std::unexpected{error};
    }

    if (process == 0)
    {
        ::close(error_pipe[0]);
        if (limits.terminate_descendants && ::setpgid(0, 0) != 0)
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
#if defined(__linux__)
        if (limits.terminate_descendants && ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0)
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
        if (limits.terminate_descendants && ::getppid() == 1)
        {
            child_failure(error_pipe[1], ECANCELED, error_code::platform_failure);
        }
#endif
        if (working_directory && ::chdir(working_directory->c_str()) != 0)
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
        if (limits.memory_bytes && !set_limit(RLIMIT_AS, *limits.memory_bytes))
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
        if (limits.cpu_time &&
            !set_limit(RLIMIT_CPU, static_cast<std::uint64_t>(limits.cpu_time->count())))
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
#if defined(RLIMIT_NPROC)
        if (limits.process_count && !set_limit(RLIMIT_NPROC, *limits.process_count))
        {
            child_failure(error_pipe[1], errno, error_code::platform_failure);
        }
#else
        if (limits.process_count)
        {
            child_failure(error_pipe[1], ENOTSUP, error_code::unsupported_limit);
        }
#endif

        ::execvp(executable.c_str(), arguments.data());
        child_failure(error_pipe[1], errno, error_code::launch_failed);
    }

    ::close(error_pipe[1]);
    ChildFailure child_error{};
    ssize_t read_size = 0;
    do
    {
        read_size = ::read(error_pipe[0], &child_error, sizeof(child_error));
    } while (read_size < 0 && errno == EINTR);
    ::close(error_pipe[0]);
    if (read_size > 0)
    {
        int status = 0;
        static_cast<void>(wait_for_process(process, status, 0));
        auto failure = posix_error("child setup or exec", child_error.native_code);
        failure.code = child_error.framework_code;
        return std::unexpected{std::move(failure)};
    }
    if (read_size < 0)
    {
        const int read_error = errno;
        terminate_process(process, limits.terminate_descendants);
        int status = 0;
        static_cast<void>(wait_for_process(process, status, 0));
        return std::unexpected{posix_error("read launch status", read_error)};
    }

    return complete_process(process, limits, stop_token, started);
}
} // namespace vosp::workflow::detail
