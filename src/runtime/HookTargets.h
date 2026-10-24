#pragma once
#include <cstddef>
#include <cstdint>

namespace marco::runtime::hooks {
// Skyrim SE/AE. The plugin gates installation to 1.6.1170.
inline constexpr std::size_t ResetInventorySlot = 0x8A;
inline constexpr std::uint64_t ResetInventoryAE = 19802;
inline constexpr std::size_t BarterAcceptSlot = 1;
inline constexpr std::size_t BarterAdvanceSlot = 5;
} // namespace marco::runtime::hooks
