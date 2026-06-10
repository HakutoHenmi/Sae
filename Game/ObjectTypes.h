#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <type_traits>
#include <cmath>
#include <algorithm>
#include "Transform.h"
#include <DirectXMath.h>
#include <cstdint>
#include <vector>
#include <string>
#include "../Engine/ParticleEmitter.h"
#include <map>
#include <memory> 

#include "../../externals/entt/entt.hpp"

namespace Game {

class IScript; // ★追加: C++スクリプトの基底クラス前方宣言

enum class ObjectType : uint32_t {
	Cube = 0,
	Slope = 1,
	Ball = 2,
	LongFloor = 3,
	Model = 999,
};

struct CollisionMeshData {
	std::vector<DirectX::XMVECTOR> vertices;
	std::vector<int> indices;
};

// ★追加: 衝突情報（GPU読み戻し用）
struct ContactInfo {
	DirectX::XMFLOAT3 normal;   // offset 0
	float depth;                // offset 12
	DirectX::XMFLOAT3 position; // offset 16
	uint32_t intersected;     // offset 28
};

// コンポーネント
enum class ComponentType { 
	MeshRenderer, BoxCollider, Tag, Animator, Rigidbody, ParticleEmitter, 
	GpuMeshCollider, PlayerInput, CharacterMovement, CameraTarget,
	DirectionalLight, PointLight, SpotLight,
	AudioSource, AudioListener, Hitbox, Hurtbox, Health, // ★追加: 音響 & 戦闘判定 & ステータス
	Script, // ★追加: スクリプトコンポーネント
	RectTransform, UIImage, UIText, UIButton, // ★追加: UIコンポーネント
	River, // ★追加: 川コンポーネント
	Variable, // ★追加: 汎用変数
	WorldSpaceUI, // ★追加: ワールド空間UI
	Motion, // ★追加: モーションエディタ用
	PlayerAction, // ★追加: プレイヤーアクション（攻撃・パリィ・回避）
	BossAction,   // ★追加: ボス用アクションステートマシン
	BodyPart      // ★追加: ボス部位破壊用
};
struct Component { 
	ComponentType type = ComponentType::MeshRenderer; 
	bool enabled = true; 
	Component() = default;
	Component(ComponentType t) : type(t), enabled(true) {}
};

struct NameComponent {
	std::string name = "Object";
	NameComponent() = default;
	NameComponent(const std::string& n) : name(n) {}
};

struct HierarchyComponent {
	entt::entity parentId = entt::null;
};

struct TransformComponent : public Component {
	DirectX::XMFLOAT3 translate = {0, 0, 0};
	DirectX::XMFLOAT3 rotate = {0, 0, 0};
	DirectX::XMFLOAT3 scale = {1, 1, 1};
	
	TransformComponent() { type = ComponentType::RectTransform; }
	TransformComponent(const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT3& r, const DirectX::XMFLOAT3& s)
		: translate(t), rotate(r), scale(s) { type = ComponentType::RectTransform; }

	Engine::Transform GetTransform() const {
		Engine::Transform t;
		t.translate = {translate.x, translate.y, translate.z};
		t.rotate = {rotate.x, rotate.y, rotate.z};
		t.scale = {scale.x, scale.y, scale.z};
		return t;
	}
	Engine::Matrix4x4 ToMatrix() const {
		using namespace DirectX;
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX R = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
		XMMATRIX T = XMMatrixTranslation(translate.x, translate.y, translate.z);
		Engine::Matrix4x4 out;
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), S * R * T);
		return out;
	}
};

struct ColorComponent {
	DirectX::XMFLOAT4 color = {1, 1, 1, 1};
};

struct EditorStateComponent {
	bool locked = false;
	bool isPendingDestroy = false;
};

struct MeshRendererComponent : public Component {
	uint32_t modelHandle = 0;
	uint32_t textureHandle = 0;
	std::string modelPath;
	std::string texturePath;
	DirectX::XMFLOAT4 color = {1, 1, 1, 1};
	// ★追加: マテリアル/ライトマッププロパティ
	DirectX::XMFLOAT2 uvTiling = {1, 1};
	DirectX::XMFLOAT2 uvOffset = {0, 0};
	uint32_t lightmapHandle = 0;
	std::string lightmapPath;
	std::vector<uint32_t> extraTextureHandles; // ★追加
	std::vector<std::string> extraTexturePaths; // ★追加
	std::string shaderName = "Default"; // ★追加
	float reflectivity = 0.0f; // ★追加: 環境マップ反射率 (0.0〜1.0)
	bool useCubemap = true; // ★追加: キューブマップ使用判定 (初期値trueで課題要件をデフォルトに)
	MeshRendererComponent() { type = ComponentType::MeshRenderer; }
};

struct BoxColliderComponent : public Component {
	DirectX::XMFLOAT3 center = {0, 0, 0};
	DirectX::XMFLOAT3 size = {1, 1, 1};
	bool isTrigger = false;
	BoxColliderComponent() { type = ComponentType::BoxCollider; }
};

enum class MeshCollisionType {
	Mesh,   // 全ポリゴン
	Convex, // 凸包近似（簡易化）
};

// ★追加: GPUメッシュコライダーコンポーネント
struct GpuMeshColliderComponent : public Component {
	uint32_t meshHandle = 0;
	std::string meshPath = "";
	MeshCollisionType collisionType = MeshCollisionType::Mesh; // 追加
	bool isTrigger = false;
	bool isIntersecting = false; // 衝突結果格納用
	GpuMeshColliderComponent() { type = ComponentType::GpuMeshCollider; }
};

// ★追加: アニメーターコンポーネント
struct AnimatorComponent : public Component {
	std::string currentAnimation;
	float time = 0.0f;
	float speed = 1.0f;
	bool isPlaying = false;
	bool loop = true;
	bool drawSkeleton = false; // ★追加: デバッグ用スケルトン表示
	AnimatorComponent() { type = ComponentType::Animator; }
};

enum class TagType : uint32_t {
	Untagged = 0,
	Player,
	Enemy,
	Bullet,
	Projectile,
	Wall,
	Default,
	VFX
};

inline const char* TagToString(TagType tag) {
	switch (tag) {
	case TagType::Player: return "Player";
	case TagType::Enemy: return "Enemy";
	case TagType::Bullet: return "Bullet";
	case TagType::Projectile: return "Projectile";
	case TagType::Wall: return "Wall";
	case TagType::VFX: return "VFX";
	case TagType::Default: return "Default";
	default: return "Untagged";
	}
}

inline TagType StringToTag(const std::string& s) {
	if (s == "Player") return TagType::Player;
	if (s == "Enemy") return TagType::Enemy;
	if (s == "Bullet") return TagType::Bullet;
	if (s == "Projectile") return TagType::Projectile;
	if (s == "Wall") return TagType::Wall;
	if (s == "VFX") return TagType::VFX;
	if (s == "Default") return TagType::Default;
	return TagType::Untagged;
}

struct TagComponent : public Component {
	TagType tag = TagType::Untagged;
	TagComponent() { type = ComponentType::Tag; }
	TagComponent(TagType t) : tag(t) { type = ComponentType::Tag; }
};

// ★追加: 武器タイプ
enum class WeaponType {
	Greatsword,    // 大剣
	DualBlades,    // 双剣
	MultiBlades    // 浮遊機巧剣（ファンネルソード）
};

// ★追加: プレイヤーの武器管理
struct PlayerWeaponComponent : public Component {
	WeaponType currentWeapon = WeaponType::Greatsword;
	std::vector<entt::entity> weaponEntities; // 武器の視覚モデルエンティティ
	PlayerWeaponComponent() { type = static_cast<ComponentType>(999); }
};

// ★追加: プレイヤー入力 (意思)
struct PlayerInputComponent : public Component {
	DirectX::XMFLOAT2 moveDir = {0, 0};
	bool jumpRequested = false;
	bool attackRequested = false;

	// ★追加: マウス操作によるカメラの旋回量（intent）
	float cameraYaw = 0.0f;
	float cameraPitch = 0.0f;
	
	entt::entity lockedEnemy = entt::null; // ★追加: ロックオン中の敵エンティティ

	PlayerInputComponent() { type = ComponentType::PlayerInput; }
};

// ★追加: キャラクター移動 (能力)
struct CharacterMovementComponent : public Component {
	float speed = 5.0f;
	float jumpPower = 12.0f;
	float gravity = 9.8f;
	float velocityY = 0.0f;
	float heightOffset = 1.0f; // ★追加: 地面からの高度オフセット
	bool isGrounded = false;
	bool enabled = true;
	CharacterMovementComponent() { type = ComponentType::CharacterMovement; }
};

// ★追加: カメラ追従対象 (属性)
struct CameraTargetComponent : public Component {
	float distance = 10.0f;
	float height = 3.0f;
	float smoothSpeed = 5.0f;
	CameraTargetComponent() { type = ComponentType::CameraTarget; }
};

struct RigidbodyComponent : public Component {
	DirectX::XMFLOAT3 velocity = {0.0f, 0.0f, 0.0f};
	bool useGravity = true;
	bool isKinematic = false;
	RigidbodyComponent() { type = ComponentType::Rigidbody; }
};

// ★追加: パーティクルエミッターコンポーネント
struct ParticleEmitterComponent : public Component {
	Engine::ParticleEmitter emitter;
	std::string assetPath = ""; // .particle ファイルのパス
	bool isInitialized = false;

	ParticleEmitterComponent() { type = ComponentType::ParticleEmitter; }
};

// ★追加: ライトコンポーネント (Directional, Point, Spot)
struct DirectionalLightComponent : public Component {
	DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
	float intensity = 1.0f;
	DirectionalLightComponent() { type = ComponentType::DirectionalLight; }
};

struct PointLightComponent : public Component {
	DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
	float intensity = 1.0f;
	float range = 10.0f;
	DirectX::XMFLOAT3 atten = {1.0f, 0.1f, 0.01f}; // 減衰(一定, 線形, 二次)
	PointLightComponent() { type = ComponentType::PointLight; }
};

struct SpotLightComponent : public Component {
	DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
	float intensity = 1.0f;
	float range = 20.0f;
	float innerCos = 0.98f; // cos(内側角度)
	float outerCos = 0.90f; // cos(外側角度)
	DirectX::XMFLOAT3 atten = {1.0f, 0.1f, 0.01f};
	SpotLightComponent() { type = ComponentType::SpotLight; }
};

enum class AudioCategory { BGM, SE };

// ★追加: AudioSource コンポーネント (音の発信源)
struct AudioSourceComponent : public Component {
	std::string soundPath = "";       // 音声ファイルパス
	uint32_t soundHandle = 0xFFFFFFFF; // Audio::Load()で取得したハンドル
	size_t voiceHandle = 0;            // 再生中のボイスハンドル
	float volume = 1.0f;               // 音量 (0.0〜1.0)
	bool loop = false;                 // ループ再生
	bool playOnStart = false;          // Play時に自動再生
	bool is3D = true;                  // 3Dサウンド（距離減衰あり）
	float maxDistance = 50.0f;         // 減衰最大距離
	bool isPlaying = false;            // 再生中フラグ
	AudioCategory category = AudioCategory::SE; // ★追加: カテゴリ
	AudioSourceComponent() { type = ComponentType::AudioSource; }
};

// ★追加: AudioListener コンポーネント (音の聞き取り位置、通常はカメラにアタッチ)
struct AudioListenerComponent : public Component {
	AudioListenerComponent() { type = ComponentType::AudioListener; }
};

// ★追加: Hitbox コンポーネント (攻撃判定)
struct HitboxComponent : public Component {
	DirectX::XMFLOAT3 center = {0, 0, 0}; // ローカルオフセット
	DirectX::XMFLOAT3 size = {1, 1, 1};    // 判定サイズ
	float damage = 10.0f;                  // ダメージ量
	bool isActive = false;                 // 有効フラグ（攻撃アニメ中のみtrue等）
	TagType tag = TagType::Default;           // 識別タグ ("Sword", "Projectile"等)
	std::vector<entt::entity> hitTargets; // ★追加: 1回の攻撃で既にヒットした対象（多段ヒット防止）
	HitboxComponent() { type = ComponentType::Hitbox; }
};

// ★追加: Hurtbox コンポーネント (食らい判定)
struct HurtboxComponent : public Component {
	DirectX::XMFLOAT3 center = {0, 0, 0}; // ローカルオフセット
	DirectX::XMFLOAT3 size = {1, 1, 1};    // 判定サイズ
	TagType tag = TagType::Default;              // 識別タグ
	float damageMultiplier = 1.0f;         // ダメージ倍率 (頭部=2.0等)
	HurtboxComponent() { type = ComponentType::Hurtbox; }
};

// ★追加: Health コンポーネント (ステータス管理)
struct HealthComponent : public Component {
	float hp = 100.0f;               // 現在の体力
	float maxHp = 100.0f;            // 最大体力
	float stamina = 100.0f;          // スタミナ
	float maxStamina = 100.0f;       // 最大スタミナ
	float invincibleTime = 0.0f;     // 残り無敵時間（ゼロ以上なら無敵）
	bool isDead = false;             // 死亡フラグ

	// ★追加: ヒット演出用
	float hitFlashTimer = 0.0f;      // ヒットフラッシュ（白く光る）
	float hitStopTimer = 0.0f;       // ヒットストップ（一時停止）
	DirectX::XMFLOAT4 baseColor = { 1, 1, 1, 1 }; // 元の色保存用
	bool baseColorSaved = false;

	HealthComponent() { type = ComponentType::Health; }
};

// ★追加: UIコンポーネント
struct RectTransformComponent : public Component {
	DirectX::XMFLOAT2 pos = {0, 0};   // スクリーン座標
	DirectX::XMFLOAT2 size = {100, 100};
	DirectX::XMFLOAT2 anchor = {0.5f, 0.5f}; // 0.0〜1.0
	DirectX::XMFLOAT2 pivot = {0.5f, 0.5f};
	float rotation = 0.0f;
	RectTransformComponent() { type = ComponentType::RectTransform; }
};

struct UIImageComponent : public Component {
	uint32_t textureHandle = 0;
	std::string texturePath = "";
	DirectX::XMFLOAT4 color = {1, 1, 1, 1};
	bool is9Slice = false;
	float borderTop = 10.0f;
	float borderBottom = 10.0f;
	float borderLeft = 10.0f;
	float borderRight = 10.0f;
	int layer = 0; // ★追加: 描画レイヤー（大きいほど手前）
	UIImageComponent() { type = ComponentType::UIImage; }
};

struct UITextComponent : public Component {
	std::string text = "New Text";
	float fontSize = 24.0f;
	DirectX::XMFLOAT4 color = {1, 1, 1, 1};
	std::string fontPath = "C:\\Windows\\Fonts\\msgothic.ttc"; // フォントファイルパス
	UITextComponent() { type = ComponentType::UIText; }
};

struct UIButtonComponent : public Component {
	bool isHovered = false;
	bool isPressed = false;
	DirectX::XMFLOAT4 normalColor = {1, 1, 1, 1};
	DirectX::XMFLOAT4 hoverColor = {0.8f, 0.8f, 0.8f, 1.0f};
	DirectX::XMFLOAT4 pressedColor = {0.6f, 0.6f, 0.6f, 1.0f};
	std::string onClickCallback = ""; // スクリプト側のメソッド名など
	
	// ★追加: 判定エリアの個別調整用
	DirectX::XMFLOAT2 hitboxOffset = {0.0f, 0.0f};
	DirectX::XMFLOAT2 hitboxScale = {1.0f, 1.0f};

	UIButtonComponent() { type = ComponentType::UIButton; }
};

// ★追加: スクリプトの個体エントリ
struct ScriptEntry {
	std::string scriptPath = "";      // スクリプトのクラス名 (例: "PlayerScript")
	std::string parameterData = "";   // 初期化用パラメータ(JSON)
	std::shared_ptr<IScript> instance = nullptr; // C++スクリプトのインスタンス
	bool isStarted = false; // PlayモードでStart()が呼ばれたか
};

// ★変更: Script コンポーネント (マルチスクリプト対応)
struct ScriptComponent : public Component {
	std::vector<ScriptEntry> scripts;
	ScriptComponent() { type = ComponentType::Script; }
};

// ★追加: River コンポーネント
struct RiverComponent : public Component {
	std::vector<DirectX::XMFLOAT3> points; // スプライン制御点 (ローカル座標)
	float width = 2.0f;                    // 川の基本幅
	float flowSpeed = 1.0f;                // 流れの速さ
	float uvScale = 1.0f;
	uint32_t meshHandle = 0;               // 動的生成メッシュハンドル
	std::string texturePath = "Resources/Water/water.png";
	RiverComponent() { type = ComponentType::River; }
};

// ★追加: ワールド空間UIコンポーネント
struct WorldSpaceUIComponent : public Component {
	bool showHealthBar = true;
	bool showDamageNumbers = true;
	DirectX::XMFLOAT3 offset = {0, 1.2f, 0};
	float barWidth = 60.0f;
	float barHeight = 6.0f;
	WorldSpaceUIComponent() { type = ComponentType::WorldSpaceUI; }
};

// ★追加: ダメージ数字コンポーネント
struct DamageNumberComponent : public Component {
	float damage = 0.0f;
	float lifetime = 1.0f;
	float maxLifetime = 1.0f;
	DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
	DirectX::XMFLOAT3 startPos = {0,0,0};
	DamageNumberComponent() { type = static_cast<ComponentType>(999); } // enumは不要
};

// パリィの歪みエフェクト用コンポーネント（地面の衝撃波にも流用）
struct ParryDistortionComponent {
	float timer = 0.0f;
	float duration = 0.5f;
	float startScale = 0.5f;
	float endScale = 12.0f;
	bool isBillboard = true; // 常にカメラを向くか（地面の衝撃波用はfalse）
};

// ★追加: 自動削除コンポーネント
struct AutoDestroyComponent : public Component {
	float timer = 1.0f;
	AutoDestroyComponent() { type = static_cast<ComponentType>(999); }
};

// ★追加: ボスの攻撃タイプ
enum class BossAttackType {
	Thrust,       // 突進（デフォルト）
	TailSpin,     // 尻尾なぎ払い（大回転）
	JumpPress     // 飛びかかりプレス（衝撃波）
};

// ★追加: ボス専用アクションステートマシン用
struct BossActionPattern {
	std::string name;
	BossAttackType type = BossAttackType::Thrust; // 攻撃のタイプ
	float windUpDuration = 0.8f;
	float activeDuration = 0.3f;
	float recoveryDuration = 1.0f;
	float range = 5.0f;
	float damage = 20.0f;
	float thrustForce = 10.0f; // 攻撃時の前進力（Thrust用）
};

enum class BossState : uint32_t {
	Idle = 0, Chase, WindUp, Attack, Cooldown, Stunned, Down, Dead
};

struct BossActionComponent : public Component {
	BossState state = BossState::Idle;
	float stateTimer = 0.0f;
	
	float chaseSpeed = 3.0f;
	float rotationSpeed = 4.0f;
	float stunDuration = 2.0f; // パリィされた時のスタン時間
	
	int currentPatternIndex = -1;
	std::vector<BossActionPattern> patterns;
	
	BossActionComponent() { type = ComponentType::BossAction; }
};

// ★追加: ボスの部位破壊用コンポーネント
struct BodyPartComponent : public Component {
	entt::entity parentEntity = entt::null; // 本体（BossActionを持つエンティティ）
	float hp = 100.0f;
	float maxHp = 100.0f;
	bool isDestroyed = false;
	float damageMultiplierToParent = 0.5f; // 部位へのダメージが本体にどれくらい入るか
	std::string partName = "Part";
	BodyPartComponent() { type = ComponentType::BodyPart; }
};

// ★追加: 汎用変数コンポーネント (スクリプト間通信用)
struct VariableComponent : public Component {
	std::map<std::string, float> values;
	std::map<std::string, std::string> strings; // ★追加: 文字列型
	VariableComponent() { type = ComponentType::Variable; }

	float GetValue(const std::string& key, float defaultVal = 0.0f) const {
		auto it = values.find(key);
		return (it != values.end()) ? it->second : defaultVal;
	}
	void SetValue(const std::string& key, float val) {
		values[key] = val;
	}

	std::string GetString(const std::string& key, const std::string& defaultVal = "") const {
		auto it = strings.find(key);
		return (it != strings.end()) ? it->second : defaultVal;
	}
	void SetString(const std::string& key, const std::string& val) {
		strings[key] = val;
	}
};

// ★追加: モーションエディタ用コンポーネント
struct MotionComponent : public Component {
	struct Keyframe {
		float time = 0.0f;
		DirectX::XMFLOAT3 translate = {0, 0, 0};
		DirectX::XMFLOAT3 rotate = {0, 0, 0};
		DirectX::XMFLOAT3 scale = {1, 1, 1};
	};

	struct MotionClip {
		std::string name;
		std::vector<Keyframe> keyframes;
		float totalDuration = 1.0f;
		bool loop = false;
	};

	std::map<std::string, MotionClip> clips;
	std::string activeClip = "Default";
	
	float currentTime = 0.0f;
	bool isPlaying = false;
	int selectedKeyframe = -1; // エディタ用

	MotionComponent() { 
		type = ComponentType::Motion; 
		// 初期値としてデフォルトクリップを作成
		MotionClip defaultClip;
		defaultClip.name = "Default";
		defaultClip.keyframes.push_back({0.0f, {0,0,0}, {0,0,0}, {1,1,1}});
		defaultClip.keyframes.push_back({1.0f, {5,0,0}, {0,0,0}, {1,1,1}});
		clips["Default"] = defaultClip;
	}

	void PlayAnimation(const std::string& clipName) {
		if (clips.find(clipName) != clips.end()) {
			activeClip = clipName;
			currentTime = 0.0f;
			isPlaying = true;
		}
	}
};

// ★ エディター・共通システム用：Entity情報の構成要素としてのコンポーネント化

// ★ EntityのIDそのものをラップする構造体（またはそのまま entt::entity を使う）
using Entity = entt::entity;


} // namespace Game