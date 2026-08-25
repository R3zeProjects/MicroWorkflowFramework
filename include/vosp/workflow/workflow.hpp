#pragma once

/** @file workflow.hpp Bounded sequential workflow composition. */

#include <vosp/workflow/runner.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <stop_token>
#include <utility>
#include <vector>

namespace vosp::workflow
{
/** @brief Bounded ordered process workflow with fail-fast policy. */
template <WorkflowErrorModel Model> class Workflow
{
  public:
    using result_type = typename Model::template Result<WorkflowReport>;

    explicit Workflow(WorkflowPolicy policy = {}) : policy_{policy}
    {
        policy_.max_steps = std::min(policy_.max_steps, WorkflowPolicy::hard_max_steps);
        steps_.reserve(policy_.max_steps);
    }

    /** @return false for an invalid, duplicate, or over-capacity step. */
    [[nodiscard]] bool add(Step step)
    {
        if (step.name.empty() || step.process.executable.empty() ||
            steps_.size() >= policy_.max_steps)
        {
            return false;
        }
        const auto duplicate = std::ranges::any_of(steps_, [&](const Step &current)
                                                   { return current.name == step.name; });
        if (duplicate)
        {
            return false;
        }
        steps_.push_back(std::move(step));
        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept { return steps_.size(); }
    [[nodiscard]] bool empty() const noexcept { return steps_.empty(); }
    void clear() noexcept { steps_.clear(); }

    /** @brief Execute steps in insertion order using one shared cancellation token. */
    [[nodiscard]] result_type run(const std::stop_token &stop_token = {}) const
    {
        WorkflowReport report;
        report.steps.reserve(steps_.size());

        for (const auto &step : steps_)
        {
            auto process = runner_.run(step.process, step.limits, stop_token);
            if (!process)
            {
                return result_type{std::unexpected{process.error()}};
            }

            report.steps.push_back(StepResult{step.name, std::move(*process)});
            if (policy_.stop_on_failure && !report.steps.back().process.succeeded())
            {
                break;
            }
        }
        return result_type{std::move(report)};
    }

  private:
    WorkflowPolicy policy_;
    std::vector<Step> steps_;
    Runner<Model> runner_;
};
} // namespace vosp::workflow
