#pragma once

#include <vector>
#include <cstdint>

namespace Engine {

// 単純なAABB（2D: XZ平面用）
struct QuadRect {
    float x; // 最小X
    float z; // 最小Z
    float w; // 幅(MaxX - MinX)
    float h; // 奥行き(MaxZ - MinZ)
    
    bool Intersects(const QuadRect& other) const {
        return !(x + w < other.x || x > other.x + other.w ||
                 z + h < other.z || z > other.z + other.h);
    }
    bool Contains(const QuadRect& other) const {
         return (x <= other.x && x + w >= other.x + other.w &&
                 z <= other.z && z + h >= other.z + other.h);
    }
};

// ツリーに登録するオブジェクト（AABBとID）
struct PhysicsQuadTreeObject {
    uint32_t id;          // 配列インデックス等、一意のID
    float minX, maxX;
    float minZ, maxZ;
    
    QuadRect GetRect() const {
        return QuadRect{minX, minZ, maxX - minX, maxZ - minZ};
    }
};

class PhysicsQuadTreeNode {
public:
    PhysicsQuadTreeNode(QuadRect bounds, int depth, int maxDepth, int maxObjects);
    ~PhysicsQuadTreeNode();

    void Clear();
    void Insert(const PhysicsQuadTreeObject& obj);
    
    // 特定の範囲と交差するオブジェクトIDを取得
    void Query(const QuadRect& range, std::vector<uint32_t>& foundIds) const;

private:
    void Split();
    int GetIndex(const QuadRect& rect) const;

    QuadRect bounds_;
    int depth_;
    int maxDepth_;
    int maxObjects_;
    std::vector<PhysicsQuadTreeObject> objects_;
    PhysicsQuadTreeNode* nodes_[4];
};

class PhysicsQuadTree {
public:
    PhysicsQuadTree(float minX, float minZ, float maxX, float maxZ, int maxDepth = 5, int maxObjects = 10);
    ~PhysicsQuadTree();

    // 毎フレーム再構築するためにクリアする
    void Clear();

    // オブジェクトの挿入
    void Insert(uint32_t id, float minX, float minZ, float maxX, float maxZ);

    // 範囲検索
    void Query(float minX, float minZ, float maxX, float maxZ, std::vector<uint32_t>& foundIds) const;

private:
    PhysicsQuadTreeNode* root_;
};

} // namespace Engine
