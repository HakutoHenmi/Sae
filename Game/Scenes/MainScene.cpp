#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "MainScene.h"
#include "../ObjectTypes.h"
#include "../../Engine/SceneManager.h"
#include "../../Engine/Renderer.h"
#include "../../Engine/Input.h"
#include "../../Engine/WindowDX.h"
#include "../../externals/imgui/imgui.h"

namespace Game {

void MainScene::Initialize(Engine::WindowDX* dx, const Engine::SceneParameters& /*params*/) {
    dx_ = dx;
    renderer_ = Engine::Renderer::GetInstance();
    lastTime_ = std::chrono::steady_clock::now();

    // Initialize Camera
    camera_.Initialize();
    float aspect = (float)Engine::WindowDX::kW / (float)Engine::WindowDX::kH;
    camera_.SetProjection(DirectX::XMConvertToRadians(60.0f), aspect, 0.1f, 1000.0f);
    camera_.SetPosition({ 0.0f, 5.0f, -10.0f });
    camera_.LookAt({ 0.0f, 0.0f, 0.0f }, { 0, 1, 0 });

    // 背景透過のためSkyboxとPostProcessを無効化
    if (renderer_) {
        renderer_->SetUseCubemapBackground(false);
        renderer_->SetPostProcessEnabled(false);
    }

    // Lighting setup
    renderer_->SetAmbientColor({ 0.3f, 0.3f, 0.3f });
    renderer_->SetDirectionalLight(
        { 0.5f, -1.0f, 0.5f },
        { 1.0f, 1.0f, 1.0f },
        true
    );
    
    // --- Boidsの初期化 ---
    boids_.resize(300); // 300匹の星屑
    swarmCenter_ = { Engine::WindowDX::kW / 2.0f, Engine::WindowDX::kH / 2.0f, 0.0f }; // 全画面の中心

    for (auto& b : boids_) {
        b.position.x = swarmCenter_.x + ((rand() % 1600) - 800.0f);
        b.position.y = swarmCenter_.y + ((rand() % 800) - 400.0f);
        b.position.z = 0.0f;
        
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        b.velocity.x = cosf(angle) * boidSpeed_;
        b.velocity.y = sinf(angle) * boidSpeed_;
        b.velocity.z = 0.0f;
        
        // 色設定（青〜シアン〜白のヒーリングカラー）
        float r = 0.2f + (rand() % 30) / 100.0f;
        float g = 0.6f + (rand() % 40) / 100.0f;
        float b_col = 0.9f + (rand() % 10) / 100.0f;
        b.color = { r, g, b_col, 0.8f };
        
        b.scale = 2.0f + (rand() % 60) / 10.0f; // サイズランダム
    }

    // ★追加: OSのウィンドウサイズを直接300x300に変更する
    if (dx_ && dx_->GetHwnd()) {
        SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, 100, 100, 300, 300, SWP_SHOWWINDOW);
    }
    
    // 確実に存在するテクスチャ（paper.png等）をロードし、失敗した場合はフォールバック
    whiteTex_ = renderer_->LoadTexture2D("Resources/Textures/white1x1.png");
    if (whiteTex_ == 0) {
        whiteTex_ = renderer_->LoadTexture2D("Resources/Textures/paper.png");
    }
}

void MainScene::Update() {
    auto now = std::chrono::steady_clock::now();
    dt_ = std::chrono::duration<float>(now - lastTime_).count();
    lastTime_ = now;
    if (dt_ > 0.1f) dt_ = 1.0f / 60.0f;
    totalTime_ += dt_;

    camera_.Tick(dt_);
    renderer_->SetCamera(camera_);

    // --- ポモドーロ機能の更新 ---
    pomodoroTimer_ += dt_;
    float targetDuration = isPomodoroWork_ ? (25.0f * 60.0f) : (5.0f * 60.0f);
    if (pomodoroTimer_ >= targetDuration) {
        pomodoroTimer_ -= targetDuration;
        isPomodoroWork_ = !isPomodoroWork_;
    }

    // 色の緩やかな遷移 (Lerp)
    float targetTransition = isPomodoroWork_ ? 0.0f : 1.0f;
    pomodoroTransition_ += (targetTransition - pomodoroTransition_) * 1.0f * dt_;

    // 作業中（集中を促す寒色） -> 休憩中（リラックスを促す暖色）
    Engine::Vector4 workColor{ 0.3f, 0.7f, 1.0f, 1.0f }; // シアン
    Engine::Vector4 restColor{ 1.0f, 0.5f, 0.2f, 1.0f }; // オレンジ
    
    currentColor_.x = workColor.x + (restColor.x - workColor.x) * pomodoroTransition_;
    currentColor_.y = workColor.y + (restColor.y - workColor.y) * pomodoroTransition_;
    currentColor_.z = workColor.z + (restColor.z - workColor.z) * pomodoroTransition_;
    currentColor_.w = 1.0f;

    auto* input = Engine::Input::GetInstance();
    float mx = 0.0f, my = 0.0f;
    input->GetMousePos(mx, my);

    // トランジション（チカチカ防止）処理
    if (transitionTimer_ > 0) {
        transitionTimer_--;
        if (transitionTimer_ == 2) {
            // 描画が透明になったタイミングで実際にリサイズを実行
            isMascotMode_ = nextMascotMode_;
            if (isMascotMode_) {
                // マスコットモード：固定サイズ・枠なし
                LONG style = GetWindowLong(dx_->GetHwnd(), GWL_STYLE);
                SetWindowLong(dx_->GetHwnd(), GWL_STYLE, style & ~WS_THICKFRAME);
                SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, lastCursorPos_.x - 150, lastCursorPos_.y - 150, 300, 300, SWP_NOZORDER | SWP_FRAMECHANGED);
            } else {
                // 没入モード：自由にリサイズ可能にするため枠（見えないリサイズハンドル）を付与
                LONG style = GetWindowLong(dx_->GetHwnd(), GWL_STYLE);
                SetWindowLong(dx_->GetHwnd(), GWL_STYLE, style | WS_THICKFRAME);
                SetWindowPos(dx_->GetHwnd(), nullptr, 0, 0, Engine::WindowDX::kW, Engine::WindowDX::kH, SWP_NOZORDER | SWP_FRAMECHANGED);
                
                // 大きくなる時に星屑をランダムに散らす
                swarmCenter_ = { Engine::WindowDX::kW / 2.0f, Engine::WindowDX::kH / 2.0f, 0.0f };
                for (auto& b : boids_) {
                    b.position.x = swarmCenter_.x + ((rand() % 1600) - 800.0f);
                    b.position.y = swarmCenter_.y + ((rand() % 800) - 400.0f);
                }
            }
        }
        return; // トランジション中は以降の更新をスキップ
    }

    if (isMascotMode_) {
        if (input->IsMouseTrigger(0)) {
            // クリック開始時にカーソル位置を記録
            GetCursorPos(&lastCursorPos_);
            startCursorPos_ = lastCursorPos_;
            isDraggingWindow_ = true;
        }

        if (isDraggingWindow_ && input->IsMouseDown(0)) {
            // ドラッグ中のウィンドウ移動処理
            POINT currentCursorPos;
            GetCursorPos(&currentCursorPos);
            int dx = currentCursorPos.x - lastCursorPos_.x;
            int dy = currentCursorPos.y - lastCursorPos_.y;
            
            if (dx != 0 || dy != 0) {
                HWND hwnd = dx_->GetHwnd();
                RECT rect;
                GetWindowRect(hwnd, &rect);
                SetWindowPos(hwnd, nullptr, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                lastCursorPos_ = currentCursorPos;
            }
        }

        if (isDraggingWindow_ && !input->IsMouseDown(0)) {
            // マウスを離したとき（クリック完了判定）
            isDraggingWindow_ = false;
            
            // ドラッグした距離を計算
            int totalDx = lastCursorPos_.x - startCursorPos_.x;
            int totalDy = lastCursorPos_.y - startCursorPos_.y;
            int distSq = totalDx * totalDx + totalDy * totalDy;

            // ほとんど動かしていなければクリックと判定してトランジション開始
            if (distSq < 25) { 
                nextMascotMode_ = false;
                transitionTimer_ = 5; // 5フレームのトランジション
            }
        }
    } else {
        // 没入モード時：右クリックで元の小さなウィンドウに戻る
        if (input->IsMouseTrigger(1)) {
            nextMascotMode_ = true;
            transitionTimer_ = 5;
            GetCursorPos(&lastCursorPos_);
        }
    }

    // Boidsアルゴリズムの更新
    for (auto& b : boids_) {
        Engine::Vector3 vSep{0,0,0}, vAli{0,0,0}, vCoh{0,0,0};
        int neighborCount = 0;

        for (const auto& other : boids_) {
            if (&b == &other) continue;
            float dx = b.position.x - other.position.x;
            float dy = b.position.y - other.position.y;
            float distSq = dx*dx + dy*dy;

            if (distSq < boidPerception_ * boidPerception_) {
                float dist = sqrtf(distSq);
                
                // Alignment（整列）
                vAli.x += other.velocity.x;
                vAli.y += other.velocity.y;
                
                // Cohesion（結合）
                vCoh.x += other.position.x;
                vCoh.y += other.position.y;
                
                // Separation（分離）
                if (dist < boidSeparation_ && dist > 0.001f) {
                    vSep.x += (dx / dist) / dist;
                    vSep.y += (dy / dist) / dist;
                }
                neighborCount++;
            }
        }

        Engine::Vector3 acceleration{0,0,0};

        if (neighborCount > 0) {
            vAli.x /= neighborCount; vAli.y /= neighborCount;
            float lenAli = sqrtf(vAli.x*vAli.x + vAli.y*vAli.y);
            if(lenAli > 0.001f) { vAli.x = (vAli.x/lenAli)*boidSpeed_ - b.velocity.x; vAli.y = (vAli.y/lenAli)*boidSpeed_ - b.velocity.y; }

            vCoh.x = (vCoh.x / neighborCount) - b.position.x;
            vCoh.y = (vCoh.y / neighborCount) - b.position.y;
            float lenCoh = sqrtf(vCoh.x*vCoh.x + vCoh.y*vCoh.y);
            if(lenCoh > 0.001f) { vCoh.x = (vCoh.x/lenCoh)*boidSpeed_ - b.velocity.x; vCoh.y = (vCoh.y/lenCoh)*boidSpeed_ - b.velocity.y; }
            
            float lenSep = sqrtf(vSep.x*vSep.x + vSep.y*vSep.y);
            if(lenSep > 0.001f) { vSep.x = (vSep.x/lenSep)*boidSpeed_ - b.velocity.x; vSep.y = (vSep.y/lenSep)*boidSpeed_ - b.velocity.y; }
            
            acceleration.x += vSep.x * forceSeparation_;
            acceleration.y += vSep.y * forceSeparation_;
            acceleration.x += vAli.x * forceAlignment_;
            acceleration.y += vAli.y * forceAlignment_;
            acceleration.x += vCoh.x * forceCohesion_;
            acceleration.y += vCoh.y * forceCohesion_;
        }

        // 画面外に出ないための引力（フルスクリーン時は画面境界、マスコット時は中心からの半径）
        if (isMascotMode_) {
            float currentRadius = 130.0f;
            Engine::Vector3 vCenter{ swarmCenter_.x - b.position.x, swarmCenter_.y - b.position.y, 0 };
            float distCenterSq = vCenter.x*vCenter.x + vCenter.y*vCenter.y;
            if (distCenterSq > currentRadius * currentRadius) { 
                float distCenter = sqrtf(distCenterSq);
                acceleration.x += (vCenter.x / distCenter) * (distCenter - currentRadius) * 0.2f;
                acceleration.y += (vCenter.y / distCenter) * (distCenter - currentRadius) * 0.2f;
            }
        } else {
            // 没入モード時は画面の境界から反発させる
            const float margin = 100.0f;
            const float screenW = (float)Engine::WindowDX::kW;
            const float screenH = (float)Engine::WindowDX::kH;
            
            if (b.position.x < margin) {
                acceleration.x += (margin - b.position.x) * 0.5f;
            } else if (b.position.x > screenW - margin) {
                acceleration.x -= (b.position.x - (screenW - margin)) * 0.5f;
            }
            
            if (b.position.y < margin) {
                acceleration.y += (margin - b.position.y) * 0.5f;
            } else if (b.position.y > screenH - margin) {
                acceleration.y -= (b.position.y - (screenH - margin)) * 0.5f;
            }
        }

        // Mouse Evasion（マウスからの回避）
        float mdx = b.position.x - mx;
        float mdy = b.position.y - my;
        float mDistSq = mdx*mdx + mdy*mdy;
        if (mDistSq < mouseEvadeRadius_ * mouseEvadeRadius_) {
            float mDist = sqrtf(mDistSq);
            if (mDist > 0.001f) {
                float force = (mouseEvadeRadius_ - mDist) / mouseEvadeRadius_;
                acceleration.x += (mdx / mDist) * force * boidSpeed_ * forceMouseEvade_;
                acceleration.y += (mdy / mDist) * force * boidSpeed_ * forceMouseEvade_;
            }
        }

        // 速度更新と制限
        b.velocity.x += acceleration.x * dt_;
        b.velocity.y += acceleration.y * dt_;

        float speedSq = b.velocity.x*b.velocity.x + b.velocity.y*b.velocity.y;
        float maxSpeed = boidSpeed_ * 2.5f;
        if (speedSq > maxSpeed * maxSpeed) {
            float speed = sqrtf(speedSq);
            b.velocity.x = (b.velocity.x / speed) * maxSpeed;
            b.velocity.y = (b.velocity.y / speed) * maxSpeed;
        }

        b.position.x += b.velocity.x * dt_;
        b.position.y += b.velocity.y * dt_;
        
        // 描画用の角度計算
        b.angle = atan2f(b.velocity.y, b.velocity.x);
    }
} // ★ Update() の閉じカッコを追加

void MainScene::Draw() {
    if (!renderer_) return;
    
    // トランジション中は何も描画しない（透明フレームを出力してチカチカを防ぐ）
    if (transitionTimer_ > 0) {
        renderer_->FlushSprites();
        return; 
    }

    if (isMascotMode_) {
        // --- 小さいウィンドウ時：ポモドーロステータス＆情報の表示 ---
        // 背景の描画 (300x300の半透明暗色背景)
        Engine::Renderer::SpriteDesc bgDesc{};
        bgDesc.w = 300.0f; 
        bgDesc.h = 300.0f;
        bgDesc.x = 0.0f;
        bgDesc.y = 0.0f; 
        bgDesc.color = { 0.1f, 0.12f, 0.15f, 0.85f };
        renderer_->DrawSprite(whiteTex_, bgDesc);

        // カラーアクセント（上部のバーにポモドーロの色を反映）
        Engine::Renderer::SpriteDesc colorBar{};
        colorBar.w = 300.0f; 
        colorBar.h = 8.0f;
        colorBar.x = 0.0f;
        colorBar.y = 0.0f; 
        colorBar.color = currentColor_;
        renderer_->DrawSprite(whiteTex_, colorBar);
        
        // ポモドーロテキストの計算
        float currentTarget = isPomodoroWork_ ? (25.0f * 60.0f) : (5.0f * 60.0f);
        int remainingSec = (int)(currentTarget - pomodoroTimer_);
        int m = remainingSec / 60;
        int s = remainingSec % 60;
        char pomoText[256];
        if (isPomodoroWork_) {
            snprintf(pomoText, sizeof(pomoText), "Focus Time");
        } else {
            snprintf(pomoText, sizeof(pomoText), "Relax Time");
        }
        
        char timeText[256];
        snprintf(timeText, sizeof(timeText), "%02d:%02d", m, s);

        // タイマーなどのテキスト描画
        renderer_->DrawString("Tranquil Space", 20.0f, 20.0f, 0.3f, {0.7f, 0.7f, 0.8f, 1.0f});
        renderer_->DrawString(pomoText, 20.0f, 80.0f, 0.4f, currentColor_);
        renderer_->DrawString(timeText, 20.0f, 120.0f, 1.2f, currentColor_); // 時間を大きく表示
        
        // サポートメッセージ
        if (isPomodoroWork_) {
            renderer_->DrawString("作業に集中しましょう...", 20.0f, 250.0f, 0.25f, {0.6f, 0.6f, 0.7f, 1.0f});
        } else {
            renderer_->DrawString("目を閉じて深呼吸...", 20.0f, 250.0f, 0.25f, {0.6f, 0.6f, 0.7f, 1.0f});
        }

    } else {
        // --- 没入モード時：Boidsとノードグラフの描画 ---
        const float connectDist = 130.0f; // 少し繋がりにくくしていたのを広げる
        const float connectDistSq = connectDist * connectDist;
        const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち"};

        // 線の描画（重すぎないように適度な数に制限）
        for (size_t i = 0; i < boids_.size(); i++) {
            int lineCount = 0;
            for (size_t j = i + 1; j < boids_.size(); j++) {
                if (lineCount > 3) break; // 1つの星から引く線を最大3本にしてスッキリさせる
                
                float dx = boids_[j].position.x - boids_[i].position.x;
                float dy = boids_[j].position.y - boids_[i].position.y;
                float distSq = dx*dx + dy*dy;
                if (distSq < connectDistSq) {
                    float dist = sqrtf(distSq);
                    float alpha = 1.0f - (dist / connectDist); 
                    
                    Engine::Renderer::SpriteDesc lineDesc{};
                    lineDesc.w = dist;
                    lineDesc.h = 2.0f; // スッキリさせるため少し細く
                    lineDesc.x = boids_[i].position.x + dx * 0.5f - dist * 0.5f;
                    lineDesc.y = boids_[i].position.y + dy * 0.5f - lineDesc.h * 0.5f;
                    lineDesc.rotationRad = atan2f(dy, dx);
                    lineDesc.color = {0.8f, 0.9f, 1.0f, alpha * 0.6f}; // 透け感を少し戻す
                    renderer_->DrawSprite(whiteTex_, lineDesc);
                    lineCount++;
                }
            }

            // Boid本体の描画
            Engine::Renderer::SpriteDesc desc{};
            desc.w = boids_[i].scale * 1.5f; 
            desc.h = boids_[i].scale * 1.5f; 
            desc.x = boids_[i].position.x - desc.w / 2.0f;
            desc.y = boids_[i].position.y - desc.h / 2.0f; 
            desc.rotationRad = boids_[i].angle;
            desc.color = { boids_[i].color.x, boids_[i].color.y, boids_[i].color.z, 0.9f };
            renderer_->DrawSprite(whiteTex_, desc);
        }
        
        // テキストは最前面に出るように、最後に描画する
        for (size_t i = 0; i < boids_.size(); i++) {
            if (i % 30 == 0) {
                int wordIdx = (i / 30) % 7;
                // テキストの背景に少し影をつけて見やすくする
                renderer_->DrawString(healingWords[wordIdx], boids_[i].position.x + 11.0f, boids_[i].position.y + 1.0f, 0.25f, {0.1f, 0.1f, 0.2f, 0.8f});
                renderer_->DrawString(healingWords[wordIdx], boids_[i].position.x + 10.0f, boids_[i].position.y, 0.25f, {0.9f, 0.9f, 1.0f, 0.9f});
            }
        }

        // --- 心穏やかな場所 UI（ImGuiを使わず独自のDrawStringで描画） ---
        // スケール1.0が約64px相当であるため、0.4等に縮小して適切なUIサイズにする
        renderer_->DrawString("心穏やかな場所", 50.0f, 50.0f, 0.4f, {0.85f, 0.85f, 0.95f, 1.0f});
        renderer_->DrawString("Tranquil Space", 50.0f, 80.0f, 0.25f, {0.8f, 0.8f, 0.9f, 1.0f});

        float rightX = Engine::WindowDX::kW - 350.0f;
        float bottomY = Engine::WindowDX::kH - 120.0f;
        renderer_->DrawString("AIのささやき：", rightX, bottomY, 0.25f, {0.7f, 0.7f, 0.8f, 1.0f});
        renderer_->DrawString("『ここは、あなたの心を休める場所です。』", rightX, bottomY + 20.0f, 0.25f, {0.7f, 0.7f, 0.8f, 1.0f});
        renderer_->DrawString("※右クリックで小さな姿に戻ります", rightX, bottomY + 50.0f, 0.2f, {0.6f, 0.6f, 0.7f, 1.0f});

        // --- フェーズ1: エコモード＆ポモドーロステータス表示 ---
        float leftX = 50.0f;
        float infoY = Engine::WindowDX::kH - 140.0f; // 少し上へずらす
        
        char loadText[256];
        snprintf(loadText, sizeof(loadText), "System Load: < 0.1%% | FrameTime: %.2fms", dt_ * 1000.0f);
        // 大きく表示 (0.15f -> 0.3f)
        renderer_->DrawString(loadText, leftX, infoY, 0.3f, {0.5f, 0.5f, 0.6f, 0.8f});
        renderer_->DrawString("=== 圧倒的静寂 ===", leftX, infoY + 40.0f, 0.3f, {0.4f, 0.4f, 0.5f, 0.6f});
        
        float currentTarget = isPomodoroWork_ ? (25.0f * 60.0f) : (5.0f * 60.0f);
        int remainingSec = (int)(currentTarget - pomodoroTimer_);
        int m = remainingSec / 60;
        int s = remainingSec % 60;
        char pomoText[256];
        if (isPomodoroWork_) {
            snprintf(pomoText, sizeof(pomoText), "Focus Time - %02d:%02d", m, s);
        } else {
            snprintf(pomoText, sizeof(pomoText), "Relax Time - %02d:%02d", m, s);
        }
        
        // ポモドーロのステータス色で描画（大きく 0.3f -> 0.6f）
        renderer_->DrawString(pomoText, leftX, 120.0f, 0.6f, currentColor_);
    }

    renderer_->FlushSprites();
}

void MainScene::DrawUI() {
#ifdef USE_IMGUI
    // このソフトウェアではImGuiのUIを一切使用しない
#endif
}

entt::entity MainScene::CreateEntity(const std::string& name) {
    auto e = registry_.create();
    registry_.emplace<NameComponent>(e, name);
    registry_.emplace<TransformComponent>(e);
    return e;
}
void MainScene::DestroyObject(uint32_t id) {
    if (registry_.valid(static_cast<entt::entity>(id))) {
        registry_.destroy(static_cast<entt::entity>(id));
    }
}

} // namespace Game