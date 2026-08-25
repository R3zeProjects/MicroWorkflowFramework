#include <vosp/error.hpp>
#include <vosp/workflow.hpp>

#include <cassert>
#include <filesystem>
#include <string_view>

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view{argv[1]} == "--child")
    {
        return 0;
    }

    vosp::ProcessRunner<vosp::error::Model> runner;
    const auto executable = std::filesystem::absolute(argv[0]);
    const auto result = runner.run({executable, {"--child"}, std::nullopt});
    assert(result);
    assert(result->succeeded());
}
