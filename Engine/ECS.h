#pragma once
#include <cstdint>
#include <bitset>

namespace TDEngine {
namespace ECS {

    // Entity
    // A simple integer ID.
    using Entity = std::uint32_t;
    const Entity MAX_ENTITIES = 10000;

    // Component Type
    // A simple integer ID to differentiate different component types.
    using ComponentType = std::uint8_t;
    const ComponentType MAX_COMPONENTS = 32;

    // Signature
    // A bitset to keep track of which components an entity has.
    // Also used to define which components a system requires.
    using Signature = std::bitset<MAX_COMPONENTS>;

} // namespace ECS
} // namespace TDEngine
