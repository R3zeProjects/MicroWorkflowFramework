#pragma once

/** @file types.hpp Owning process, resource, and workflow value types. */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vosp::workflow
{
namespace error_code
{
inline constexpr std::uint32_t invalid_specification = 0x5701;
inline constexpr std::uint32_t unsupported_limit = 0x5702;
inline constexpr std::uint32_t launch_failed = 0x5703;
inline constexpr std::uint32_t platform_failure = 0x5704;
} // namespace error_code

/** @brief Why a launched process stopped. */
enum class StopReason : std::uint8_t
{
    exited,
    signaled,
    timed_out,
    cancelled
};

/** @brief Optional operating-system resource limits for one process tree. */
struct ResourceLimits
{
    std::optional<std::uint64_t> memory_bytes;
    std::optional<std::chrono::seconds> cpu_time;
    std::optional<std::chrono::milliseconds> wall_time;
    std::optional<std::uint32_t> process_count;
    bool terminate_descendants{true};
};

/** @brief Owning specification for a child process. */
struct ProcessSpec
{
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> working_directory;
};

/** @brief Observable result of one successfully launched child process. */
struct ProcessResult
{
    std::uint32_t exit_code{};
    StopReason reason{StopReason::exited};
    std::chrono::milliseconds elapsed{};

    /** @return true only for a normal zero exit. */
    [[nodiscard]] constexpr bool succeeded() const noexcept
    {
        return reason == StopReason::exited && exit_code == 0;
    }
};

/** @brief One named workflow step and its resource envelope. */
struct Step
{
    std::string name;
    ProcessSpec process;
    ResourceLimits limits;
};

/** @brief Result associated with one completed workflow step. */
struct StepResult
{
    std::string name;
    ProcessResult process;
};

/** @brief Ordered report produced by a workflow execution. */
struct WorkflowReport
{
    std::vector<StepResult> steps;

    /** @return true when every executed step exited normally with code zero. */
    [[nodiscard]] bool succeeded() const noexcept
    {
        for (const auto &step : steps)
        {
            if (!step.process.succeeded())
            {
                return false;
            }
        }
        return true;
    }
};

/** @brief Execution policy for a bounded workflow. */
struct WorkflowPolicy
{
    static constexpr std::size_t hard_max_steps = 1024;

    std::size_t max_steps{hard_max_steps};
    bool stop_on_failure{true};
};

/** @brief Resource guarantees implemented by the active platform backend. */
struct Capabilities
{
    bool memory_limit{};
    bool cpu_time_limit{};
    bool wall_time_limit{};
    bool process_count_limit{};
    bool process_tree_termination{};
    bool cooperative_cancellation{};
};
} // namespace vosp::workflow
