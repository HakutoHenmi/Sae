// CollisionCompute.hlsl
// GPU-based batched OBB-vs-Mesh (BVH) collision detection

struct CollisionRequest {
    float4x4 worldA;
    float4x4 worldB;
    float3 obbCenter;
    float _pad0;
    float3 obbExtents;
    float _pad1;
    float3 obbAxisX;
    float _pad2;
    float3 obbAxisY;
    float _pad3;
    float3 obbAxisZ;
    float _pad4;
    uint resultIndex;
    uint numBvhNodes;
    uint meshB; 
    uint _pad5;
};

struct ContactInfo {
    float3 normal;
    float depth;
    float3 position;
    uint intersected;
};

// BVH Node structure (Matching Model.h exactly: 48 bytes)
struct BvhNode {
    float3 bmin;
    float3 bmax;
    int left;
    int right;
    uint firstTri;
    uint triCount;
    float2 _pad0; // Padding to 48 bytes
};

// Root Parameters
cbuffer cbConstants : register(b0) {
    uint g_NumRequests;
};

StructuredBuffer<CollisionRequest> g_Requests : register(t0);
ByteAddressBuffer g_BvhNodes : register(t1);      // StructuredBuffer -> ByteAddressBuffer
ByteAddressBuffer g_BvhIndices : register(t2); 
ByteAddressBuffer g_ModelVertices : register(t3); 
ByteAddressBuffer g_ModelIndices : register(t4);   
RWStructuredBuffer<ContactInfo> g_Results : register(u0);

// Node stride in C++ (float3+float3+int+int+uint+uint+float2 = 12+12+4+4+4+4+8 = 48 bytes)
static const uint kNodeStride = 48;
// Vertex stride in C++ (float4+float2+float3+float4+uint4 = 16+8+12+16+16 = 68 bytes)
static const uint kVertexStride = 68;

// Helper: Get Vertex Position
float3 GetVertexPos(uint vertexIdx) {
    uint addr = vertexIdx * kVertexStride;
    return asfloat(g_ModelVertices.Load3(addr));
}

// Helper: Fast AABB-AABB intersection test
bool IntersectAabbAabb(float3 min1, float3 max1, float3 min2, float3 max2) {
    return (min1.x <= max2.x && max1.x >= min2.x) &&
           (min1.y <= max2.y && max1.y >= min2.y) &&
           (min1.z <= max2.z && max1.z >= min2.z);
}

// OBB vs Triangle SAT
bool IntersectObbTriangle(float3 center, float3 extents, float3 axes[3], float3 v0, float3 v1, float3 v2, out float3 outNormal, out float outDepth) {
    float3 edges[3] = { v1 - v0, v2 - v1, v0 - v2 };
    float3 triNormalVec = cross(edges[0], edges[1]);
    float triNormalLenSq = dot(triNormalVec, triNormalVec);
    if (triNormalLenSq < 1e-8) return false;
    float3 triNormal = triNormalVec / sqrt(triNormalLenSq);

    float minOverlap = 1e30;
    float3 bestAxis = float3(0, 1, 0);

    // 1. Triangle Normal
    float3 axis = triNormal;
    float rA = extents.x * abs(dot(axis, axes[0])) + extents.y * abs(dot(axis, axes[1])) + extents.z * abs(dot(axis, axes[2]));
    float p0 = dot(v0 - center, axis);
    float overlap = rA - abs(p0);
    if (overlap < 0) return false;
    if (overlap < minOverlap) { minOverlap = overlap; bestAxis = axis; }

    // 2. Box Axes (3)
    for (int iBox = 0; iBox < 3; ++iBox) {
        float3 bAxis = axes[iBox];
        float rA = extents[iBox];
        float t0 = dot(v0 - center, bAxis);
        float t1 = dot(v1 - center, bAxis);
        float t2 = dot(v2 - center, bAxis);
        float tMin = min(t0, min(t1, t2)), tMax = max(t0, max(t1, t2));
        float overlap = min(rA - tMin, tMax + rA);
        if (tMin > rA || tMax < -rA) return false;
        if (overlap < minOverlap) { minOverlap = overlap; bestAxis = bAxis; }
    }

    // 3. Cross Products (9)
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float3 cAxis = cross(axes[i], edges[j]);
            float cLenSq = dot(cAxis, cAxis);
            if (cLenSq < 1e-8) continue;
            cAxis /= sqrt(cLenSq);

            float rA = extents.x * abs(dot(cAxis, axes[0])) + extents.y * abs(dot(cAxis, axes[1])) + extents.z * abs(dot(cAxis, axes[2]));
            float t0 = dot(v0 - center, cAxis), t1 = dot(v1 - center, cAxis), t2 = dot(v2 - center, cAxis);
            float tMin = min(t0, min(t1, t2)), tMax = max(t0, max(t1, t2));
            float overlap = min(rA - tMin, tMax + rA);
            if (tMin > rA || tMax < -rA) return false;
            if (overlap < minOverlap) { minOverlap = overlap; bestAxis = cAxis; }
        }
    }

    if (dot(center - v0, bestAxis) < 0) bestAxis = -bestAxis;
    outNormal = bestAxis;
    outDepth = minOverlap;
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint pairIdx = dtid.x;
    if (pairIdx >= g_NumRequests) return;
    
    CollisionRequest req = g_Requests[pairIdx];
    if (req.numBvhNodes == 0) return;
    
    float3 obbCenter = req.obbCenter;
    float3 obbExtents = req.obbExtents;
    float3 obbAxes[3] = { req.obbAxisX, req.obbAxisY, req.obbAxisZ };

    float3 obbAabbMin = obbCenter - (abs(obbAxes[0]) * obbExtents.x + abs(obbAxes[1]) * obbExtents.y + abs(obbAxes[2]) * obbExtents.z);
    float3 obbAabbMax = obbCenter + (abs(obbAxes[0]) * obbExtents.x + abs(obbAxes[1]) * obbExtents.y + abs(obbAxes[2]) * obbExtents.z);

    ContactInfo res;
    res.intersected = 0;
    res.normal = float3(0, 1, 0);
    res.depth = 0;
    res.position = float3(0, 0, 0);

    int stack[64]; int stackPtr = 0; stack[stackPtr++] = 0; 
    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        if (nodeIdx < 0 || (uint)nodeIdx >= req.numBvhNodes) continue;
        
        // Manual loading from ByteAddressBuffer
        uint nodeAddr = nodeIdx * kNodeStride;
        float3 bmin = asfloat(g_BvhNodes.Load3(nodeAddr + 0));
        float3 bmax = asfloat(g_BvhNodes.Load3(nodeAddr + 12));
        int left = g_BvhNodes.Load(nodeAddr + 24);
        int right = g_BvhNodes.Load(nodeAddr + 28);
        uint firstTri = g_BvhNodes.Load(nodeAddr + 32);
        uint triCount = g_BvhNodes.Load(nodeAddr + 36);

        if (IntersectAabbAabb(obbAabbMin, obbAabbMax, bmin, bmax)) {
            if (left < 0) {
                // Leaf: Iterate triangles
                for (uint i = 0; i < triCount; ++i) {
                    uint triIdx = g_BvhIndices.Load((firstTri + i) * 4);
                    uint vIdx0 = g_ModelIndices.Load((triIdx * 3 + 0) * 4);
                    uint vIdx1 = g_ModelIndices.Load((triIdx * 3 + 1) * 4);
                    uint vIdx2 = g_ModelIndices.Load((triIdx * 3 + 2) * 4);

                    float3 v0 = GetVertexPos(vIdx0);
                    float3 v1 = GetVertexPos(vIdx1);
                    float3 v2 = GetVertexPos(vIdx2);

                    float3 localNormal; float localDepth;
                    if (IntersectObbTriangle(obbCenter, obbExtents, obbAxes, v0, v1, v2, localNormal, localDepth)) {
                        if (res.intersected == 0 || localDepth > res.depth) {
                            res.intersected = 1;
                            float3 worldNormalVec = mul(float4(localNormal, 0), req.worldB).xyz;
                            float worldScale = length(worldNormalVec);
                            res.normal = worldNormalVec / max(1e-6, worldScale);
                            res.position = mul(float4(obbCenter, 1), req.worldB).xyz;
                            res.depth = localDepth * worldScale;
                        }
                    }
                }
            } else {
                if (stackPtr < 62) { stack[stackPtr++] = right; stack[stackPtr++] = left; }
            }
        }
    }
    g_Results[req.resultIndex] = res;
}
