#include <vosp/workflow.hpp>

#include "test_model.hpp"

#include <cassert>
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
    assert(success);
    assert(success->succeeded());

    const auto nonzero = runner.run(child(executable, "exit", "7"));
    assert(nonzero);
    assert(nonzero->reason == vosp::StopReason::exited);
    assert(nonzero->exit_code == 7);
    assert(!nonzero->succeeded());

    vosp::ResourceLimits timeout_limits;
    timeout_limits.wall_time = 30ms;
    const auto timeout = runner.run(child(executable, "sleep", "500"), timeout_limits);
    assert(timeout);
    assert(timeout->reason == vosp::StopReason::timed_out);

    vosp::ResourceLimits memory_limits;
    memory_limits.memory_bytes = 128ULL * 1024 * 1024;
    memory_limits.wall_time = 5s;
    const auto memory_limited =
        runner.run(child(executable, "allocate", "536870912"), memory_limits);
    assert(memory_limited);
    assert(!memory_limited->succeeded());

    std::stop_source source;
    std::jthread canceller{[&source]
                           {
                               std::this_thread::sleep_for(30ms);
                               source.request_stop();
                           }};
    const auto cancelled = runner.run(child(executable, "sleep", "500"), {}, source.get_token());
    assert(cancelled);
    assert(cancelled->reason == vosp::StopReason::cancelled);

    const auto invalid = runner.run({});
    assert(!invalid);
    assert(invalid.error().code() == vosp::workflow::error_code::invalid_specification);

    vosp::ProcessWorkflow<TestErrorModel> workflow;
    assert(workflow.add({"prepare", child(executable, "exit", "0"), {}}));
    assert(workflow.add({"fail", child(executable, "exit", "9"), {}}));
    assert(workflow.add({"skipped", child(executable, "exit", "0"), {}}));
    assert(!workflow.add({"fail", child(executable, "exit", "0"), {}}));
    const auto report = workflow.run();
    assert(report);
    assert(report->steps.size() == 2);
    assert(!report->succeeded());

    const auto capabilities = vosp::ProcessRunner<TestErrorModel>::capabilities();
    assert(capabilities.wall_time_limit);
    assert(capabilities.cooperative_cancellation);
}
