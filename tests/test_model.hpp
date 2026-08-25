#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

struct TestError
{
    std::uint32_t value{};
    std::string text;

    [[nodiscard]] std::uint32_t code() const noexcept { return value; }
    [[nodiscard]] std::string_view message() const noexcept { return text; }
};

struct TestErrorModel
{
    using Error = TestError;

    template <typename Type> using Result = std::expected<Type, Error>;

    using OperationResult = Result<void>;

    [[nodiscard]] static Error make_error(std::uint32_t code, std::string message)
    {
        return Error{code, std::move(message)};
    }
};
