#include "EntityManager.h"

namespace TDEngine {
namespace ECS {

    EntityManager::EntityManager() {
        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity) {
            m_availableEntities.push(entity);
        }
    }

    Entity EntityManager::CreateEntity() {
        assert(m_livingEntityCount < MAX_ENTITIES && "Too many entities in existence.");

        // Take an ID from the front of the queue
        Entity id = m_availableEntities.front();
        m_availableEntities.pop();
        ++m_livingEntityCount;

        return id;
    }

    void EntityManager::DestroyEntity(Entity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");

        // Invalidate the destroyed entity's signature
        m_signatures[entity].reset();

        // Put the destroyed ID at the back of the queue
        m_availableEntities.push(entity);
        --m_livingEntityCount;
    }

    void EntityManager::SetSignature(Entity entity, Signature signature) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        m_signatures[entity] = signature;
    }

    Signature EntityManager::GetSignature(Entity entity) {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        return m_signatures[entity];
    }

} // namespace ECS
} // namespace TDEngine
