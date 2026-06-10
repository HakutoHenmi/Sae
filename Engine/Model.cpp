#include "Model.h"
#include "Renderer.h"
#include "WindowDX.h"
#include "PathUtils.h"
#include <fstream>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include "d3dx12.h"
#include <DirectXTex.h>

// Assimp Includes
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace Engine {

// ToWide function replaced by PathUtils::FromUTF8

static void SplitPath(const std::string& full, std::string& dir, std::string& file) {
	size_t p = full.find_last_of("/\\");
	if (p == std::string::npos) {
		dir = ".";
		file = full;
	} else {
		dir = full.substr(0, p);
		file = full.substr(p + 1);
	}
}

static Matrix4x4 XMToM4(const DirectX::XMMATRIX& xm) {
	Matrix4x4 out{};
	DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&out), xm);
	return out;
}

// ★修正: Assimp(Column) -> DirectX(Row) 転置 + X軸反転
static Matrix4x4 AiToMat4(const aiMatrix4x4& m) {
	Matrix4x4 out;
	// 転置しつつ、0列目と0行目（0,0以外）を反転させてX軸ミラーリングを行う
	out.m[0][0] = m.a1;
	out.m[0][1] = -m.b1;
	out.m[0][2] = -m.c1;
	out.m[0][3] = -m.d1;
	out.m[1][0] = -m.a2;
	out.m[1][1] = m.b2;
	out.m[1][2] = m.c2;
	out.m[1][3] = m.d2;
	out.m[2][0] = -m.a3;
	out.m[2][1] = m.b3;
	out.m[2][2] = m.c3;
	out.m[2][3] = m.d3;
	out.m[3][0] = -m.a4;
	out.m[3][1] = m.b4;
	out.m[3][2] = m.c4;
	out.m[3][3] = m.d4;
	return out;
}

static void ReadNodeHierarchy(Node& node, const aiNode* src) {
	node.name = src->mName.C_Str();

	aiVector3D scale, pos;
	aiQuaternion rot;
	src->mTransformation.Decompose(scale, rot, pos);
	node.transform.scale = {scale.x, scale.y, scale.z};
	node.transform.rotate = {rot.x, -rot.y, -rot.z, rot.w};
	node.transform.translate = {-pos.x, pos.y, pos.z};

	node.localMatrix = AiToMat4(src->mTransformation);

	node.children.resize(src->mNumChildren);
	for (unsigned int i = 0; i < src->mNumChildren; ++i) {
		ReadNodeHierarchy(node.children[i], src->mChildren[i]);
	}
}

static void ReadAnimation(ModelData& modelData, const aiScene* scene) {
	for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
		aiAnimation* srcAnim = scene->mAnimations[i];
		Animation dstAnim;
		dstAnim.name = srcAnim->mName.C_Str();
		dstAnim.duration = (float)srcAnim->mDuration;
		dstAnim.ticksPerSecond = (srcAnim->mTicksPerSecond != 0) ? (float)srcAnim->mTicksPerSecond : 25.0f;

		for (unsigned int j = 0; j < srcAnim->mNumChannels; ++j) {
			aiNodeAnim* channel = srcAnim->mChannels[j];
			NodeAnimation nodeAnim;
			// Translation: X反転
			for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
				aiVector3D v = channel->mPositionKeys[k].mValue;
				nodeAnim.translations.push_back({
				    (float)channel->mPositionKeys[k].mTime, {-v.x, v.y, v.z}
                });
			}
			// Rotation: Y, Z反転 (X軸ミラー)
			for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
				aiQuaternion q = channel->mRotationKeys[k].mValue;
				nodeAnim.rotations.push_back({
				    (float)channel->mRotationKeys[k].mTime, {q.x, -q.y, -q.z, q.w}
                });
			}
			// Scale: そのまま
			for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
				aiVector3D v = channel->mScalingKeys[k].mValue;
				nodeAnim.scales.push_back({
				    (float)channel->mScalingKeys[k].mTime, {v.x, v.y, v.z}
                });
			}
			dstAnim.nodeAnimations[channel->mNodeName.C_Str()] = nodeAnim;
		}
		modelData.animations.push_back(dstAnim);
	}
}

static Vector3 CalculateTranslation(const std::vector<Keyframe<XMFLOAT3>>& keys, float time) {
	if (keys.empty())
		return {0, 0, 0};
	if (keys.size() == 1 || time <= keys.front().time)
		return {keys.front().value.x, keys.front().value.y, keys.front().value.z};
	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			XMVECTOR p = XMVectorLerp(XMLoadFloat3(&keys[i].value), XMLoadFloat3(&keys[i + 1].value), t);
			Vector3 res;
			XMStoreFloat3((XMFLOAT3*)&res, p);
			return res;
		}
	}
	return {keys.back().value.x, keys.back().value.y, keys.back().value.z};
}

static Vector3 CalculateScale(const std::vector<Keyframe<XMFLOAT3>>& keys, float time) {
	if (keys.empty())
		return {1, 1, 1};
	if (keys.size() == 1 || time <= keys.front().time)
		return {keys.front().value.x, keys.front().value.y, keys.front().value.z};
	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			XMVECTOR s = XMVectorLerp(XMLoadFloat3(&keys[i].value), XMLoadFloat3(&keys[i + 1].value), t);
			Vector3 res;
			XMStoreFloat3((XMFLOAT3*)&res, s);
			return res;
		}
	}
	return {keys.back().value.x, keys.back().value.y, keys.back().value.z};
}

static XMFLOAT4 CalculateRotation(const std::vector<Keyframe<XMFLOAT4>>& keys, float time) {
	if (keys.empty())
		return {0, 0, 0, 1};
	if (keys.size() == 1 || time <= keys.front().time)
		return keys.front().value;
	for (size_t i = 0; i < keys.size() - 1; ++i) {
		if (time >= keys[i].time && time <= keys[i + 1].time) {
			float t = (time - keys[i].time) / (keys[i + 1].time - keys[i].time);
			XMVECTOR q = XMQuaternionSlerp(XMLoadFloat4(&keys[i].value), XMLoadFloat4(&keys[i + 1].value), t);
			XMFLOAT4 res;
			XMStoreFloat4(&res, q);
			return res;
		}
	}
	return keys.back().value;
}

ComPtr<ID3D12Resource> Model::CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_UPLOAD};
	D3D12_RESOURCE_DESC rd{
	    D3D12_RESOURCE_DIMENSION_BUFFER, 0, (UINT64)sizeInBytes, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0},
               D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE
    };
	ComPtr<ID3D12Resource> res;
	// 引数のdeviceがnullptrの場合はシステム全体で共有されているRendererのデバイスを借りる（簡易実装）
	ID3D12Device* pDev = device;
	if (!pDev && Renderer::GetInstance()) pDev = Renderer::GetInstance()->GetDevice(); 
	
	if (pDev) {
		pDev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&res));
	}
	return res;
}

ComPtr<ID3D12Resource> Model::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& m) {
	D3D12_RESOURCE_DESC rd{
	    D3D12_RESOURCE_DIMENSION(m.dimension), 0, (UINT64)m.width, (UINT)m.height, (UINT16)m.arraySize, (UINT16)m.mipLevels, m.format, {1, 0},
               D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE
    };
	D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
	ComPtr<ID3D12Resource> tex;
	device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
	return tex;
}

ComPtr<ID3D12Resource> Model::UploadTextureData(ID3D12Resource* tex, const DirectX::ScratchImage& mip, ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) {
	std::vector<D3D12_SUBRESOURCE_DATA> subs;
	const DirectX::Image* imgs = mip.GetImages();
	for (size_t i = 0; i < mip.GetImageCount(); ++i)
		subs.push_back({imgs[i].pixels, (LONG_PTR)imgs[i].rowPitch, (LONG_PTR)imgs[i].slicePitch});
	ComPtr<ID3D12Resource> inter = CreateBufferResource(dev, GetRequiredIntermediateSize(tex, 0, (UINT)subs.size()));
	UpdateSubresources(cmd, tex, inter.Get(), 0, 0, (UINT)subs.size(), subs.data());
	D3D12_RESOURCE_BARRIER b = CD3DX12_RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	cmd->ResourceBarrier(1, &b);
	return inter;
}

bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* cmd, const std::string& objPath) {
	Assimp::Importer importer;
	const unsigned int flags = aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_LimitBoneWeights;
	
	// ReadFile supports external files (.bin, .mtl) unlike ReadFileFromMemory without an IO handler
	const aiScene* scene = importer.ReadFile(objPath.c_str(), flags);

	if (!scene || !scene->mRootNode) {
		OutputDebugStringA(("[Model::Load] Failed to load model: " + objPath + "\n").c_str());
		if (importer.GetErrorString()) {
			OutputDebugStringA((std::string("Assimp Error: ") + importer.GetErrorString() + "\n").c_str());
		}
		return false;
	}

	ReadNodeHierarchy(data_.rootNode, scene->mRootNode);
	if (scene->HasAnimations())
		ReadAnimation(data_, scene);

	uint32_t vertexOffset = 0;
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
			VertexData v{};
			// ★修正: 手動X反転 (World.cppと一致させる)
			v.position = {mesh->mVertices[i].x * -1.0f, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f};
			if (mesh->HasNormals())
				v.normal = {mesh->mNormals[i].x * -1.0f, mesh->mNormals[i].y, mesh->mNormals[i].z};
			if (mesh->HasTextureCoords(0))
				v.texcoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
			data_.vertices.push_back(v);
		}
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			aiFace& face = mesh->mFaces[f];
			data_.indices.push_back(vertexOffset + face.mIndices[0]);
			data_.indices.push_back(vertexOffset + face.mIndices[1]);
			data_.indices.push_back(vertexOffset + face.mIndices[2]);
		}
		if (mesh->HasBones()) {
			for (unsigned int i = 0; i < mesh->mNumBones; ++i) {
				aiBone* bone = mesh->mBones[i];
				std::string bName = bone->mName.C_Str();
				int bIdx = 0;
				if (data_.boneMapping.find(bName) == data_.boneMapping.end()) {
					bIdx = (int)data_.bones.size();
					// AiToMat4でX反転処理済み
					data_.bones.push_back({bName, AiToMat4(bone->mOffsetMatrix), bIdx});
					data_.boneMapping[bName] = bIdx;
				} else
					bIdx = data_.boneMapping[bName];
				for (unsigned int j = 0; j < bone->mNumWeights; ++j) {
					VertexData& v = data_.vertices[vertexOffset + bone->mWeights[j].mVertexId];
					for (int k = 0; k < 4; ++k) {
						if (v.boneWeights[k] == 0.0f) {
							v.boneWeights[k] = bone->mWeights[j].mWeight;
							v.boneIndices[k] = bIdx;
							break;
						}
					}
				}
			}
		}
		vertexOffset += mesh->mNumVertices;
	}

	// Calculate AABB
	if (!data_.vertices.empty()) {
		data_.min = {FLT_MAX, FLT_MAX, FLT_MAX};
		data_.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		for (const auto& v : data_.vertices) {
			data_.min.x = (std::min)(data_.min.x, v.position.x);
			data_.min.y = (std::min)(data_.min.y, v.position.y);
			data_.min.z = (std::min)(data_.min.z, v.position.z);
			data_.max.x = (std::max)(data_.max.x, v.position.x);
			data_.max.y = (std::max)(data_.max.y, v.position.y);
			data_.max.z = (std::max)(data_.max.z, v.position.z);
		}
	}

	// ★★★ 修正箇所: テクスチャ読み込み部分 ★★★
	if (scene->mNumMaterials > 0) {
		aiString str;
		aiMaterial* material = scene->mMaterials[0];

		// 1. 従来(Legacy)のDiffuseテクスチャを探す
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &str) != aiReturn_SUCCESS) {
			// 2. なければglTF(PBR)のBaseColorテクスチャを探す
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &str);
		}

		// パスが見つかった場合のみ設定する
		if (str.length > 0) {
			std::filesystem::path fullPath(PathUtils::FromUTF8(objPath));
			std::filesystem::path dir = fullPath.parent_path();
			std::filesystem::path texPath = dir / str.C_Str();
			data_.material.textureFilePath = PathUtils::ToUTF8(texPath.wstring());
		}
	}
	// ★★★ 修正終わり ★★★

	vb_ = CreateBufferResource(device, sizeof(VertexData) * data_.vertices.size());
	void* vmap;
	vb_->Map(0, nullptr, &vmap);
	std::memcpy(vmap, data_.vertices.data(), sizeof(VertexData) * data_.vertices.size());
	vb_->Unmap(0, nullptr);
	vbv_ = {vb_->GetGPUVirtualAddress(), (UINT)(sizeof(VertexData) * data_.vertices.size()), sizeof(VertexData)};
	ib_ = CreateBufferResource(device, sizeof(uint32_t) * data_.indices.size());
	void* imap;
	ib_->Map(0, nullptr, &imap);
	std::memcpy(imap, data_.indices.data(), sizeof(uint32_t) * data_.indices.size());
	ib_->Unmap(0, nullptr);
	ibv_ = {ib_->GetGPUVirtualAddress(), (UINT)(sizeof(uint32_t) * data_.indices.size()), DXGI_FORMAT_R32_UINT};
	indexCount_ = (uint32_t)data_.indices.size();

	if (!data_.material.textureFilePath.empty()) {
		ScratchImage mip;
		std::wstring widePath = PathUtils::GetUnifiedPathW(PathUtils::FromUTF8(data_.material.textureFilePath));
		if (SUCCEEDED(LoadFromWICFile(widePath.c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, mip))) {
			tex_ = CreateTextureResource(device, mip.GetMetadata());
			upload_ = UploadTextureData(tex_.Get(), mip, device, cmd);
			srvDesc_.Format = mip.GetMetadata().format;
			srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc_.Texture2D.MipLevels = (UINT)mip.GetMetadata().mipLevels;
			srvDesc_.Texture2D.MostDetailedMip = 0;
			srvDesc_.Texture2D.PlaneSlice = 0;
			srvDesc_.Texture2D.ResourceMinLODClamp = 0.0f;
			hasTexture_ = true;
		}
	}

	// BVH構築
	BuildBVH();

	return true;
}

void Model::InitializeDynamic(ID3D12Device* device, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
	data_.vertices = vertices;
	data_.indices = indices;
	hasTexture_ = false;

	vb_ = CreateBufferResource(device, sizeof(VertexData) * vertices.size());
	UpdateVertices(vertices);
	vbv_ = {vb_->GetGPUVirtualAddress(), (UINT)(sizeof(VertexData) * vertices.size()), sizeof(VertexData)};

	ib_ = CreateBufferResource(device, sizeof(uint32_t) * indices.size());
	void* imap;
	ib_->Map(0, nullptr, &imap);
	std::memcpy(imap, indices.data(), sizeof(uint32_t) * indices.size());
	ib_->Unmap(0, nullptr);
	ibv_ = {ib_->GetGPUVirtualAddress(), (UINT)(sizeof(uint32_t) * indices.size()), DXGI_FORMAT_R32_UINT};
	indexCount_ = (uint32_t)indices.size();
	
	// 動的メッシュにおけるBVH構築（地形追従などの必要性があれば追加可能）
	BuildBVH();
}

void Model::UpdateVertices(const std::vector<VertexData>& vertices) {
	if (vertices.size() != data_.vertices.size()) return; // 頂点数は固定前提
	data_.vertices = vertices;
	void* vmap;
	vb_->Map(0, nullptr, &vmap);
	std::memcpy(vmap, vertices.data(), sizeof(VertexData) * vertices.size());
	vb_->Unmap(0, nullptr);
}

void Model::CreateSrv(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap, ID3D12DescriptorHeap* srvHeapMaster, UINT descriptorSize, UINT heapIndex) {
	if (!hasTexture_)
		return;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu = srvHeap->GetCPUDescriptorHandleForHeapStart();
	cpu.ptr += (SIZE_T)descriptorSize * heapIndex;
	
	// ★追加: マスターヒープに対しても記述子を作成
	if (srvHeapMaster) {
		D3D12_CPU_DESCRIPTOR_HANDLE cpuMaster = srvHeapMaster->GetCPUDescriptorHandleForHeapStart();
		cpuMaster.ptr += (SIZE_T)descriptorSize * heapIndex;
		device->CreateShaderResourceView(tex_.Get(), &srvDesc_, cpuMaster);
	}

	srvGpu_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
	srvGpu_.ptr += (UINT64)descriptorSize * heapIndex;
	device->CreateShaderResourceView(tex_.Get(), &srvDesc_, cpu);
}

void Model::Draw(ID3D12GraphicsCommandList* cmd, UINT /*root*/) {
	cmd->IASetVertexBuffers(0, 1, &vbv_);
	cmd->IASetIndexBuffer(&ibv_);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

void Model::DrawInstanced(ID3D12GraphicsCommandList* cmd, UINT instanceCount, UINT /*root*/) {
	cmd->IASetVertexBuffers(0, 1, &vbv_);
	cmd->IASetIndexBuffer(&ibv_);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->DrawIndexedInstanced(indexCount_, instanceCount, 0, 0, 0);
}

void Model::BuildBVH() {
	if (data_.indices.empty()) return;

	data_.bvhNodes.clear();
	data_.bvhIndices.clear();

	uint32_t numTriangles = (uint32_t)data_.indices.size() / 3;
	data_.bvhIndices.reserve(numTriangles);
	for (uint32_t i = 0; i < numTriangles; ++i) data_.bvhIndices.push_back(i);

	// Root node
	BVHNode root{};
	root.firstTriangle = 0;
	root.triangleCount = numTriangles;
	data_.bvhNodes.push_back(root);

	UpdateNodeBounds(0);
	SubdivideBVH(0);

	// ★追加: GPU用バッファの構築と転送
	if (!data_.bvhNodes.empty()) {
		// ※本来は専用のUPLOAD→DEFAULT遷移が必要だが、このエンジンのCreateBufferResourceは
		// UPLOADヒープで作られているため、そのままMapしてコピーする。
		vbBvhNodes_ = CreateBufferResource(nullptr, data_.bvhNodes.size() * sizeof(BVHNode));
		void* nodePtr = nullptr;
		vbBvhNodes_->Map(0, nullptr, &nodePtr);
		std::memcpy(nodePtr, data_.bvhNodes.data(), data_.bvhNodes.size() * sizeof(BVHNode));
		vbBvhNodes_->Unmap(0, nullptr);

		vbBvhIndices_ = CreateBufferResource(nullptr, data_.bvhIndices.size() * sizeof(uint32_t));
		void* indexPtr = nullptr;
		vbBvhIndices_->Map(0, nullptr, &indexPtr);
		std::memcpy(indexPtr, data_.bvhIndices.data(), data_.bvhIndices.size() * sizeof(uint32_t));
		vbBvhIndices_->Unmap(0, nullptr);
	}
}

void Model::UpdateNodeBounds(uint32_t nodeIdx) {
	BVHNode& node = data_.bvhNodes[nodeIdx];
	node.min = {FLT_MAX, FLT_MAX, FLT_MAX};
	node.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

	for (uint32_t i = 0; i < node.triangleCount; ++i) {
		uint32_t triIdx = data_.bvhIndices[node.firstTriangle + i];
		for (int v = 0; v < 3; ++v) {
			const auto& pos = data_.vertices[data_.indices[triIdx * 3 + v]].position;
			node.min.x = (std::min)(node.min.x, pos.x);
			node.min.y = (std::min)(node.min.y, pos.y);
			node.min.z = (std::min)(node.min.z, pos.z);
			node.max.x = (std::max)(node.max.x, pos.x);
			node.max.y = (std::max)(node.max.y, pos.y);
			node.max.z = (std::max)(node.max.z, pos.z);
		}
	}
}

void Model::SubdivideBVH(uint32_t nodeIdx) {
	// ノードの参照を取得（注意：bvhNodesの再確保により無効になる可能性があるため、分割後に再取得する）
	if (data_.bvhNodes[nodeIdx].triangleCount <= 4) return;

	// 分割軸の決定 (最も長い軸を選択)
	Vector3 extent = data_.bvhNodes[nodeIdx].max - data_.bvhNodes[nodeIdx].min;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > (axis == 0 ? extent.x : extent.y)) axis = 2;

	float splitPos = ((axis == 0 ? data_.bvhNodes[nodeIdx].min.x : (axis == 1 ? data_.bvhNodes[nodeIdx].min.y : data_.bvhNodes[nodeIdx].min.z)) +
					  (axis == 0 ? data_.bvhNodes[nodeIdx].max.x : (axis == 1 ? data_.bvhNodes[nodeIdx].max.y : data_.bvhNodes[nodeIdx].max.z))) * 0.5f;

	// 三角形を左右に分ける (in-place partitioning)
	int i = data_.bvhNodes[nodeIdx].firstTriangle;
	int j = data_.bvhNodes[nodeIdx].firstTriangle + data_.bvhNodes[nodeIdx].triangleCount - 1;
	while (i <= j) {
		uint32_t triIdx = data_.bvhIndices[i];
		Vector3 center = (Vector3{data_.vertices[data_.indices[triIdx * 3]].position.x, data_.vertices[data_.indices[triIdx * 3]].position.y, data_.vertices[data_.indices[triIdx * 3]].position.z} +
						  Vector3{data_.vertices[data_.indices[triIdx * 3 + 1]].position.x, data_.vertices[data_.indices[triIdx * 3 + 1]].position.y, data_.vertices[data_.indices[triIdx * 3 + 1]].position.z} +
						  Vector3{data_.vertices[data_.indices[triIdx * 3 + 2]].position.x, data_.vertices[data_.indices[triIdx * 3 + 2]].position.y, data_.vertices[data_.indices[triIdx * 3 + 2]].position.z}) / 3.0f;
		
		float val = (axis == 0 ? center.x : (axis == 1 ? center.y : center.z));
		if (val < splitPos) i++;
		else {
			std::swap(data_.bvhIndices[i], data_.bvhIndices[j]);
			j--;
		}
	}

	uint32_t leftCount = i - data_.bvhNodes[nodeIdx].firstTriangle;
	if (leftCount == 0 || leftCount == data_.bvhNodes[nodeIdx].triangleCount) return;

	uint32_t firstTri = data_.bvhNodes[nodeIdx].firstTriangle;
	uint32_t triCount = data_.bvhNodes[nodeIdx].triangleCount;

	// 子ノードを作成
	int leftIdx = (int)data_.bvhNodes.size();
	BVHNode left{};
	left.firstTriangle = firstTri;
	left.triangleCount = leftCount;
	data_.bvhNodes.push_back(left);

	int rightIdx = (int)data_.bvhNodes.size();
	BVHNode right{};
	right.firstTriangle = i;
	right.triangleCount = triCount - leftCount;
	data_.bvhNodes.push_back(right);

	// 親ノードを更新
	data_.bvhNodes[nodeIdx].leftChild = leftIdx;
	data_.bvhNodes[nodeIdx].rightChild = rightIdx;
	data_.bvhNodes[nodeIdx].triangleCount = 0;

	UpdateNodeBounds(leftIdx);
	UpdateNodeBounds(rightIdx);

	SubdivideBVH(leftIdx);
	SubdivideBVH(rightIdx);
}

bool Model::RayIntersectsAABB(const DirectX::XMVECTOR& rayOrig, const DirectX::XMVECTOR& rayDir, const Vector3& bmin, const Vector3& bmax, float& tOut) {
	XMFLOAT3 orig; XMStoreFloat3(&orig, rayOrig);
	XMFLOAT3 dir; XMStoreFloat3(&dir, rayDir);
	float tmin = -FLT_MAX, tmax = FLT_MAX;
	float mn[3] = {bmin.x, bmin.y, bmin.z};
	float mx[3] = {bmax.x, bmax.y, bmax.z};
	float o[3] = {orig.x, orig.y, orig.z};
	float d[3] = {dir.x, dir.y, dir.z};
	for (int i = 0; i < 3; ++i) {
		if (std::fabs(d[i]) < 1e-8f) {
			if (o[i] < mn[i] || o[i] > mx[i]) return false;
		} else {
			float t1 = (mn[i] - o[i]) / d[i];
			float t2 = (mx[i] - o[i]) / d[i];
			if (t1 > t2) std::swap(t1, t2);
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
			if (tmin > tmax) return false;
		}
	}
	if (tmax < 0) return false;
	tOut = tmin > 0 ? tmin : tmax;
	return true;
}

bool Model::RayCast(const DirectX::XMVECTOR& rayOrig, const DirectX::XMVECTOR& rayDir, const Matrix4x4& worldTransform, float& outDist, Vector3& outHitPoint) const {
	if (data_.bvhNodes.empty() || data_.indices.empty()) return false;

	// XMMATRIXに変換
	DirectX::XMMATRIX worldMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&worldTransform));
	DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(nullptr, worldMat);

	// ワールド空間のRayをローカル空間に変換
	DirectX::XMVECTOR localOrig = DirectX::XMVector3TransformCoord(rayOrig, invWorld);
	DirectX::XMVECTOR localDir = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayDir, invWorld));

	float minDistLocal = FLT_MAX;
	bool hit = false;
	
	std::vector<uint32_t> stack;
	stack.push_back(0);

	while (!stack.empty()) {
		uint32_t nodeIdx = stack.back();
		stack.pop_back();

		const BVHNode& node = data_.bvhNodes[nodeIdx];
		float tBox;
		if (!RayIntersectsAABB(localOrig, localDir, node.min, node.max, tBox)) {
			continue;
		}
		if (tBox >= minDistLocal) {
			continue; // これ以上近い結果は望めない
		}

		if (node.leftChild == -1) {
			// 葉ノード: 三角形と交差判定
			for (uint32_t i = 0; i < node.triangleCount; ++i) {
				uint32_t triIdx = data_.bvhIndices[node.firstTriangle + i];
				
				DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&data_.vertices[data_.indices[triIdx * 3]].position));
				DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&data_.vertices[data_.indices[triIdx * 3 + 1]].position));
				DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(&data_.vertices[data_.indices[triIdx * 3 + 2]].position));

				// DirectXMathのTriangleTests機能を使う (bvhで代用)
				// Möller–Trumbore intersection algorithm
				DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
				DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v2, v0);
				DirectX::XMVECTOR h = DirectX::XMVector3Cross(localDir, edge2);
				float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge1, h));

				if (a > -1e-6f && a < 1e-6f) continue; // 平行

				float f = 1.0f / a;
				DirectX::XMVECTOR s = DirectX::XMVectorSubtract(localOrig, v0);
				float u = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(s, h));
				if (u < 0.0f || u > 1.0f) continue;

				DirectX::XMVECTOR q = DirectX::XMVector3Cross(s, edge1);
				float v = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(localDir, q));
				if (v < 0.0f || u + v > 1.0f) continue;

				float t = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge2, q));
				if (t > 1e-6f && t < minDistLocal) {
					minDistLocal = t;
					hit = true;
				}
			}
		} else {
			// 子ノードへ（近い方から処理するとより効率的だが、今回はスタックにそのまま積む）
			stack.push_back(node.leftChild);
			stack.push_back(node.rightChild);
		}
	}

	if (hit) {
		// ローカル上の交差座標を計算
		DirectX::XMVECTOR hitLocal = DirectX::XMVectorAdd(localOrig, DirectX::XMVectorScale(localDir, minDistLocal));
		// ワールド座標に戻す
		DirectX::XMVECTOR hitWorld = DirectX::XMVector3TransformCoord(hitLocal, worldMat);
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&outHitPoint), hitWorld);
		// 距離を計算 (ワールド空間上)
		outDist = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(hitWorld, rayOrig)));
		return true;
	}

	return false;
}

void Model::UpdateSkeleton(const Node& node, const Matrix4x4& parentMatrix, const Animation& animation, float time, std::vector<Matrix4x4>& skeletonParams, std::vector<std::pair<Vector3, Vector3>>* debugLines) {
	Matrix4x4 localTransform = node.localMatrix;

	auto it = animation.nodeAnimations.find(node.name);
	if (it != animation.nodeAnimations.end()) {
		const NodeAnimation& nodeAnim = it->second;
		Vector3 trans = CalculateTranslation(nodeAnim.translations, time);
		XMFLOAT4 rot = CalculateRotation(nodeAnim.rotations, time);
		Vector3 scale = CalculateScale(nodeAnim.scales, time);

		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rot));
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(trans.x, trans.y, trans.z);
		localTransform = XMToM4(S * R * T);
	}

	DirectX::XMMATRIX localMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&localTransform));
	DirectX::XMMATRIX parentMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&parentMatrix));
	Matrix4x4 globalTransform = XMToM4(localMat * parentMat);

	if (debugLines) {
		Vector3 pParent = { parentMatrix.m[3][0], parentMatrix.m[3][1], parentMatrix.m[3][2] };
		Vector3 pCurrent = { globalTransform.m[3][0], globalTransform.m[3][1], globalTransform.m[3][2] };
		if (node.name != "RootNode") {
			debugLines->push_back({ pParent, pCurrent });
		}
	}

	auto boneIt = data_.boneMapping.find(node.name);
	if (boneIt != data_.boneMapping.end()) {
		int boneIndex = boneIt->second;
		if (boneIndex < (int)skeletonParams.size()) {
			DirectX::XMMATRIX offsetMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&data_.bones[boneIndex].offsetMatrix));
			DirectX::XMMATRIX globalMat = DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&globalTransform));
			skeletonParams[boneIndex] = XMToM4(offsetMat * globalMat);
		}
	}

	for (const Node& child : node.children) {
		UpdateSkeleton(child, globalTransform, animation, time, skeletonParams, debugLines);
	}
}

void Skeleton::Update() {
	for (Joint& joint : joints) {
		joint.localMatrix = joint.transform.ToMatrix();
		if (joint.parent) {
			joint.skeletonSpaceMatrix = Matrix4x4::Multiply(joint.localMatrix, joints[*joint.parent].skeletonSpaceMatrix);
		} else {
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

} // namespace Engine