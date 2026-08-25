#pragma once

/** @file backend.hpp Narrow internal seam implemented by each supported OS. */

#include <vosp/workflow/types.hpp>

#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>

namespace vosp::workflow::detail
{
struct NativeError
{
    std::uint32_t code{};
    std::string message;
};

[[nodiscard]] std::expected<ProcessResult, NativeError>
run_process(const ProcessSpec &specification, const ResourceLimits &limits,
            const std::stop_token &stop_token);

[[nodiscard]] Capabilities platform_capabilities() noexcept;
} // namespace vosp::workflow::detail
