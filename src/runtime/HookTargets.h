#pragma once
#include <cstddef>
#include <cstdint>

namespace marco::runtime::hooks {
// Desktop SE/AE slots are shared; function IDs use their Address Library family.
inline constexpr std::size_t ResetInventorySlot = 0x8A;
inline constexpr std::uint64_t ResetInventorySE = 19375;
inline constexpr std::uint64_t ResetInventoryAE = 19802;
inline constexpr std::size_t BarterAcceptSlot = 1;
inline constexpr std::size_t BarterAdvanceSlot = 5;
inline constexpr std::size_t ContainerAcceptSlot = 1;
inline constexpr std::size_t ContainerAdvanceSlot = 5;
} // namespace marco::runtime::hooks
