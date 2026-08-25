#include <vosp/workflow.hpp>

#include "test_model.hpp"

#include <chrono>
#include <filesystem>
#include <new>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace std::chrono_literals;

#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            return __LINE__;                                                                       \
        }                                                                                          \
    } while (false)

int child_main(int argc, char **argv)
{
    const std::string mode = argc > 2 ? argv[2] : "";
    if (mode == "exit")
    {
        return argc > 3 ? std::stoi(argv[3]) : 0;
    }
    if (mode == "sleep")
    {
        const auto duration = argc > 3 ? std::stoi(argv[3]) : 100;
        std::this_thread::sleep_for(std::chrono::milliseconds{duration});
        return 0;
    }
    if (mode == "allocate")
    {
        const auto bytes = argc > 3 ? std::stoull(argv[3]) : 0;
        try
        {
            std::vector<std::byte> memory(static_cast<std::size_t>(bytes));
            for (std::size_t index = 0; index < memory.size(); index += 4096)
            {
                memory[index] = std::byte{1};
            }
            return memory.empty() || memory.front() != std::byte{1} ? 34 : 0;
        }
        catch (const std::bad_alloc &)
        {
            return 33;
        }
    }
    return 2;
}

vosp::ProcessSpec child(const std::filesystem::path &executable, std::string mode,
                        std::string value)
{
    return vosp::ProcessSpec{
        executable, {"--mwf-child", std::move(mode), std::move(value)}, std::nullopt};
}
} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::string{argv[1]} == "--mwf-child")
    {
        return child_main(argc, argv);
    }

    const auto executable = std::filesystem::absolute(argv[0]);
    vosp::ProcessRunner<TestErrorModel> runner;

    const auto success = runner.run(child(executable, "exit", "0"));
    CHECK(success);
    CHECK(success->succeeded());

    const auto nonzero = runner.run(child(executable, "exit", "7"));
    CHECK(nonzero);
    CHECK(nonzero->reason == vosp::StopReason::exited);
    CHECK(nonzero->exit_code == 7);
    CHECK(!nonzero->succeeded());

    vosp::ResourceLimits timeout_limits;
    timeout_limits.wall_time = 30ms;
    const auto timeout = runner.run(child(executable, "sleep", "500"), timeout_limits);
    CHECK(timeout);
    CHECK(timeout->reason == vosp::StopReason::timed_out);

    vosp::ResourceLimits memory_limits;
    memory_limits.memory_bytes = 128ULL * 1024 * 1024;
    memory_limits.wall_time = 5s;
    const auto memory_limited =
        runner.run(child(executable, "allocate", "536870912"), memory_limits);
    CHECK(memory_limited);
    CHECK(!memory_limited->succeeded());

    std::stop_source source;
    std::jthread canceller{[&source]
                           {
                               std::this_thread::sleep_for(30ms);
                               source.request_stop();
                           }};
    const auto cancelled = runner.run(child(executable, "sleep", "500"), {}, source.get_token());
    CHECK(cancelled);
    CHECK(cancelled->reason == vosp::StopReason::cancelled);

    const auto invalid = runner.run({});
    CHECK(!invalid);
    CHECK(invalid.error().code() == vosp::workflow::error_code::invalid_specification);

    vosp::ResourceLimits invalid_limits;
    invalid_limits.process_count = 0;
    const auto invalid_limit = runner.run(child(executable, "exit", "0"), invalid_limits);
    CHECK(!invalid_limit);
    CHECK(invalid_limit.error().code() == vosp::workflow::error_code::invalid_specification);

    const auto missing = runner.run({executable.parent_path() / "mwf-definitely-missing", {}, {}});
    CHECK(!missing);
    CHECK(missing.error().code() == vosp::workflow::error_code::launch_failed);

    vosp::ProcessWorkflow<TestErrorModel> workflow;
    CHECK(workflow.add({"prepare", child(executable, "exit", "0"), {}}));
    CHECK(workflow.add({"fail", child(executable, "exit", "9"), {}}));
    CHECK(workflow.add({"skipped", child(executable, "exit", "0"), {}}));
    CHECK(!workflow.add({"fail", child(executable, "exit", "0"), {}}));
    const auto report = workflow.run();
    CHECK(report);
    CHECK(report->steps.size() == 2);
    CHECK(!report->succeeded());

    vosp::ProcessWorkflow<TestErrorModel> empty_workflow{{.max_steps = 0}};
    CHECK(!empty_workflow.add({"rejected", child(executable, "exit", "0"), {}}));

    const auto capabilities = vosp::ProcessRunner<TestErrorModel>::capabilities();
    CHECK(capabilities.wall_time_limit);
    CHECK(capabilities.cooperative_cancellation);
}
