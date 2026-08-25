#pragma once

/** @file version.hpp Version metadata for MicroWorkflowFramework. */

#include <cstdint>
#include <string_view>

namespace vosp::workflow
{
inline constexpr std::uint32_t version_major = 0;
inline constexpr std::uint32_t version_minor = 1;
inline constexpr std::uint32_t version_patch = 1;
inline constexpr std::string_view version = "0.1.1-beta";
} // namespace vosp::workflow
