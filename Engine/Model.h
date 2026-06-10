// Engine/Model.h
#pragma once
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

#include "Matrix4x4.h"
#include "Transform.h"
#include <algorithm> // 追加
#include <optional>

namespace Engine {

// 最大ボーン数（シェーダーの定義と合わせる）
static constexpr int kMaxBones = 128;

// 頂点データ（スキニング対応）
struct VertexData {
	DirectX::XMFLOAT4 position;
	DirectX::XMFLOAT2 texcoord;
	DirectX::XMFLOAT3 normal;
	// --- 追加: スキニング用 ---
	float boneWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	uint32_t boneIndices[4] = {0, 0, 0, 0};
};

struct MaterialData {
	std::string textureFilePath;
	std::vector<std::string> extraTextures; // ★追加: 地形用のスプラットマップやレイヤー
};

// ボーン単体
struct Bone {
	std::string name;
	Matrix4x4 offsetMatrix; // 初期姿勢 -> ボーン空間 (Inverse Bind Pose)
	int index = -1;
};

// ノード階層構造（アニメーション計算用）
struct Node {
	std::string name;
	QuaternionTransform transform; // ローカル変換
	Matrix4x4 localMatrix;
	std::vector<Node> children;
};

// --- 追加: スケルトン用構造体 ---
struct Joint {
	QuaternionTransform transform; // ローカル変換
	Matrix4x4 localMatrix;
	Matrix4x4 skeletonSpaceMatrix;
	std::string name;
	std::vector<int32_t> children;
	int32_t index;
	std::optional<int32_t> parent;
};

struct Skeleton {
	int32_t root;
	std::map<std::string, int32_t> jointMap;
	std::vector<Joint> joints;

	void Update();
};

// --- アニメーション用構造体 ---
template<typename T> struct Keyframe {
	float time;
	T value;
};

// ノードごとのアニメーション（移動・回転・スケール）
struct NodeAnimation {
	std::vector<Keyframe<DirectX::XMFLOAT3>> translations;
	std::vector<Keyframe<DirectX::XMFLOAT4>> rotations; // Quaternion
	std::vector<Keyframe<DirectX::XMFLOAT3>> scales;
};

// BVHノード (空間分割用)
struct BVHNode {
	Vector3 min;
	Vector3 max;
	int leftChild = -1;
	int rightChild = -1;
	uint32_t firstTriangle = 0;
	uint32_t triangleCount = 0;
	float _pad[2]; // 16バイトアライメント用 (計48バイト)
};

// アニメーション全体
struct Animation {
	std::string name;
	float duration;                                      // 全体の長さ(単位: Tick)
	float ticksPerSecond;                                // 1秒あたりのTick数
	std::map<std::string, NodeAnimation> nodeAnimations; // ノード名 -> アニメーション
};

// モデルデータ全体
struct ModelData {
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices; // インデックスバッファ対応
	MaterialData material;

	// スキニング情報
	std::vector<Bone> bones;
	std::unordered_map<std::string, int> boneMapping; // 名前 -> bones配列のインデックス
	Node rootNode;

	// アニメーション情報
	std::vector<Animation> animations;

	// AABB (Local Space)
	Vector3 min{0, 0, 0};
	Vector3 max{0, 0, 0};

	// BVHデータ
	std::vector<BVHNode> bvhNodes;
	std::vector<uint32_t> bvhIndices; // 並び替えられたインデックス
};

class Model {
public:
	// ファイル読み込み
	bool Load(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& objPath);

	// 動的メッシュ初期化 (新規追加)
	void InitializeDynamic(ID3D12Device* device, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);
	
	// 動的メッシュ頂点更新 (新規追加)
	void UpdateVertices(const std::vector<VertexData>& vertices);

	// SRV作成
	void CreateSrv(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, ID3D12DescriptorHeap* srvHeapMaster, UINT descriptorSize, UINT heapIndex);

	// 描画 (Rendererの実装に合わせてデフォルト引数を調整: t0がindex 3の場合)
	void Draw(ID3D12GraphicsCommandList* cmd, UINT rootSrvParamIndex = 3);
	void DrawInstanced(ID3D12GraphicsCommandList* cmd, UINT instanceCount, UINT rootSrvParamIndex = 3);

	// ゲッター
	const ModelData& GetData() const { return data_; }
	UINT GetVertexCount() const { return static_cast<UINT>(data_.vertices.size()); }
	uint32_t GetIndexCount() const { return indexCount_; } // 追加

	const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return vbv_; }
	const D3D12_INDEX_BUFFER_VIEW& GetIBV() const { return ibv_; } // 追加
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const { return srvGpu_; }

	// ★追加: GPU用BVH・メッシュバッファ
	D3D12_GPU_VIRTUAL_ADDRESS GetBvhNodeBufferAddr() const { return vbBvhNodes_ ? vbBvhNodes_->GetGPUVirtualAddress() : 0; }
	D3D12_GPU_VIRTUAL_ADDRESS GetBvhIndexBufferAddr() const { return vbBvhIndices_ ? vbBvhIndices_->GetGPUVirtualAddress() : 0; }
	D3D12_GPU_VIRTUAL_ADDRESS GetVertexBufferAddr() const { return vb_ ? vb_->GetGPUVirtualAddress() : 0; }
	D3D12_GPU_VIRTUAL_ADDRESS GetIndexBufferAddr() const { return ib_ ? ib_->GetGPUVirtualAddress() : 0; }
	uint32_t GetBvhNodeCount() const { return (uint32_t)data_.bvhNodes.size(); }

	// アニメーション適用時の行列計算関数
	// node: 現在処理中のノード
	// parentTransform: 親ノードのワールド変換行列
	// animation: 再生するアニメーションデータ
	// time: 現在のアニメーション時刻(Tick)
	// outPalette: 計算結果のボーン行列書き込み先
	// debugLines: デバッグ用のスケルトン描画ライン出力先（任意）
	void UpdateSkeleton(const Node& node, const Matrix4x4& parentTransform, const Animation& animation, float time, std::vector<Matrix4x4>& outPalette, std::vector<std::pair<Vector3, Vector3>>* debugLines = nullptr);

	// BVH構築
	void BuildBVH();

	// レイテスト（AABBおよびTriangle交差判定）
	bool RayCast(const DirectX::XMVECTOR& rayOrig, const DirectX::XMVECTOR& rayDir, const Matrix4x4& worldTransform, float& outDist, Vector3& outHitPoint) const;

private:
	void SubdivideBVH(uint32_t nodeIdx);
	void UpdateNodeBounds(uint32_t nodeIdx);
	static bool RayIntersectsAABB(const DirectX::XMVECTOR& rayOrig, const DirectX::XMVECTOR& rayDir, const Vector3& bmin, const Vector3& bmax, float& tOut);

	// ------------ 低レベルユーティリティ ------------
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);
	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& meta);
	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* cmd);

private:
	// ------------ メンバ ------------
	ModelData data_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
	Microsoft::WRL::ComPtr<ID3D12Resource> ib_; // 追加: インデックスバッファ
	D3D12_VERTEX_BUFFER_VIEW vbv_{};
	D3D12_INDEX_BUFFER_VIEW ibv_{}; // 追加
	uint32_t indexCount_ = 0;       // 追加

	// ★追加: GPU用BVHバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vbBvhNodes_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vbBvhIndices_;

	Microsoft::WRL::ComPtr<ID3D12Resource> tex_;
	Microsoft::WRL::ComPtr<ID3D12Resource> upload_; // 中間バッファ保持
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpu_{};
	bool hasTexture_ = false;
};

} // namespace Engine