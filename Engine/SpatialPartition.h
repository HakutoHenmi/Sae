#pragma once
#include <vector>
#include <memory>
#include "ECS.h"
#include "Components.h"
#include "Matrix4x4.h"

namespace Engine {

    struct Rect {
        float x, z, w, h;
        bool Contains(const Vector3& pos) const {
            return pos.x >= x && pos.x <= x + w && pos.z >= z && pos.z <= z + h;
        }
    };

    class QuadTree {
    public:
        struct Node {
            Rect bounds;
            std::vector<TDEngine::ECS::Entity> entities;
            std::unique_ptr<Node> children[4];
            bool isLeaf = true;

            Node(Rect b) : bounds(b) {}
        };

        QuadTree(Rect bounds, uint32_t maxEntities = 8, uint32_t maxDepth = 5)
            : maxEntities_(maxEntities), maxDepth_(maxDepth) {
            root_ = std::make_unique<Node>(bounds);
        }

        void Insert(TDEngine::ECS::Entity entity, const Vector3& pos) {
            Insert(root_.get(), entity, pos, 0);
        }

        void Query(Rect range, std::vector<TDEngine::ECS::Entity>& outEntities) const {
            Query(root_.get(), range, outEntities);
        }

        void Clear() {
            Rect b = root_->bounds;
            root_ = std::make_unique<Node>(b);
        }

    private:
        void Split(Node* node) {
            float hw = node->bounds.w * 0.5f;
            float hh = node->bounds.h * 0.5f;
            float x = node->bounds.x;
            float z = node->bounds.z;

            node->children[0] = std::make_unique<Node>(Rect{ x, z, hw, hh });
            node->children[1] = std::make_unique<Node>(Rect{ x + hw, z, hw, hh });
            node->children[2] = std::make_unique<Node>(Rect{ x, z + hh, hw, hh });
            node->children[3] = std::make_unique<Node>(Rect{ x + hw, z + hh, hw, hh });
            node->isLeaf = false;

            for (auto entity : node->entities) {
                (void)entity; // 警告回避
                // 本来はエンティティの座標を再取得して振り分けるべきだが、簡略化のため
                // ここではSplit直後のエンティティ再配置は呼び出し側で制御するか、
                // あるいは座標も保持するように拡張する。
            }
        }

        void Insert(Node* node, TDEngine::ECS::Entity entity, const Vector3& pos, uint32_t depth) {
            if (!node->bounds.Contains(pos)) return;

            if (node->isLeaf) {
                if (node->entities.size() < maxEntities_ || depth >= maxDepth_) {
                    node->entities.push_back(entity);
                } else {
                    Split(node);
                    InsertToChildren(node, entity, pos, depth);
                }
            } else {
                InsertToChildren(node, entity, pos, depth);
            }
        }

        void InsertToChildren(Node* node, TDEngine::ECS::Entity entity, const Vector3& pos, uint32_t depth) {
            for (int i = 0; i < 4; ++i) {
                if (node->children[i]->bounds.Contains(pos)) {
                    Insert(node->children[i].get(), entity, pos, depth + 1);
                    break;
                }
            }
        }

        void Query(Node* node, Rect range, std::vector<TDEngine::ECS::Entity>& outEntities) const {
            if (!CheckOverlap(node->bounds, range)) return;

            for (auto entity : node->entities) {
                outEntities.push_back(entity);
            }

            if (!node->isLeaf) {
                for (int i = 0; i < 4; ++i) {
                    Query(node->children[i].get(), range, outEntities);
                }
            }
        }

        bool CheckOverlap(Rect a, Rect b) const {
            return !(a.x + a.w < b.x || b.x + b.w < a.x || a.z + a.h < b.z || b.z + b.h < a.z);
        }

    private:
        std::unique_ptr<Node> root_;
        uint32_t maxEntities_;
        uint32_t maxDepth_;
    };

} // namespace Engine
