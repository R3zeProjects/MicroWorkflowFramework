#include <vosp/workflow.hpp>

#include "test_model.hpp"

#include <atomic>
#include <barrier>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view{argv[1]} == "--child")
    {
        return 0;
    }

    constexpr std::size_t thread_count = 8;
    constexpr std::size_t launches_per_thread = 8;
    const auto executable = std::filesystem::absolute(argv[0]);
    const vosp::ProcessSpec child{executable, {"--child"}, std::nullopt};
    const vosp::ProcessRunner<TestErrorModel> runner;
    std::barrier start{thread_count};
    std::atomic<std::size_t> completed{};
    std::atomic<bool> failed{};

    std::vector<std::jthread> workers;
    workers.reserve(thread_count);
    for (std::size_t thread = 0; thread < thread_count; ++thread)
    {
        workers.emplace_back(
            [&, thread]
            {
                vosp::ResourceLimits limits;
                limits.terminate_descendants = (thread % 2) == 0;
                start.arrive_and_wait();
                for (std::size_t launch = 0; launch < launches_per_thread; ++launch)
                {
                    const auto result = runner.run(child, limits);
                    if (!result || !result->succeeded())
                    {
                        failed.store(true, std::memory_order_relaxed);
                        return;
                    }
                    completed.fetch_add(1, std::memory_order_relaxed);
                }
            });
    }
    workers.clear();

    return !failed.load(std::memory_order_relaxed) &&
                   completed.load(std::memory_order_relaxed) == thread_count * launches_per_thread
               ? 0
               : 1;
}
