#pragma once
#include "ECS.h"
#include <array>
#include <cassert>
#include <unordered_map>

namespace TDEngine {
namespace ECS {

    // Interfaces are needed so that ComponentManager can keep a list
    // of arrays without knowing the specific component types beforehand.
    class IComponentArray {
    public:
        virtual ~IComponentArray() = default;
        virtual void EntityDestroyed(Entity entity) = 0;
    };

    template<typename T>
    class ComponentArray : public IComponentArray {
    public:
        void InsertData(Entity entity, T component) {
            assert(entityToIndexMap.find(entity) == entityToIndexMap.end() && "Component added to same entity more than once.");

            // Put new item at end
            size_t newIndex = m_size;
            entityToIndexMap[entity] = newIndex;
            indexToEntityMap[newIndex] = entity;
            componentArray[newIndex] = component;
            m_size++;
        }

        void RemoveData(Entity entity) {
            assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Removing non-existent component.");

            // Copy element at end into deleted element's place to maintain density
            size_t indexOfRemovedEntity = entityToIndexMap[entity];
            size_t indexOfLastElement = m_size - 1;
            componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

            // Update map to point to moved spot
            Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
            entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            entityToIndexMap.erase(entity);
            indexToEntityMap.erase(indexOfLastElement);
            m_size--;
        }

        T& GetData(Entity entity) {
            assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Retrieving non-existent component.");
            return componentArray[entityToIndexMap[entity]];
        }

        void EntityDestroyed(Entity entity) override {
            if (entityToIndexMap.find(entity) != entityToIndexMap.end()) {
                RemoveData(entity);
            }
        }

    private:
        // The packed array of components (SoA approach for specific component type)
        std::array<T, MAX_ENTITIES> componentArray;

        // Map from an entity ID to an array index.
        std::unordered_map<Entity, size_t> entityToIndexMap;

        // Map from an array index to an entity ID.
        std::unordered_map<size_t, Entity> indexToEntityMap;

        size_t m_size{};
    };

} // namespace ECS
} // namespace TDEngine
