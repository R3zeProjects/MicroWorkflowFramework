#include <vosp/workflow/detail/backend.hpp>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
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

[[noreturn]] void child_failure(int descriptor, int code) noexcept
{
    const auto ignored = ::write(descriptor, &code, sizeof(code));
    static_cast<void>(ignored);
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
            child_failure(error_pipe[1], errno);
        }
#if defined(__linux__)
        if (limits.terminate_descendants && ::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0)
        {
            child_failure(error_pipe[1], errno);
        }
        if (limits.terminate_descendants && ::getppid() == 1)
        {
            child_failure(error_pipe[1], ECANCELED);
        }
#endif
        if (working_directory && ::chdir(working_directory->c_str()) != 0)
        {
            child_failure(error_pipe[1], errno);
        }
        if (limits.memory_bytes && !set_limit(RLIMIT_AS, *limits.memory_bytes))
        {
            child_failure(error_pipe[1], errno);
        }
        if (limits.cpu_time &&
            !set_limit(RLIMIT_CPU, static_cast<std::uint64_t>(limits.cpu_time->count())))
        {
            child_failure(error_pipe[1], errno);
        }
#if defined(RLIMIT_NPROC)
        if (limits.process_count && !set_limit(RLIMIT_NPROC, *limits.process_count))
        {
            child_failure(error_pipe[1], errno);
        }
#else
        if (limits.process_count)
        {
            child_failure(error_pipe[1], ENOTSUP);
        }
#endif

        ::execvp(executable.c_str(), arguments.data());
        child_failure(error_pipe[1], errno);
    }

    ::close(error_pipe[1]);
    int child_error = 0;
    ssize_t read_size = 0;
    do
    {
        read_size = ::read(error_pipe[0], &child_error, sizeof(child_error));
    } while (read_size < 0 && errno == EINTR);
    ::close(error_pipe[0]);
    if (read_size > 0)
    {
        int status = 0;
        static_cast<void>(::waitpid(process, &status, 0));
        return std::unexpected{posix_error("child setup or exec", child_error)};
    }
    if (read_size < 0)
    {
        terminate_process(process, limits.terminate_descendants);
        int status = 0;
        static_cast<void>(::waitpid(process, &status, 0));
        return std::unexpected{posix_error("read launch status")};
    }

    StopReason reason = StopReason::exited;
    int status = 0;
    while (true)
    {
        const pid_t waited = ::waitpid(process, &status, WNOHANG);
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
            static_cast<void>(::waitpid(process, &status, 0));
            break;
        }
        if (limits.wall_time && clock::now() - started >= *limits.wall_time)
        {
            reason = StopReason::timed_out;
            terminate_process(process, limits.terminate_descendants);
            static_cast<void>(::waitpid(process, &status, 0));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
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
} // namespace vosp::workflow::detail
