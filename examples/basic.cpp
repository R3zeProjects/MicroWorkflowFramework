#include <vosp/workflow.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
struct Error
{
    std::uint32_t value{};
    std::string text;

    [[nodiscard]] std::uint32_t code() const noexcept { return value; }
    [[nodiscard]] std::string_view message() const noexcept { return text; }
};

struct ErrorModel
{
    using Error = ::Error;
    template <typename Type> using Result = std::expected<Type, Error>;
    using OperationResult = Result<void>;

    [[nodiscard]] static Error make_error(std::uint32_t code, std::string message)
    {
        return Error{code, std::move(message)};
    }
};
} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::string_view{argv[1]} == "--child")
    {
        return 0;
    }

    vosp::ProcessWorkflow<ErrorModel> workflow;
    vosp::ResourceLimits limits;
    limits.memory_bytes = 256ULL * 1024 * 1024;
    limits.wall_time = std::chrono::seconds{5};
    limits.process_count = 1;

    const auto executable = std::filesystem::absolute(argv[0]);
    if (!workflow.add({"isolated-step", {executable, {"--child"}, {}}, limits}))
    {
        return 1;
    }

    const auto report = workflow.run();
    if (!report)
    {
        std::cerr << report.error().message() << '\n';
        return 2;
    }
    std::cout << "steps=" << report->steps.size() << " success=" << std::boolalpha
              << report->succeeded() << '\n';
    return report->succeeded() ? 0 : 3;
}
