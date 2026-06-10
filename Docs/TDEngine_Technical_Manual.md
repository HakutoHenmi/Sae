# TD Engine 技術マニュアル (Technical Manual)

## 1. アーキテクチャ概要 (Core Architecture)

TD Engine は、**Entity-Component-System (ECS)** の思想をベースにしたハイブリッドなゲームエンジンです。

### 1.1 構成要素
- **SceneObject (Entity)**: すべてのゲーム内オブジェクトの基体です。位置、回転、スケールを持ちますが、それ以外の機能はすべて後述のコンポーネントによって提供されます。
- **Component (Data)**: オブジェクトの状態や設定（メッシュ、物理パラメータ、ステータス等）を保持するデータ構造体です。
- **System (Process)**: 全ての `SceneObject` を走査し、特定のコンポーネントを持つものに対して一括でロジックを適用する「裏方」です。
- **IScript (Object Logic)**: 各オブジェクト固有の非凡な挙動を C++ で記述するための仕組みです。

---

## 2. SceneObject とコンポーネントシステム

`SceneObject` は各コンポーネントの **vector** を保持しています。

| コンポーネント名 | 用途 |
| :--- | :--- |
| `MeshRenderer` | 3Dモデル（.obj）とテクスチャの表示 |
| `BoxCollider` | AABBによる衝突判定（トリガー設定可） |
| `Rigidbody` | 物理演算（重力、速度、運動量） |
| `Tag` | オブジェクトを識別するための文字列ラベル |
| `Animator` | 骨格アニメーションの管理 |
| `GpuMeshCollider` | GPUによる高精度なメッシュ衝突判定 |
| `Hitbox / Hurtbox` | 攻撃判定と食らい判定。対戦ゲーム等の実装用 |
| `Health` | HP、スタミナ、無敵時間の管理 |
| `AudioSource` | 3D空間上での音源再生 |
| `ParticleEmitter` | エフェクト（.particle）の発火管理 |
| `Script` | `IScript` を継承した C++ クラスのアタッチ |

---

## 3. レンダリング API (Renderer)

描画はシングルトン `Engine::Renderer` が担当します。

### 3.1 主要な描画メソッド
```cpp
auto* renderer = Engine::Renderer::GetInstance();

// 3Dモデルの描画
renderer->DrawMesh(modelHandle, textureHandle, transform, color);

// パーティクルの描画 (UVアニメーション対応)
renderer->DrawParticle(mesh, tex, transform, color, uvScaleOffset);

// デバッグ用ライン描画
renderer->DrawLine3D(posStart, posEnd, color, xray);
```

### 3.2 ライト設定
```cpp
// 環境光
renderer->SetAmbientColor({0.4f, 0.4f, 0.45f});
// 平行光源
renderer->SetDirectionalLight({0, -1, 0}, {1, 1, 1});
// 点光源 (最大4つ)
renderer->SetPointLight(index, pos, color, range);
// スポットライト (最大4つ)
renderer->SetSpotLight(index, pos, dir, color, range, innerCos, outerCos);
```

---

## 4. スクリプト作成ガイド (IScript)

`IScript` を継承することで、独自のゲームロジックを作成できます。

### 4.1 ライフサイクル
1. **`Start`**: オブジェクトがシーンに追加された直後に呼ばれます。
2. **`Update`**: 毎フレーム呼ばれます。`dt` (DeltaTime) を使用して移動を行います。
3. **`OnDestroy`**: オブジェクトが `isDead = true` になり削除される直前に呼ばれます。

### 4.2 スクリプトの登録 (マクロ)
クラス定義の下に必ず以下のマクロを記述してください。
```cpp
// PlayerScript.cpp 等の末尾
REGISTER_SCRIPT(Game::PlayerScript);
```

---

## 5. 実践サンプルコード集

### 5.1 キャラクター移動とコンポーネント操作
```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    // 1. 入力コンポーネントから移動方向を取得 (または直接GetAsyncKeyState)
    if (!obj.playerInputs.empty()) {
        auto& input = obj.playerInputs[0];
        float moveX = input.moveDir.x;
        float moveZ = input.moveDir.y;

        // 2. 座標の更新
        obj.translate.x += moveX * 5.0f * dt;
        obj.translate.z += moveZ * 5.0f * dt;
    }

    // 3. 他のコンポーネント（例：アニメーター）の操作
    if (!obj.animators.empty()) {
        obj.animators[0].currentAnimation = "Run";
        obj.animators[0].isPlaying = true;
    }
}
```

### 5.2 弾の発射とヒット時の処理
```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    // スペースキーで弾をスポーン
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        SceneObject bullet;
        bullet.name = "Bullet";
        bullet.translate = obj.translate; // 自身の位置を使用
        
        // 描画設定（簡易）
        auto* renderer = Engine::Renderer::GetInstance();
        bullet.modelHandle = renderer->LoadObjMesh("Resources/sphere.obj");
        
        // 当たり判定 (Hitbox) を追加
        HitboxComponent hb;
        hb.isActive = true;
        hb.damage = 10.0f;
        bullet.hitboxes.push_back(hb);

        // 飛ぶためのスクリプト
        ScriptComponent sc;
        sc.scriptPath = "BulletScript";
        bullet.scripts.push_back(sc);

        scene->SpawnObject(bullet);
    }

    // 自身の死亡チェック
    if (!obj.healths.empty() && obj.healths[0].hp <= 0) {
        obj.isDead = true; // CleanupSystemが自動回収
    }
}
```

---

## 6. エディターとリリース時の使い分け

### 6.1 `USE_IMGUI` の活用
デバッグ時のみ表示したいコードは、プリプロセッサで囲みます。
```cpp
#ifdef USE_IMGUI
    // エディター専用のギズモ描画など
    ImGui::Begin("Debug Window");
    ImGui::Text("Player HP: %.1f", obj.healths[0].hp);
    ImGui::End();
#endif
```

### 6.2 シーンの自動ロード
シーンの `Initialize` で `Resources/TPS_Scene.json` 等を判定してロードすることで、リリースビルドでも配置したマップが反映されます。

---

## 7. トラブルシューティング
- **画面が真っ暗**: カメラの `SetProjection` が呼ばれているか確認してください。
- **オブジェクトが重ならない**: どちらかに `BoxCollider` がない、または `Rigidbody` が kinematic になっていないか確認してください。
- **スクリプトが動かない**: `REGISTER_SCRIPT` を忘れていないか、パス名が Json と一致しているか確認してください。
