#include "QuadTree.h"
#include <algorithm>

namespace Engine {

PhysicsQuadTreeNode::PhysicsQuadTreeNode(QuadRect bounds, int depth, int maxDepth, int maxObjects)
    : bounds_(bounds), depth_(depth), maxDepth_(maxDepth), maxObjects_(maxObjects) {
    for (int i = 0; i < 4; ++i) {
        nodes_[i] = nullptr;
    }
}

PhysicsQuadTreeNode::~PhysicsQuadTreeNode() {
    for (int i = 0; i < 4; ++i) {
        delete nodes_[i];
    }
}

void PhysicsQuadTreeNode::Clear() {
    objects_.clear();
    for (int i = 0; i < 4; ++i) {
        if (nodes_[i]) {
            nodes_[i]->Clear();
            delete nodes_[i];
            nodes_[i] = nullptr;
        }
    }
}

void PhysicsQuadTreeNode::Split() {
    float subWidth = bounds_.w / 2.0f;
    float subHeight = bounds_.h / 2.0f;
    float x = bounds_.x;
    float z = bounds_.z;

    nodes_[0] = new PhysicsQuadTreeNode({x + subWidth, z, subWidth, subHeight}, depth_ + 1, maxDepth_, maxObjects_); // 右上
    nodes_[1] = new PhysicsQuadTreeNode({x, z, subWidth, subHeight}, depth_ + 1, maxDepth_, maxObjects_);              // 左上
    nodes_[2] = new PhysicsQuadTreeNode({x, z + subHeight, subWidth, subHeight}, depth_ + 1, maxDepth_, maxObjects_);  // 左下
    nodes_[3] = new PhysicsQuadTreeNode({x + subWidth, z + subHeight, subWidth, subHeight}, depth_ + 1, maxDepth_, maxObjects_); // 右下
}

int PhysicsQuadTreeNode::GetIndex(const QuadRect& rect) const {
    int index = -1;
    double verticalMidpoint = bounds_.x + (bounds_.w / 2.0);
    double horizontalMidpoint = bounds_.z + (bounds_.h / 2.0);

    // 完全に属する象限を特定する
    bool topQuadrant = (rect.z < horizontalMidpoint && rect.z + rect.h < horizontalMidpoint);
    bool bottomQuadrant = (rect.z > horizontalMidpoint);
    bool leftQuadrant = (rect.x < verticalMidpoint && rect.x + rect.w < verticalMidpoint);
    bool rightQuadrant = (rect.x > verticalMidpoint);

    if (topQuadrant) {
        if (leftQuadrant) index = 1;
        else if (rightQuadrant) index = 0;
    } else if (bottomQuadrant) {
        if (leftQuadrant) index = 2;
        else if (rightQuadrant) index = 3;
    }
    // 境界をまたぐ場合は -1

    return index;
}

void PhysicsQuadTreeNode::Insert(const PhysicsQuadTreeObject& obj) {
    if (nodes_[0] != nullptr) {
        int index = GetIndex(obj.GetRect());
        if (index != -1) {
            nodes_[index]->Insert(obj);
            return;
        }
    }

    objects_.push_back(obj);

    if (objects_.size() > (size_t)maxObjects_ && depth_ < maxDepth_) {
        if (nodes_[0] == nullptr) {
            Split();
        }

        int i = 0;
        while (i < (int)objects_.size()) {
            int index = GetIndex(objects_[i].GetRect());
            if (index != -1) {
                PhysicsQuadTreeObject moveObj = objects_[i];
                objects_.erase(objects_.begin() + i);
                nodes_[index]->Insert(moveObj);
            } else {
                i++;
            }
        }
    }
}

void PhysicsQuadTreeNode::Query(const QuadRect& range, std::vector<uint32_t>& foundIds) const {
    if (!bounds_.Intersects(range)) return;

    // 現在のノードが保持する境界をまたぐオブジェクトと判定処理
    for (const auto& obj : objects_) {
        if (range.Intersects(obj.GetRect())) {
            foundIds.push_back(obj.id);
        }
    }

    if (nodes_[0] != nullptr) {
        for (int i = 0; i < 4; ++i) {
            nodes_[i]->Query(range, foundIds);
        }
    }
}


PhysicsQuadTree::PhysicsQuadTree(float minX, float minZ, float maxX, float maxZ, int maxDepth, int maxObjects) {
    QuadRect bounds{minX, minZ, maxX - minX, maxZ - minZ};
    root_ = new PhysicsQuadTreeNode(bounds, 0, maxDepth, maxObjects);
}

PhysicsQuadTree::~PhysicsQuadTree() {
    delete root_;
}

void PhysicsQuadTree::Clear() {
    root_->Clear();
}

void PhysicsQuadTree::Insert(uint32_t id, float minX, float minZ, float maxX, float maxZ) {
    PhysicsQuadTreeObject obj;
    obj.id = id;
    obj.minX = minX;
    obj.maxX = maxX;
    obj.minZ = minZ;
    obj.maxZ = maxZ;
    root_->Insert(obj);
}

void PhysicsQuadTree::Query(float minX, float minZ, float maxX, float maxZ, std::vector<uint32_t>& foundIds) const {
    QuadRect range{minX, minZ, maxX - minX, maxZ - minZ};
    root_->Query(range, foundIds);
}

} // namespace Engine
