#include <vosp/workflow.hpp>

#include "../tests/test_model.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/process/v2/process.hpp>
#include <uv.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct Scenario
{
    std::string_view name;
    std::function<bool()> launch;
    std::vector<double> rates;
};

struct UvProcess
{
    uv_process_t handle{};
    std::int64_t exit_status{-1};
};

void on_uv_exit(uv_process_t *handle, std::int64_t exit_status, int)
{
    auto &process = *static_cast<UvProcess *>(handle->data);
    process.exit_status = exit_status;
    uv_close(reinterpret_cast<uv_handle_t *>(handle), nullptr);
}

std::string utf8_path(const std::filesystem::path &path)
{
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}
} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view{argv[1]} == "--child")
    {
        return 0;
    }

    const std::size_t iterations = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 100;
    const std::size_t rounds = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 7;
    if (iterations == 0 || rounds == 0)
    {
        return 2;
    }

    const auto executable = std::filesystem::absolute(argv[0]);
    const auto executable_utf8 = utf8_path(executable);
    const boost::filesystem::path boost_executable{executable.wstring()};
    vosp::ProcessRunner<TestErrorModel> runner;
    const vosp::ProcessSpec specification{executable, {"--child"}, std::nullopt};

    vosp::ResourceLimits launch_only;
    launch_only.terminate_descendants = false;

    boost::asio::io_context boost_context;
    uv_loop_t uv_loop{};
    if (uv_loop_init(&uv_loop) != 0)
    {
        return 3;
    }
    std::string uv_child_argument{"--child"};
    char *uv_arguments[]{const_cast<char *>(executable_utf8.c_str()), uv_child_argument.data(),
                         nullptr};

    std::vector<Scenario> scenarios;
    scenarios.push_back({"mwf_supervised", [&]
                         {
                             const auto result = runner.run(specification);
                             return result && result->succeeded();
                         }});
    scenarios.push_back({"mwf_launch_only", [&]
                         {
                             const auto result = runner.run(specification, launch_only);
                             return result && result->succeeded();
                         }});
    scenarios.push_back({"boost_process_v2", [&]
                         {
                             boost::process::v2::process process{
                                 boost_context, boost_executable, {"--child"}};
                             return process.wait() == 0;
                         }});
    scenarios.push_back({"libuv", [&]
                         {
                             UvProcess process;
                             process.handle.data = &process;
                             uv_process_options_t options{};
                             options.exit_cb = on_uv_exit;
                             options.file = executable_utf8.c_str();
                             options.args = uv_arguments;
                             if (uv_spawn(&uv_loop, &process.handle, &options) != 0)
                             {
                                 return false;
                             }
                             uv_run(&uv_loop, UV_RUN_DEFAULT);
                             return process.exit_status == 0;
                         }});

    for (auto &scenario : scenarios)
    {
        for (int warmup = 0; warmup < 3; ++warmup)
        {
            if (!scenario.launch())
            {
                return 4;
            }
        }
    }

    std::cout << "scenario,round,iterations,seconds,launches_per_second\n";
    for (std::size_t round = 0; round < rounds; ++round)
    {
        std::rotate(scenarios.begin(), scenarios.begin() + (round % scenarios.size()),
                    scenarios.end());
        for (auto &scenario : scenarios)
        {
            const auto started = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < iterations; ++index)
            {
                if (!scenario.launch())
                {
                    return 5;
                }
            }
            const std::chrono::duration<double> elapsed =
                std::chrono::steady_clock::now() - started;
            const double rate = static_cast<double>(iterations) / elapsed.count();
            scenario.rates.push_back(rate);
            std::cout << scenario.name << ',' << round << ',' << iterations << ','
                      << elapsed.count() << ',' << rate << '\n';
        }
    }

    std::cout << "summary,median_launches_per_second\n";
    for (auto &scenario : scenarios)
    {
        std::ranges::sort(scenario.rates);
        std::cout << scenario.name << ',' << scenario.rates[scenario.rates.size() / 2] << '\n';
    }

    return uv_loop_close(&uv_loop) == 0 ? 0 : 6;
}
