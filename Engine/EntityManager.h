#pragma once
#include "ECS.h"
#include <queue>
#include <array>
#include <cassert>

namespace TDEngine {
namespace ECS {

    class EntityManager {
    public:
        // Initialize the queue with all available entity IDs
        EntityManager();

        // Create an entity, giving it an ID
        Entity CreateEntity();

        // Destroy an entity, resetting its signature and putting its ID back in the queue
        void DestroyEntity(Entity entity);

        // Set an entity's signature (which components it has)
        void SetSignature(Entity entity, Signature signature);

        // Get an entity's signature
        Signature GetSignature(Entity entity);

    private:
        // Queue of unused entity IDs
        std::queue<Entity> m_availableEntities{};

        // Array of signatures where the index corresponds to the entity ID
        std::array<Signature, MAX_ENTITIES> m_signatures{};

        // Total living entities
        uint32_t m_livingEntityCount{};
    };

} // namespace ECS
} // namespace TDEngine
