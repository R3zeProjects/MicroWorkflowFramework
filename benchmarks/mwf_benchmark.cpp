#include <vosp/workflow.hpp>

#include "../tests/test_model.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

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

    const auto started = std::chrono::steady_clock::now();
    std::size_t completed = 0;
    for (std::size_t index = 0; index < iterations; ++index)
    {
        const auto result = runner.run(child);
        if (!result || !result->succeeded())
        {
            return 1;
        }
        ++completed;
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started);
    std::cout << "scenario,iterations,seconds,launches_per_second\n"
              << "native_process_round_trip," << completed << ',' << elapsed.count() << ','
              << static_cast<double>(completed) / elapsed.count() << '\n';
}
