#pragma pack_matrix(row_major)

// b0: フレーム共有情報 (Renderer::CBFrame に対応)
cbuffer ViewProjection : register(b0)
{
    matrix view; // ビュー変換行列
    matrix projection; // プロジェクション変換行列
    matrix viewProj; // ビュープロジェクション行列
    float3 cameraPos; // カメラ座標
    float time; // シェーダー時間
};

// b1: オブジェクト固有情報 (Renderer::CBObj に対応)
cbuffer WorldTransform : register(b1)
{
    matrix world; // ワールド行列
    float4 color; // オブジェクトカラー
    float reflectivity; // ★追加: 環境マップ反射率
    uint useCubemap; // ★追加: キューブマップ使用フラグ
    float2 pad_b1;
};

// 方向光源構造体
struct DirectionalLight
{
    float3 direction;
    float pad0;
    float3 color;
    float pad1;
    uint enabled;
    float3 pad2;
};

// 点光源構造体
struct PointLight
{
    float3 position;
    float pad0;
    float3 color;
    float range;
    float3 atten;
    float pad1;
    uint enabled;
    float3 pad2;
};

// スポットライト構造体
struct SpotLight
{
    float3 position;
    float pad0;
    float3 direction;
    float range;
    float3 color;
    float innerCos;
    float3 atten;
    float outerCos;
    uint enabled;
    float3 pad;
};

// エリアライト構造体 (現状未使用だがバッファ一致用)
struct AreaLight
{
    float3 position;
    float pad0;
    float3 color;
    float range;
    float3 right;
    float halfWidth;
    float3 up;
    float halfHeight;
    float3 direction;
    float pad1;
    float3 atten;
    float pad2;
    uint enabled;
    float3 pad3;
};

// 最大ライト数 (Renderer.h と一致)
#define MAX_DIR_LIGHTS 1
#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4
#define MAX_AREA_LIGHTS 4

// b2: ライト情報 (Renderer::LightCB に対応)
cbuffer LightGroup : register(b2)
{
    float3 ambientColor;
    float pad0;

    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    PointLight pointLights[MAX_POINT_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
    AreaLight areaLights[MAX_AREA_LIGHTS];
    
    matrix shadowMatrix;
};

// マテリアル定数 (固定値)
static const float3 m_ambient = float3(0.3, 0.3, 0.3);
static const float3 m_diffuse = float3(1.0, 1.0, 1.0);
static const float3 m_specular = float3(0.5, 0.5, 0.5);
static const float m_alpha = 1.0;
static const float3 m_uv_scale = float3(1.0, 1.0, 1.0);
static const float3 m_uv_offset = float3(0.0, 0.0, 0.0);

// 頂点シェーダー出力構造体
struct VSOutput
{
    float4 svpos : SV_POSITION; // システム用頂点座標
    float4 worldpos : POSITION; // ワールド座標
    float3 normal : NORMAL; // 法線
    float2 uv : TEXCOORD; // uv値
};