# TD Engine - ゲーム開発マニュアル

## 1. TD Engine の基本概念

TD Engine は**「エンティティ・コンポーネント・システム（ECS）」**と**「スクリプト」**を組み合わせた近代的なアーキテクチャを採用しています。コードを直接シーンにゴリゴリ書くのではなく、**「オブジェクト（Entity）に部品（Component）を貼り付け、スクリプト（Script）で振る舞いを書く」**だけでゲームが作れるよう設計されています。

---

## 2. 登場人物（主要機能）

### 🎮 SceneObject (Entity)
ゲーム世界に存在する「物体」そのものです。プレイヤー、敵、弾、カメラ、見えない判定など、全てが `SceneObject` です。
主なプロパティ：
*   `translate` (X, Y, Z) - 位置
*   `rotate` (X, Y, Z) - 回転
*   `scale` (X, Y, Z) - 大きさ

### 🧩 Component (部品)
`SceneObject` に「能力」を与えます。
*   **MeshRenderer:** 3Dモデルやテクスチャを表示する能力
*   **BoxCollider / GpuMeshCollider:** 物理的な壁や床となる当たり判定
*   **Rigidbody:** 重力に従い、押せば動く物理エンジンの能力
*   **Hitbox / Hurtbox:** 攻撃判定（剣・弾）と被弾判定（体）
*   **Health:** HP（体力）の概念
*   **ParticleEmitter:** パーティクル（火花や爆発）を出す能力
*   **AudioSource:** 3Dサウンド（音が鳴る位置）の能力
*   **CameraTarget:** カメラが自分を追いかける能力
*   **Script:** C++で自由な動きをプログラムする能力

### ⚙️ System (裏方)
Component を持っているオブジェクトを毎フレーム自動で処理するエンジン側の仕組みです。（例：`PhysicsSystem` は `Rigidbody` と `BoxCollider` を見て勝手に衝突計算をします。開発者がシステムを触る必要は基本的にありません。）

---

## 3. ゲームの作り方（ワークフロー）

ゲームを作る基本的な流れは以下の通りです。

1.  **リソースの準備:** `.obj` (3Dモデル) や `.png` (テクスチャ) を `Resources/` フォルダに入れます。
2.  **オブジェクトの配置:** エディタ画面上で右クリックし、空のオブジェクトを作って表示させたいComponent（例：`MeshRenderer`）を付けます。
3.  **当たり判定の追加:** 物理的な壁なら `BoxCollider` を、敵やプレイヤなら `Hurtbox` と `Health` を追加します。
4.  **スクリプトの作成:** 複雑な動き（キーボード入力で動く、敵を追いかけるなど）は C++ スクリプトを作成してオブジェクトにアタッチします。

---

## 4. スクリプト（C++）の書き方

一番の特徴である C++ スクリプトの使い方です。エディタのProjectウィンドウで右クリック > `Create File` > `C++ Script` を選ぶと、自動でテンプレートが生成され VS Code が起動します。

### スクリプトの基本構造

```cpp
#pragma once
#include "IScript.h"

namespace Game {

class MyFirstScript : public IScript {
public:
    // 1. スクリプトが読み込まれ、シーンが始まった直後に1回だけ呼ばれる
    void Start(SceneObject& obj, GameScene* scene) override {
        // 例：初期HPの設定
    }

    // 2. ゲーム中、毎フレーム（1秒間に60回など）呼ばれる
    void Update(SceneObject& obj, GameScene* scene, float dt) override {
        // 例：キーボード入力で obj.translate (位置) を動かす
    }

    // 3. オブジェクトが破壊された時に呼ばれる
    void OnDestroy(SceneObject& obj, GameScene* scene) override {
        // 例：爆発エフェクトを出す
    }
};

} // namespace Game
```

> **重要:** `.cpp` ファイルの一番下には必ず `REGISTER_SCRIPT(Game::MyFirstScript);` が記述されています。これを消さないでください（エンジンがスクリプトを認識できなくなります）。

### よく使うコードスニペット集

#### ① キーボードで移動する
```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    if (GetAsyncKeyState('W') & 0x8000) obj.translate.z += 5.0f * dt;
    if (GetAsyncKeyState('S') & 0x8000) obj.translate.z -= 5.0f * dt;
    if (GetAsyncKeyState('A') & 0x8000) obj.translate.x -= 5.0f * dt;
    if (GetAsyncKeyState('D') & 0x8000) obj.translate.x += 5.0f * dt;
}
```

#### ② 弾を発射する（動的にオブジェクトを生成する）
スクリプトから新しいオブジェクトを作ってシーンに登場させる方法です。

```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
        // 1. 空のオブジェクトを作る
        SceneObject bullet;
        bullet.name = "MagicBullet";
        bullet.translate = obj.translate; // 自分の位置から発射
        
        // 2. 見た目（メッシュとテクスチャ）を設定する
        auto* renderer = scene->GetRenderer();
        if (renderer) {
            bullet.modelHandle = renderer->LoadObjMesh("Resources/sphere.obj");
            bullet.textureHandle = renderer->LoadTexture2D("Resources/red.png");
            MeshRendererComponent mr;
            mr.modelHandle = bullet.modelHandle;
            mr.textureHandle = bullet.textureHandle;
            bullet.meshRenderers.push_back(mr);
        }

        // 3. 攻撃判定（Hitbox）をつける
        HitboxComponent hb;
        hb.isActive = true;
        hb.damage = 50.0f; 
        hb.tag = "PlayerBullet"; // 「誰の弾か」を識別するタグ
        hb.size = {1.0f, 1.0f, 1.0f};
        bullet.hitboxes.push_back(hb);

        // 4. まっすぐ飛ぶスクリプト（別途作成したBulletScript）をつける
        ScriptComponent sc;
        sc.scriptPath = "BulletScript";
        bullet.scripts.push_back(sc);

        // 5. シーンに放つ！
        scene->SpawnObject(bullet);
    }
}
```

#### ③ HPが減ったら消滅する（死亡処理）
オブジェクトが `Hitbox` (攻撃) に触れてダメージを受けると、エンジンが自動で `Health.hp` を減らします。HPが0になった時の処理はスクリプトで書きます。

```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    if (!obj.healths.empty()) {
        if (obj.healths[0].hp <= 0.0f) {
            // 例：敵が死んだら破片を発生させる等
            
            // 重要: isDeadをtrueにするとエンジン(CleanupSystem)が自動でシーンから削除します
            obj.isDead = true; 
        }
    }
}
```

---

## 5. 応用編：物理演算とコンポーネント

### 重力と壁抜け防止
ただ単に `translate.y` を下げるのではなく、**`Rigidbody`** と **`BoxCollider`** コンポーネントを使用してください。
1. `BoxCollider` コンポーネントを追加し、壁や床のサイズに合わせる。
2. キャラクターに `Rigidbody` を追加し `useGravity = true` にする。
3. キャラクターにも `BoxCollider` を追加する。
これだけで、キャラクターは自由落下し、床の上でピタッと止まります。

### スクリプトで物理的な力（ジャンプ）を加える
```cpp
void Update(SceneObject& obj, GameScene* scene, float dt) override {
    if (!obj.rigidbodies.empty()) {
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            // 上(Y軸)に向かって瞬間的に力を加える
            obj.rigidbodies[0].velocity.y = 10.0f;
        }
    }
}
```

---

## 6. ショートカットと便利機能

*   **Ctrl + S:** シーンを保存します。
*   **右クリック (プロジェクト窓):** `Create File` から新しいファイルやPrefabsを作成可能。
*   **ダブルクリック:** スクリプト(`.cpp` / `.h`)、`.json` 設定等をVS Codeで開きます。
*   **ドラッグ＆ドロップ:** リソース窓から `Resources/` 内のフォルダ移動が可能です。
*   **Play Mode (Start/Stop):** 再生ボタンを押すと `System` と `Script` の `Update` が動き出し、ゲームが始まります。終了すると再生前の状態に完全にリセットされます。
