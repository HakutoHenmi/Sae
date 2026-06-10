#pragma once
#include "ECS.h"
#include <set>

namespace TDEngine {
namespace ECS {

    class System {
    public:
        std::set<Entity> m_entities;
        virtual ~System() = default;
        virtual void Update(float) {} // dt名を削除して警告回避
    };

} // namespace ECS
} // namespace TDEngine
