#pragma once

/** @file runner.hpp Typed process runner backed by native resource controls. */

#include <vosp/contracts/error.hpp>
#include <vosp/workflow/detail/backend.hpp>

#include <concepts>
#include <expected>
#include <stop_token>
#include <string>
#include <utility>

namespace vosp::workflow
{
/** @return Resource controls implemented by the active platform backend. */
[[nodiscard]] inline Capabilities capabilities() noexcept
{
    return detail::platform_capabilities();
}

/** @brief Error model requirements needed by the workflow result types. */
template <typename Model>
concept WorkflowErrorModel = vosp::contracts::ErrorModel<Model> && requires(ProcessResult process,
                                                                            WorkflowReport report,
                                                                            typename Model::Error
                                                                                error) {
    typename Model::template Result<ProcessResult>;
    typename Model::template Result<WorkflowReport>;
    requires std::constructible_from<typename Model::template Result<ProcessResult>, ProcessResult>;
    requires std::constructible_from<typename Model::template Result<WorkflowReport>,
                                     WorkflowReport>;
    requires std::constructible_from<typename Model::template Result<ProcessResult>,
                                     std::unexpected<typename Model::Error>>;
    requires std::constructible_from<typename Model::template Result<WorkflowReport>,
                                     std::unexpected<typename Model::Error>>;
};

/** @brief Executes one child process under an explicit resource envelope. */
template <WorkflowErrorModel Model> class Runner
{
  public:
    using result_type = typename Model::template Result<ProcessResult>;

    /** @return Resource controls implemented by the active backend. */
    [[nodiscard]] static Capabilities capabilities() noexcept { return workflow::capabilities(); }

    /**
     * @brief Launch and wait for one process.
     * @param specification Owning executable, arguments, and working directory.
     * @param limits Resource envelope enforced before user code runs where supported.
     * @param stop_token Cooperative cancellation signal observed by the parent.
     */
    [[nodiscard]] result_type run(const ProcessSpec &specification,
                                  const ResourceLimits &limits = {},
                                  const std::stop_token &stop_token = {}) const
    {
        if (specification.executable.empty())
        {
            return failure(error_code::invalid_specification,
                           "process executable must not be empty");
        }
        if ((limits.memory_bytes && *limits.memory_bytes == 0) ||
            (limits.cpu_time && *limits.cpu_time <= std::chrono::seconds::zero()) ||
            (limits.wall_time && *limits.wall_time <= std::chrono::milliseconds::zero()) ||
            (limits.process_count && *limits.process_count == 0))
        {
            return failure(error_code::invalid_specification,
                           "resource limits must be greater than zero");
        }

        auto native = detail::run_process(specification, limits, stop_token);
        if (!native)
        {
            return failure(native.error().code, std::move(native.error().message));
        }
        return result_type{*native};
    }

  private:
    [[nodiscard]] static result_type failure(std::uint32_t code, std::string message)
    {
        return result_type{std::unexpected{Model::make_error(code, std::move(message))}};
    }
};
} // namespace vosp::workflow
