#include <vosp/workflow.hpp>

#include "../tests/test_model.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
template <typename Launch>
bool measure(std::string_view scenario, std::size_t iterations, Launch &&launch)
{
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index)
    {
        const auto result = launch();
        if (!result || !result->succeeded())
        {
            return false;
        }
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    std::cout << scenario << ',' << iterations << ',' << elapsed.count() << ','
              << static_cast<double>(iterations) / elapsed.count() << '\n';
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view{argv[1]} == "--child")
    {
        return 0;
    }
    const std::size_t iterations = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 100;
    const auto executable = std::filesystem::absolute(argv[0]);
    const vosp::ProcessSpec child{executable, {"--child"}, std::nullopt};
    vosp::ProcessRunner<TestErrorModel> runner;

    vosp::ResourceLimits launch_only;
    launch_only.terminate_descendants = false;

    std::cout << "scenario,iterations,seconds,launches_per_second\n";
    if (!measure("supervised_process_round_trip", iterations, [&] { return runner.run(child); }) ||
        !measure("launch_only_process_round_trip", iterations,
                 [&] { return runner.run(child, launch_only); }))
    {
        return 1;
    }
}
