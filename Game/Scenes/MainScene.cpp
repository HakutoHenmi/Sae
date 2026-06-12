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

#include <fstream>
#include "../../Engine/ThirdParty/nlohmann/json.hpp"
using json = nlohmann::json;

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
    
    const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち"};
    for (size_t i = 0; i < boids_.size(); i++) {
        if ((i % 15) == 0 && boids_[i].scale > 4.0f) {
            boids_[i].customText = healingWords[(i / 15) % 7];
        }
    }
    
    // テーマのセットアップ
    themes_ = {
        { {0.12f, 0.14f, 0.18f, 1.0f}, {0.10f, 0.18f, 0.16f, 1.0f}, {0.3f, 0.7f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.2f, 1.0f}, "Deep Space" },
        { {0.02f, 0.05f, 0.15f, 1.0f}, {0.05f, 0.15f, 0.10f, 1.0f}, {0.0f, 0.8f, 0.8f, 1.0f}, {0.2f, 0.9f, 0.5f, 1.0f}, "Deep Sea" },
        { {0.15f, 0.08f, 0.10f, 1.0f}, {0.10f, 0.08f, 0.15f, 1.0f}, {1.0f, 0.5f, 0.3f, 1.0f}, {0.9f, 0.3f, 0.5f, 1.0f}, "Sunset" },
        { {0.05f, 0.05f, 0.05f, 1.0f}, {0.02f, 0.02f, 0.02f, 1.0f}, {0.7f, 0.7f, 0.8f, 1.0f}, {0.5f, 0.5f, 0.6f, 1.0f}, "Midnight" }
    };
    
    // アプリ起動時に保存データを読み込み
    LoadData();

    // ★追加: OSのウィンドウサイズを直接250x320に変更する（新しいマスコットモードサイズ）
    if (dx_ && dx_->GetHwnd()) {
        SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, 100, 100, 250, 320, SWP_SHOWWINDOW);
        // ウィンドウ全体を角丸にする
        HRGN rgn = CreateRoundRectRgn(0, 0, 250, 320, 30, 30);
        SetWindowRgn(dx_->GetHwnd(), rgn, TRUE);
    }
    
    if (renderer_) {
        whiteTex_ = renderer_->LoadTexture2D("Resources/Textures/white1x1.png");
        gearTex_ = renderer_->LoadTexture2D("Resources/Textures/settings_icon_fix.png");
        roundedRectTex_ = renderer_->LoadTexture2D("Resources/Textures/rounded_rect_fix.png");
        softCircleTex_ = renderer_->LoadTexture2D("Resources/Textures/soft_circle.png");
    }
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
    if (!isPomodoroPaused_) {
        pomodoroTimer_ += dt_;
        if (isPomodoroWork_) {
            totalWorkTime_ += dt_;
        }
        float targetDuration = isPomodoroWork_ ? (25.0f * 60.0f) : (5.0f * 60.0f);
        if (pomodoroTimer_ >= targetDuration) {
            pomodoroTimer_ -= targetDuration;
            isPomodoroWork_ = !isPomodoroWork_;
            // セッション終了時（集中完了時）にセーブ
            SaveData();
        }
    }

    // 色の緩やかな遷移 (Lerp)
    float targetTransition = isPomodoroWork_ ? 0.0f : 1.0f;
    pomodoroTransition_ += (targetTransition - pomodoroTransition_) * 1.0f * dt_;

    // 作業中（集中を促す寒色） -> 休憩中（リラックスを促す暖色）
    Engine::Vector4 workColor = themes_[currentThemeIndex_].uiWork;
    Engine::Vector4 restColor = themes_[currentThemeIndex_].uiRelax;
    
    currentColor_.x = workColor.x + (restColor.x - workColor.x) * pomodoroTransition_;
    currentColor_.y = workColor.y + (restColor.y - workColor.y) * pomodoroTransition_;
    currentColor_.z = workColor.z + (restColor.z - workColor.z) * pomodoroTransition_;
    currentColor_.w = 1.0f;

    auto* input = Engine::Input::GetInstance();
    float mx = 0.0f, my = 0.0f;
    input->GetMousePos(mx, my);

    if (isMascotMode_) {
        // マスコットモードではウィンドウが250x320に切り取られるため、
        // エンジン内部で1920x1080にスケーリングされたマウス座標を元の物理ピクセルに戻す
        float realMx = mx * (250.0f / 1920.0f);
        float realMy = my * (320.0f / 1080.0f);

        // ホバー判定 (ウィンドウサイズ 250x320 内にマウスがあるか)
        bool isHovering = (realMx >= 0.0f && realMx <= 250.0f && realMy >= 0.0f && realMy <= 320.0f);
        float targetAlpha = isHovering ? 1.0f : 0.0f;
        hoverAlpha_ += (targetAlpha - hoverAlpha_) * 10.0f * dt_;
    }

    // トランジション（フルスクリーン展開・縮小アニメーション）処理
    if (transitionTimer_ > 0) {
        transitionTimer_--;
        
        float progress = 1.0f - (transitionTimer_ / 30.0f);
        // イージング関数 (Out Cubic)
        float easeOut = 1.0f - powf(1.0f - progress, 3.0f);
        
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(dx_->GetHwnd(), MONITOR_DEFAULTTOPRIMARY), &mi);
        int monitorW = mi.rcMonitor.right - mi.rcMonitor.left;
        int monitorH = mi.rcMonitor.bottom - mi.rcMonitor.top;
        
        int startX = lastCursorPos_.x;
        int startY = lastCursorPos_.y;
        int startW = 250;
        int startH = 320;
        
        int currentX, currentY, currentW, currentH;
        
        if (!nextMascotMode_) {
            // Mascot -> Immersive : 250x320 から FullScreen へ展開
            currentX = static_cast<int>(startX + (mi.rcMonitor.left - startX) * easeOut);
            currentY = static_cast<int>(startY + (mi.rcMonitor.top - startY) * easeOut);
            currentW = static_cast<int>(startW + (monitorW - startW) * easeOut);
            currentH = static_cast<int>(startH + (monitorH - startH) * easeOut);
        } else {
            // Immersive -> Mascot : FullScreen から 250x320 へ縮小
            currentX = static_cast<int>(mi.rcMonitor.left + (startX - mi.rcMonitor.left) * easeOut);
            currentY = static_cast<int>(mi.rcMonitor.top + (startY - mi.rcMonitor.top) * easeOut);
            currentW = static_cast<int>(monitorW + (startW - monitorW) * easeOut);
            currentH = static_cast<int>(monitorH + (startH - monitorH) * easeOut);
        }
        
        // アニメーション中は常に最前面表示にしておく
        SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, 
                     currentX, currentY, currentW, currentH, SWP_SHOWWINDOW);

        if (transitionTimer_ == 0) {
            if (nextMascotMode_) {
                isMascotMode_ = true;
                LONG style = GetWindowLong(dx_->GetHwnd(), GWL_STYLE);
                SetWindowLong(dx_->GetHwnd(), GWL_STYLE, style & ~WS_THICKFRAME);
                HRGN rgn = CreateRoundRectRgn(0, 0, 250, 320, 30, 30);
                SetWindowRgn(dx_->GetHwnd(), rgn, TRUE);
            } else {
                // フルスクリーン展開完了時は最前面を解除する
                SetWindowPos(dx_->GetHwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            }
        }
    } else {
        if (isMascotMode_) {
            if (input->IsMouseTrigger(0)) {
                // マスコットモード時は座標を補正
                float checkMx = mx * (250.0f / 1920.0f);
                float checkMy = my * (320.0f / 1080.0f);

                // クリックされた位置を確認してボタン判定
                bool clickedButton = false;
                
                // ボタンのY座標: 260〜310
                if (checkMy >= 260.0f && checkMy <= 310.0f) {
                    if (checkMx >= 55.0f && checkMx <= 95.0f) { // Reset
                        pomodoroTimer_ = 0.0f;
                        clickedButton = true;
                    } else if (checkMx >= 100.0f && checkMx <= 150.0f) { // Play/Pause
                        isPomodoroPaused_ = !isPomodoroPaused_;
                        clickedButton = true;
                    } else if (checkMx >= 155.0f && checkMx <= 195.0f) { // Stop/Next
                        pomodoroTimer_ = 0.0f;
                        isPomodoroWork_ = !isPomodoroWork_; // モード切り替え
                        clickedButton = true;
                    }
                }

                // 閉じるボタン判定 (右上: X:210〜250, Y:0〜40)
                if (!clickedButton && checkMx >= 210.0f && checkMx <= 250.0f && checkMy >= 0.0f && checkMy <= 40.0f) {
                    PostQuitMessage(0); // アプリケーション終了
                    clickedButton = true;
                }

                if (!clickedButton) {
                    // ウィンドウドラッグ開始
                    GetCursorPos(&lastCursorPos_);
                    startCursorPos_ = lastCursorPos_;
                    isDraggingWindow_ = true;
                }
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
                    transitionTimer_ = 30; // 30フレームのアニメーション
                    
                    // アニメーション開始直後：UIを消して没入モードの色にするため isMascotMode_ をすぐに false に
                    isMascotMode_ = false; 
                    LONG style = GetWindowLong(dx_->GetHwnd(), GWL_STYLE);
                    SetWindowLong(dx_->GetHwnd(), GWL_STYLE, style | WS_THICKFRAME);
                    SetWindowRgn(dx_->GetHwnd(), NULL, TRUE); // 角丸を解除して四角にする
                    
                    swarmCenter_ = { Engine::WindowDX::kW / 2.0f, Engine::WindowDX::kH / 2.0f, 0.0f };
                    for (auto& b : boids_) {
                        b.position.x = swarmCenter_.x + ((rand() % 1600) - 800.0f);
                        b.position.y = swarmCenter_.y + ((rand() % 800) - 400.0f);
                    }
                }
            }
        } else {
            // 右クリックでマスコットモードへ戻る
            if (input->IsMouseTrigger(1)) { // 1 = Right Click
                nextMascotMode_ = true;
                transitionTimer_ = 30; // 30フレームでフェードアウト
                
                // 没入モード(フルスクリーン)からウィンドウモードへ戻すための準備
                LONG style = GetWindowLong(dx_->GetHwnd(), GWL_STYLE);
                SetWindowLong(dx_->GetHwnd(), GWL_STYLE, style | WS_THICKFRAME);
                SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            }
            
            // --- 設定メニューとカラーパレットのマウス判定 ---
            float btnW = 50.0f; float btnH = 50.0f;
            float btnX = Engine::WindowDX::kW - btnW - 30.0f;
            float btnY = 30.0f;

            float panelW = 500.0f; float panelH = 340.0f;
            float panelX = Engine::WindowDX::kW / 2.0f - panelW / 2.0f;
            float panelY = Engine::WindowDX::kH / 2.0f - panelH / 2.0f;

            bool handledMouse = false;
            if (input->IsMouseTrigger(0)) {
                if (isSettingsOpen_) {
                    // カラーパレットのクリック判定
                    for (int i = 0; i < (int)themes_.size(); ++i) {
                        float sx = panelX + 50.0f + i * 110.0f;
                        float sy = panelY + 80.0f;
                        if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                            currentThemeIndex_ = i;
                            SaveData();
                            handledMouse = true;
                            break;
                        }
                    }
                    
                    // インタラクションモードのクリック判定
                    if (!handledMouse) {
                        for (int i = 0; i < 4; ++i) {
                            float sx = panelX + 50.0f + i * 110.0f;
                            float sy = panelY + 200.0f; // カラーパレットの下に配置
                            if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                                interactionMode_ = i;
                                SaveData();
                                handledMouse = true;
                                break;
                            }
                        }
                    }
                    
                    // パネル外クリックで閉じる
                    if (!handledMouse && (mx < panelX || mx > panelX + panelW || my < panelY || my > panelY + panelH)) {
                        isSettingsOpen_ = false;
                        handledMouse = true;
                    }
                }

                // 設定ボタンのクリック判定
                if (!handledMouse && mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
                    isSettingsOpen_ = !isSettingsOpen_;
                    inputBuffer_.clear();
                    handledMouse = true;
                }

                // インタラクションモード：Rippleのトリガー（UI以外をクリックした場合）
                if (!handledMouse && interactionMode_ == 2 && !isSettingsOpen_) {
                    rippleActive_ = true;
                    rippleRadius_ = 0.0f;
                    ripplePos_ = { mx, my, 0.0f };
                }

                // インタラクションモード：Popcornのトリガー（UI以外をクリックした場合）
                if (!handledMouse && interactionMode_ == 3 && !isSettingsOpen_) {
                    for (auto& b : boids_) {
                        if (b.customText.empty()) continue;
                        
                        // だいたいの位置（ノードの中心から半径100px以内ならヒット）でストレスフリーに
                        float dx = b.position.x - mx;
                        float dy = b.position.y - my;
                        if (dx*dx + dy*dy < 100.0f * 100.0f) {
                            float textScale = (b.scale > 6.0f) ? 0.5f : 0.4f;
                            float ty = b.position.y + ((b.scale > 6.0f) ? -35.0f : -25.0f);
                            
                            // テキストのすべての文字を一気に弾け飛ばす（長押し不要、一撃で粉砕）
                            std::vector<size_t> boundaries;
                            for (size_t i = 0; i < b.customText.length(); ) {
                                boundaries.push_back(i);
                                unsigned char c = b.customText[i];
                                if ((c & 0x80) == 0) i += 1;
                                else if ((c & 0xE0) == 0xC0) i += 2;
                                else if ((c & 0xF0) == 0xE0) i += 3;
                                else if ((c & 0xF8) == 0xF0) i += 4;
                                else i += 1;
                            }
                            boundaries.push_back(b.customText.length());
                            
                            // 文字列を順番にFallingChar化して派手に散らす
                            for (size_t i = 0; i < boundaries.size() - 1; ++i) {
                                size_t start = boundaries[i];
                                size_t len = boundaries[i+1] - start;
                                
                                FallingChar fc;
                                fc.character = b.customText.substr(start, len);
                                fc.position = { b.position.x + ((rand()%60)-30.0f), ty + ((rand()%40)-20.0f) };
                                fc.velocity = { ((rand()%100)-50) * 4.0f, -400.0f - (rand()%400) }; // 派手に上へ弾ける
                                fc.angle = 0.0f;
                                fc.angularVelocity = ((rand()%100)-50) * 0.1f;
                                fc.color = (b.scale > 6.0f) ? Engine::Vector4{1.0f, 0.95f, 0.8f, 1.0f} : Engine::Vector4{0.9f, 0.9f, 1.0f, 0.8f};
                                fc.scale = textScale;
                                fc.life = 0.0f;
                                fallingChars_.push_back(fc);
                            }
                            // 元のテキストは全消去
                            b.customText.clear();
                        }
                    }
                }
            } // end if (input->IsMouseTrigger(0))

            if (!isSettingsOpen_) {
                // --- 独自キーボード入力処理 (A-Z等) ---
                struct KeyMap { BYTE dik; char c; char s; };
                static const KeyMap keys[] = {
                {DIK_A,'a','A'},{DIK_B,'b','B'},{DIK_C,'c','C'},{DIK_D,'d','D'},{DIK_E,'e','E'},{DIK_F,'f','F'},{DIK_G,'g','G'},
                {DIK_H,'h','H'},{DIK_I,'i','I'},{DIK_J,'j','J'},{DIK_K,'k','K'},{DIK_L,'l','L'},{DIK_M,'m','M'},{DIK_N,'n','N'},
                {DIK_O,'o','O'},{DIK_P,'p','P'},{DIK_Q,'q','Q'},{DIK_R,'r','R'},{DIK_S,'s','S'},{DIK_T,'t','T'},{DIK_U,'u','U'},
                {DIK_V,'v','V'},{DIK_W,'w','W'},{DIK_X,'x','X'},{DIK_Y,'y','Y'},{DIK_Z,'z','Z'},{DIK_SPACE,' ',' '},
                {DIK_MINUS,'-','_'},{DIK_EQUALS,'=','+'},{DIK_LBRACKET,'[','{'},{DIK_RBRACKET,']','}'},{DIK_SEMICOLON,';',':'},
                {DIK_APOSTROPHE,'\'','\"'},{DIK_COMMA,',','<'},{DIK_PERIOD,'.','>'},{DIK_SLASH,'/','?'}
            };
            bool shift = input->Down(DIK_LSHIFT) || input->Down(DIK_RSHIFT);
            for (const auto& k : keys) {
                if (input->Trigger(k.dik)) {
                    if (inputBuffer_.length() < 30) {
                        inputBuffer_ += shift ? k.s : k.c;
                    }
                }
            }
            if (input->Trigger(DIK_BACK) && !inputBuffer_.empty()) {
                inputBuffer_.pop_back();
            }
            if (input->Trigger(DIK_RETURN) && !inputBuffer_.empty()) {
                if (!boids_.empty()) {
                    int randIdx = rand() % boids_.size();
                    boids_[randIdx].customText = inputBuffer_;
                    boids_[randIdx].scale = 6.5f; 
                    boids_[randIdx].color = {1.0f, 0.8f, 0.6f, 1.0f}; 
                }
                
                // 入力した気持ちをログに保存（最大50件）
                savedFeelings_.push_back(inputBuffer_);
                if (savedFeelings_.size() > 50) {
                    savedFeelings_.erase(savedFeelings_.begin());
                }
                SaveData();

                inputBuffer_.clear();
            }
        } // end if (!isSettingsOpen_)
    } // end if (!isMascotMode_)
    } // end else (transitionTimer_ == 0)

    // Ripple（波紋）モードの更新
    if (interactionMode_ == 2 && rippleActive_) {
        rippleRadius_ += 800.0f * dt_; // 波紋が広がる速度
        if (rippleRadius_ > 2500.0f) {
            rippleActive_ = false; // 画面外まで十分広がったら終了
        }
    }

    // --- 癒やしの言葉のリスポーン機能（アイデア1） ---
    static int wordRespawnTimer = 0;
    wordRespawnTimer++;
    if (wordRespawnTimer > 60) { // 1秒に1回チェック
        wordRespawnTimer = 0;
        
        int textCount = 0;
        std::vector<Boid*> emptyBoids;
        for (auto& b : boids_) {
            if (!b.customText.empty()) textCount++;
            else emptyBoids.push_back(&b);
        }
        
        // 全体の言葉の数が少なめ（15個以下）なら積極的に補充
        if (textCount < 15 && !emptyBoids.empty()) {
            int idx = rand() % emptyBoids.size();
            const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち", "深呼吸", "星屑", "光"};
            emptyBoids[idx]->customText = healingWords[rand() % 10];
            emptyBoids[idx]->scale = 4.5f; // 少し大きめに設定して目立たせる
            emptyBoids[idx]->textAlpha = 0.0f; // フェードイン開始
        }
    }

    // Boidsアルゴリズムの更新
    for (auto& b : boids_) {
        // フェードイン処理
        if (!b.customText.empty() && b.textAlpha < 1.0f) {
            b.textAlpha += 0.5f * dt_; // 2秒かけてフワッと現れる
            if (b.textAlpha > 1.0f) b.textAlpha = 1.0f;
        }

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

        // --- インタラクションモードごとのマウス干渉処理 ---
        float mdx = b.position.x - mx;
        float mdy = b.position.y - my;
        float mDistSq = mdx*mdx + mdy*mdy;
        float mDist = sqrtf(mDistSq);

        // UI操作中（設定画面が開いている等）は一部のインタラクションを無効化する
        bool isInteracting = !isMascotMode_ && !isSettingsOpen_;

        if (interactionMode_ == 0) {
            // モード0: Repel（回避・静寂モード）
            if (isInteracting && mDistSq < mouseEvadeRadius_ * mouseEvadeRadius_ && mDist > 0.001f) {
                float force = (mouseEvadeRadius_ - mDist) / mouseEvadeRadius_;
                acceleration.x += (mdx / mDist) * force * boidSpeed_ * forceMouseEvade_;
                acceleration.y += (mdy / mDist) * force * boidSpeed_ * forceMouseEvade_;
            }
        } 
        else if (interactionMode_ == 1) {
            // モード1: Gravity（引力・吸収モード）
            if (isInteracting && input->IsMouseDown(0) && mDist > 10.0f) {
                // 左クリック中は非常に強い引力
                float gravityRadius = 1500.0f; // 画面全体に届く
                if (mDist < gravityRadius) {
                    float force = (gravityRadius - mDist) / gravityRadius;
                    // 吸い込み力を大幅にアップ
                    float suckPower = 1200.0f; 
                    acceleration.x -= (mdx / mDist) * force * suckPower;
                    acceleration.y -= (mdy / mDist) * force * suckPower;
                }
            }
        }
        else if (interactionMode_ == 2) {
            // モード2: Ripple（波紋・衝撃波モード）
            if (rippleActive_) {
                float rdx = b.position.x - ripplePos_.x;
                float rdy = b.position.y - ripplePos_.y;
                float rDistSq = rdx*rdx + rdy*rdy;
                float rDist = sqrtf(rDistSq);
                
                // 星と波紋のリング（rippleRadius_）との距離を計算
                float distToRing = fabsf(rDist - rippleRadius_);
                float ringThickness = 120.0f; // 波紋の厚み
                
                if (rDist > 0.001f && distToRing < ringThickness) {
                    // 波紋に触れた星に外向きの衝撃を与える
                    float force = (ringThickness - distToRing) / ringThickness;
                    // 波紋が遠くに広がるにつれて威力を減衰させ、画面外へ吹き飛ぶのを防ぐ工夫
                    float falloff = 1.0f - (rippleRadius_ / 2500.0f);
                    if (falloff < 0.0f) falloff = 0.0f;
                    float blastPower = 2000.0f * falloff; 
                    
                    acceleration.x += (rdx / rDist) * force * blastPower;
                    acceleration.y += (rdy / rDist) * force * blastPower;
                }
            }
            
            // Rippleモード全体に対する工夫：外周に張り付くのを防ぐため、画面中央へ向かう微弱な引力を常にかける
            float cx = Engine::WindowDX::kW / 2.0f;
            float cy = Engine::WindowDX::kH / 2.0f;
            float dx = cx - b.position.x;
            float dy = cy - b.position.y;
            acceleration.x += dx * 0.15f;
            acceleration.y += dy * 0.15f;
        }
        else if (interactionMode_ == 3) {
            // モード3: Popcorn（ポップコーン・パチンコモード）
            // Boids自体のマウス回避（Repel）はオフ
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

    // --- パチンコ・テキストの物理更新（ステップ3・4） ---
    const float nodeRadius = 8.0f;               // 星（ノード）の大雑把な当たり判定半径
    const float charRadius = 15.0f;              // 文字の大雑把な当たり判定半径
    
    for (auto it = fallingChars_.begin(); it != fallingChars_.end(); ) {
        it->velocity.y += 980.0f * dt_; // 重力
        it->angle += it->angularVelocity * dt_;
        
        // 空気抵抗
        it->velocity.x *= 0.99f;
        it->velocity.y *= 0.99f;
        
        it->position.x += it->velocity.x * dt_;
        it->position.y += it->velocity.y * dt_;
        it->life += dt_;
        
        // ----------------------------------------------------
        // 【ステップ3・4】ノード（釘）およびライン（線分）との当たり判定
        // ----------------------------------------------------
        // 高速化のため、まずは落下文字の近くにいるノードだけを抽出
        std::vector<Boid*> nearBoids;
        for (auto& b : boids_) {
            float dx = it->position.x - b.position.x;
            float dy = it->position.y - b.position.y;
            if (dx*dx + dy*dy < 200.0f * 200.0f) { // 文字の周囲200px以内のノードを対象
                nearBoids.push_back(&b);
                
                // 【ステップ3】ノード自体（釘）との衝突判定
                float dist = sqrtf(dx*dx + dy*dy);
                float combinedRadius = nodeRadius + charRadius;
                if (dist < combinedRadius && dist > 0.001f) {
                    float nx = dx / dist; // ノードから文字へ向かう法線ベクトル
                    float ny = dy / dist;
                    
                    // 位置の押し出し（めり込み防止）
                    it->position.x = b.position.x + nx * combinedRadius;
                    it->position.y = b.position.y + ny * combinedRadius;
                    
                    // 速度の反射計算
                    float dot = it->velocity.x * nx + it->velocity.y * ny;
                    if (dot < 0) { // 文字がノードに向かっている時のみ反射
                        float bounce = 0.6f; // パチンコの反発係数
                        it->velocity.x -= (1.0f + bounce) * dot * nx;
                        it->velocity.y -= (1.0f + bounce) * dot * ny;
                        it->angularVelocity = ((rand()%100)-50) * 0.2f; // 当たると回転がランダムに変化
                    }
                    
                    // 【相互作用】ぶつかられたノード側のリアクション（揺れと発光）
                    // 力を弱めにして、ノードが画面外に吹き飛んでしまわないように調整
                    b.velocity.x -= nx * 20.0f;
                    b.velocity.y -= ny * 20.0f;
                    b.scale = 6.0f; // ぶつかると一瞬大きくなる
                }
            }
        }
        
        // 画面の一番下に落ちたら、新しい星として生まれ変わる（アイデア2）
        if (it->position.y > Engine::WindowDX::kH + 20.0f) {
            Boid newStar;
            newStar.position = { it->position.x, Engine::WindowDX::kH + 20.0f, 0.0f };
            // 上に向かってフワッと昇る初速（群れに合流する）
            newStar.velocity = { ((rand()%100)-50) * 1.5f, -250.0f - (rand()%200) };
            newStar.color = it->color; // 元の文字の色を引き継ぐ
            newStar.scale = 6.0f;      // 生まれたては少し大きめ
            newStar.angle = 0.0f;
            newStar.textAlpha = 1.0f;
            
            // 星が増えすぎないように新陳代謝（上限400）
            if (boids_.size() < 400) {
                boids_.push_back(newStar);
            } else {
                int replaceIdx = rand() % boids_.size();
                boids_[replaceIdx] = newStar;
            }
            
            it = fallingChars_.erase(it);
        } else if (it->life > 15.0f) {
            it = fallingChars_.erase(it); // 寿命の場合はただ消える
        } else {
            ++it;
        }
    }

    // 次のフレームの風力計算用にマウス座標を保存
    prevMx_ = mx;
    prevMy_ = my;
} // ★ Update() の閉じカッコを追加

void MainScene::Draw() {
    if (!renderer_) return;
    
    // フェードアルファの計算
    float immersiveAlpha = 1.0f;
    float mascotAlpha = 1.0f;
    
    if (transitionTimer_ > 0) {
        float p = transitionTimer_ / 30.0f; // 1.0(開始) -> 0.0(終了)
        if (!nextMascotMode_) {
            // Mascot -> Immersive (拡大中): 
            // 拡大中はBoids(ノード)がじわじわ現れ、MascotUIがスッと消える
            immersiveAlpha = 1.0f - p;
            mascotAlpha = p; // (※MascotUIは即座に消したい場合はこの後のdrawMascot判定で弾く)
        } else {
            // Immersive -> Mascot (縮小中):
            // 縮小中はBoids(ノード)がじわじわ消え、MascotUIが現れる
            immersiveAlpha = p;
            mascotAlpha = 1.0f - p;
        }
    } else {
        if (isMascotMode_) immersiveAlpha = 0.0f;
        else mascotAlpha = 0.0f;
    }

    // 文字UI（Tranquil Space等のテキスト）はフルスクリーン時のみ表示、かつトランジション中は非表示
    bool drawUI = (transitionTimer_ == 0 && !isMascotMode_);
    bool drawImmersive = (immersiveAlpha > 0.01f);
    
    // ==========================================
    // 0. 共通背景の描画
    // ==========================================
    Engine::Vector4 bgWork = themes_[currentThemeIndex_].bgWork;
    Engine::Vector4 bgRelax = themes_[currentThemeIndex_].bgRelax;
    Engine::Vector4 bgColor = {
        bgWork.x * (1.0f - pomodoroTransition_) + bgRelax.x * pomodoroTransition_,
        bgWork.y * (1.0f - pomodoroTransition_) + bgRelax.y * pomodoroTransition_,
        bgWork.z * (1.0f - pomodoroTransition_) + bgRelax.z * pomodoroTransition_,
        1.0f
    };
    Engine::Renderer::SpriteDesc bgDesc{};
    // DWMでどのようにスケールされても画面を覆い尽くす十分なサイズ
    bgDesc.w = 5000.0f;
    bgDesc.h = 5000.0f;
    bgDesc.x = -1000.0f;
    bgDesc.y = -1000.0f;
    bgDesc.color = bgColor;
    renderer_->DrawSprite(whiteTex_, bgDesc);

    // ==========================================
    // 1. 没入モード (Immersive) の描画
    // ==========================================
    if (drawImmersive) {
        const float connectDist = 130.0f;
        const float connectDistSq = connectDist * connectDist;
        const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち"};

        for (size_t i = 0; i < boids_.size(); i++) {
            int lineCount = 0;
            for (size_t j = i + 1; j < boids_.size(); j++) {
                if (lineCount > 3) break;
                
                float dx = boids_[i].position.x - boids_[j].position.x;
                float dy = boids_[i].position.y - boids_[j].position.y;
                float distSq = dx*dx + dy*dy;
                if (distSq < connectDistSq) {
                    float dist = sqrtf(distSq);
                    float alpha = 1.0f - (dist / connectDist);
                    
                    Engine::Renderer::SpriteDesc line{};
                    line.x = (boids_[i].position.x + boids_[j].position.x) / 2.0f;
                    line.y = (boids_[i].position.y + boids_[j].position.y) / 2.0f;
                    line.w = dist;
                    line.h = 1.0f;
                    line.rotationRad = atan2f(dy, dx);
                    line.x -= line.w / 2.0f;
                    line.y -= line.h / 2.0f;
                    line.color = {0.8f, 0.9f, 1.0f, alpha * 0.5f * immersiveAlpha};
                    renderer_->DrawSprite(whiteTex_, line);
                    lineCount++;
                }
            }

            Engine::Renderer::SpriteDesc bDesc{};
            bDesc.w = boids_[i].scale;
            bDesc.h = boids_[i].scale;
            bDesc.x = boids_[i].position.x - bDesc.w/2.0f;
            bDesc.y = boids_[i].position.y - bDesc.h/2.0f;
            bDesc.rotationRad = boids_[i].angle;
            bDesc.color = boids_[i].color;
            renderer_->DrawSprite(whiteTex_, bDesc);

            if (drawUI) {
                if (!boids_[i].customText.empty()) {
                    float alpha = boids_[i].textAlpha;
                    if (boids_[i].scale > 6.0f) {
                        // ユーザーが入力した気持ちを常時表示（目立たせるため少し大きく、色も明るく）
                        const char* word = boids_[i].customText.c_str();
                        float wordW = renderer_->MeasureTextWidth(word, 0.5f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                        renderer_->DrawString(word, boids_[i].position.x - wordW / 2.0f, boids_[i].position.y - 35.0f, 0.5f, {1.0f, 0.95f, 0.8f, alpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    } else {
                        // デフォルトの癒やしの言葉
                        const char* word = boids_[i].customText.c_str();
                        float wordW = renderer_->MeasureTextWidth(word, 0.4f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                        renderer_->DrawString(word, boids_[i].position.x - wordW / 2.0f, boids_[i].position.y - 25.0f, 0.4f, {0.9f, 0.9f, 1.0f, 0.6f * alpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    }
                }
            }
        }
        
        // --- 落下するパチンコ文字の描画 ---
        if (drawUI) {
            for (const auto& fc : fallingChars_) {
                renderer_->DrawString(fc.character.c_str(), fc.position.x, fc.position.y, fc.scale, fc.color, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
        }
    }

    // ==========================================
    // UI の共通マウス座標取得
    // ==========================================
    auto* input = Engine::Input::GetInstance();
    float mx = 0.0f, my = 0.0f;
    if (input) input->GetMousePos(mx, my);
    
    float mascotMx = mx * (250.0f / 1920.0f);
    float mascotMy = my * (320.0f / 1080.0f);

    // ==========================================
    // 2. マスコットモード (Mascot UI) の描画
    // ==========================================
    if (mascotAlpha > 0.01f) {
        // ※背景は既に共通で描画済みのため削除

        // 左上: Tranquil Space
        renderer_->DrawString("Tranquil Space", 15.0f, 10.0f, 0.4f, {0.9f, 0.85f, 0.8f, mascotAlpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        
        // 右上: フルスクリーンに戻る/閉じるボタン (×マーク) - 背景のみガウスグロウ
        auto drawCross = [&](float cx, float cy, float size, Engine::Vector4 color) {
            for(int i = 0; i < 2; ++i) {
                // 背景のガウスグロウ（サイズを広げて外側に淡い光を漏らす）
                Engine::Renderer::SpriteDesc g{};
                g.w = size + 24.0f; g.h = 24.0f;
                g.x = cx - g.w/2.0f; g.y = cy - g.h/2.0f;
                g.rotationRad = (i == 0) ? 0.785398f : -0.785398f; 
                g.color = color;
                g.color.w *= 0.6f; // 外側に広がるため少し抑えてふんわりさせる
                g.layer = 90;
                g.shaderName = "GaussianUI";
                renderer_->DrawSprite(whiteTex_, g);
                
                // シャープな元の形状
                Engine::Renderer::SpriteDesc sd{};
                sd.w = size; sd.h = 2.0f;
                sd.x = cx - sd.w/2.0f; sd.y = cy - sd.h/2.0f;
                sd.rotationRad = (i == 0) ? 0.785398f : -0.785398f;
                sd.color = color;
                sd.layer = 91;
                renderer_->DrawSprite(whiteTex_, sd);
            }
        };
        bool crossHover = (abs(mascotMx - 225.0f) < 15.0f && abs(mascotMy - 20.0f) < 15.0f);
        float crossSize = crossHover ? 18.0f : 16.0f;
        Engine::Vector4 crossColor = crossHover ? Engine::Vector4{1.0f, 1.0f, 1.0f, mascotAlpha} : Engine::Vector4{0.6f, 0.6f, 0.6f, mascotAlpha};
        drawCross(225.0f, 20.0f, crossSize, crossColor);

        // 中央のプログレスリング（円弧）
        float cx = 125.0f;
        // さらにゲージを下げる(132.0f -> 138.0f)
        float cy = 138.0f;
        float radius = 80.0f;
            
            float currentTarget = isPomodoroWork_ ? (25.0f * 60.0f) : (5.0f * 60.0f);
            float progress = 1.0f - (pomodoroTimer_ / currentTarget); // 時間経過で減っていく
        
        // ガウスブラーシェーダーによる完璧なプロシージャルぼかし円
        auto drawGaussianCircle = [&](float cx_c, float cy_c, float maxR, Engine::Vector4 baseColor) {
            Engine::Renderer::SpriteDesc sd{};
            sd.w = maxR * 2.0f;
            sd.h = maxR * 2.0f;
            sd.x = cx_c - maxR;
            sd.y = cy_c - maxR;
            sd.color = baseColor;
            sd.shaderName = "GaussianUI";
            renderer_->DrawSprite(whiteTex_, sd);
        };

        // スキャンライン法による完璧な塗りつぶし円（先端の丸みやグロウに利用）
        auto drawSolidCircle = [&](float cx_c, float cy_c, float r, Engine::Vector4 color) {
            float r2 = r * r;
            for (float dy = -r; dy <= r; dy += 1.0f) {
                float dx = sqrtf(std::max(0.0f, r2 - dy * dy));
                Engine::Renderer::SpriteDesc sd{};
                sd.w = dx * 2.0f;
                sd.h = 1.5f; // 隙間を防ぐため少し太く
                sd.x = cx_c - dx;
                sd.y = cy_c + dy - sd.h / 2.0f;
                sd.color = color;
                renderer_->DrawSprite(whiteTex_, sd);
            }
        };

        // 滑らかな円を描画するためのラムダ関数（超高分割による美しいブルーム基礎）
        auto drawRing = [&](float r, float thickness, float startAngle, float endAngle, Engine::Vector4 color) {
            int segments = 720; // 720分割で限界まで滑らかにし、放射状の隙間や重なりを最小化
            float dAngle = (endAngle - startAngle) / segments;
            for (int i = 0; i < segments; ++i) {
                float a1 = startAngle + i * dAngle;
                float a2 = startAngle + (i + 1) * dAngle;
                float x1 = cx + cosf(a1) * r;
                float y1 = cy + sinf(a1) * r;
                float x2 = cx + cosf(a2) * r;
                float y2 = cy + sinf(a2) * r;
                
                float dx = x2 - x1;
                float dy = y2 - y1;
                float len = sqrtf(dx * dx + dy * dy);
                
                Engine::Renderer::SpriteDesc line{};
                line.w = len + 1.0f; // 隙間を防ぐ
                line.h = thickness;
                line.x = (x1 + x2) / 2.0f - line.w / 2.0f;
                line.y = (y1 + y2) / 2.0f - line.h / 2.0f;
                line.rotationRad = atan2f(dy, dx);
                line.color = color;
                renderer_->DrawSprite(whiteTex_, line);
            }
            
            // 先端の丸み（スキャンライン円を利用して完璧な半円を描画）
            if (thickness >= 4.0f) {
                drawSolidCircle(cx + cosf(startAngle) * r, cy + sinf(startAngle) * r, thickness / 2.0f, color);
                drawSolidCircle(cx + cosf(endAngle) * r, cy + sinf(endAngle) * r, thickness / 2.0f, color);
            }
        };

        // 下部のくぼみ（ギャップ）の設定
        float gap = 1.1f; // ギャップの大きさ（ラジアン）
        float bottomAngle = 3.141592f / 2.0f; // 真下 (PI/2)
        // 左下から開始し、時計回りに右下まで（右から消えていく動きになる）
        float startBg = bottomAngle + gap / 2.0f;
        float endBg = bottomAngle - gap / 2.0f + 6.283185f;
        float totalRange = endBg - startBg;

        // 背景リング（上品な細さと透明度に調整）
        drawRing(radius, 6.0f, startBg, endBg, {0.8f, 0.9f, 0.9f, 0.08f * mascotAlpha});
        
        // アクティブなリング
        if (progress > 0.001f) {
            float activeStart = endBg - totalRange * progress;
            Engine::Vector4 ringCol = currentColor_;
            ringCol.w *= mascotAlpha;
            
            // リングの完璧なガウスブラー・グロウ効果
            // 専用のシェーダーを用いて計算上の完璧なグラデーション円を連続配置し、すりガラスのような滑らかな発光を実現します
            int numDots = 360; // 1度ごとに1つのガウス円を配置
            float dAngle = (endBg - activeStart) / numDots;
            for (int i = 0; i <= numDots; ++i) {
                float a = activeStart + i * dAngle;
                float px = cx + cosf(a) * radius;
                float py = cy + sinf(a) * radius;
                
                Engine::Vector4 dotCol = ringCol;
                dotCol.w *= 0.15f; // 重なりによる白飛びを防ぐ適度なアルファ
                drawGaussianCircle(px, py, 21.0f, dotCol); // 21pxの半径で柔らかく発光
            }

            // リング本体（上品な太さに調整）
            drawRing(radius, 6.0f, activeStart, endBg, ringCol);
        }

        // 時間テキスト (中央)
        int remainingSec = (int)(currentTarget - pomodoroTimer_);
        int m = remainingSec / 60;
        int s = remainingSec % 60;
        char timeText[256];
        snprintf(timeText, sizeof(timeText), "%02d:%02d", m, s);
        
        // 1.05f だと円からはみ出すため 0.85f に縮小
        float timeW = renderer_->MeasureTextWidth(timeText, 0.85f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        Engine::Vector4 timeCol = currentColor_;
        timeCol.w *= mascotAlpha;
        // Y軸のオフセットを大きくして、数字を上へ移動 (-15.0f -> -22.0f)
        renderer_->DrawString(timeText, cx - timeW / 2.0f, cy - 22.0f, 0.85f, timeCol, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        
        // ステータス (Focus Time / Relax Time)
        const char* pomoText = isPomodoroWork_ ? "Focus Time" : "Relax Time";
        float pomoW = renderer_->MeasureTextWidth(pomoText, 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        // ゲージ下降に伴い、間隔を少しだけ詰める
        renderer_->DrawString(pomoText, cx - pomoW / 2.0f, cy + radius + 6.0f, 0.35f, {0.9f, 0.9f, 0.9f, mascotAlpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

        // 今日の作業時間 (Total)
        int tS = (int)totalWorkTime_ % 60;
        int tM = ((int)totalWorkTime_ / 60) % 60;
        int tH = (int)totalWorkTime_ / 3600;
        char totalText[256];
        snprintf(totalText, sizeof(totalText), "Total  %02d:%02d:%02d", tH, tM, tS);
        
        float totalW = renderer_->MeasureTextWidth(totalText, 0.32f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        // Focus Time から適度な隙間
        renderer_->DrawString(totalText, cx - totalW / 2.0f, cy + radius + 24.0f, 0.32f, {0.6f, 0.6f, 0.7f, mascotAlpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

        // --- ここから下はホバー時のみ表示 (hoverAlpha_ を適用) ---
        if (hoverAlpha_ > 0.01f) {
            // 背景の現在色（ボタンのフェードイン用）
            Engine::Vector4 currentBgColor = {
                bgWork.x * (1.0f - pomodoroTransition_) + bgRelax.x * pomodoroTransition_,
                bgWork.y * (1.0f - pomodoroTransition_) + bgRelax.y * pomodoroTransition_,
                bgWork.z * (1.0f - pomodoroTransition_) + bgRelax.z * pomodoroTransition_,
                1.0f
            };

            // 丸みを帯びたボタン背景とアイコンを描画するラムダ
            auto drawRoundedBtn = [&](float bx, float by, float bw, float bh, const char* icon, float scale, Engine::Vector4 c, bool isCircle) {
                bool isHover = (mascotMx >= bx && mascotMx <= bx + bw && mascotMy >= by && mascotMy <= by + bh);

                Engine::Vector4 btnTargetColor = {0.2f, 0.22f, 0.28f, 1.0f};
                Engine::Vector4 defaultBgColorBtn = {
                    currentBgColor.x + (btnTargetColor.x - currentBgColor.x) * hoverAlpha_,
                    currentBgColor.y + (btnTargetColor.y - currentBgColor.y) * hoverAlpha_,
                    currentBgColor.z + (btnTargetColor.z - currentBgColor.z) * hoverAlpha_,
                    1.0f
                };
                
                Engine::Vector4 bgColorBtn = defaultBgColorBtn;
                Engine::Vector4 iconColor = c;
                
                // ホバー時の色反転と拡大
                if (isHover) {
                    bx -= bw * 0.05f; by -= bh * 0.05f;
                    bw *= 1.1f; bh *= 1.1f;
                    scale *= 1.1f;
                    
                    // 背景とアイコンの色を完全に反転（アイコンは元の背景色に近い暗い色に）
                    bgColorBtn = c;
                    iconColor = {0.1f, 0.12f, 0.15f, 1.0f}; // 視認性の高い暗い色
                }
                
                bgColorBtn.w *= mascotAlpha;
                iconColor.w *= hoverAlpha_ * mascotAlpha;

                // ボタン背後全体のアンビエントグロウ（アイコン外側に淡い光を漏らす）
                Engine::Vector4 btnGlowColor = c;
                // 全体のガウスを強め、淡く美しく広げる
                btnGlowColor.w *= (isHover ? 1.5f : 1.2f) * hoverAlpha_ * mascotAlpha; 
                float glowRadius = (bw / 2.0f) + 28.0f; // 広範囲に淡い光を漏らす
                drawGaussianCircle(bx + bw / 2.0f, by + bh / 2.0f, glowRadius, btnGlowColor);

                if (isCircle) {
                    drawSolidCircle(bx + bw / 2.0f, by + bh / 2.0f, bw / 2.0f, bgColorBtn);
                } else {
                    float r = 8.0f;
                    Engine::Renderer::SpriteDesc lr{};
                    lr.color = bgColorBtn;
                    lr.x = bx + r; lr.y = by; lr.w = bw - r * 2.0f; lr.h = bh;
                    renderer_->DrawSprite(whiteTex_, lr);
                    lr.x = bx; lr.y = by + r; lr.w = bw; lr.h = bh - r * 2.0f;
                    renderer_->DrawSprite(whiteTex_, lr);
                    drawSolidCircle(bx + r, by + r, r, bgColorBtn);
                    drawSolidCircle(bx + bw - r, by + r, r, bgColorBtn);
                    drawSolidCircle(bx + r, by + bh - r, r, bgColorBtn);
                    drawSolidCircle(bx + bw - r, by + bh - r, r, bgColorBtn);
                }
                
                // アイコンの中身にはガウスをかけず、シャープな本体のみを描画する
                if (strcmp(icon, "||") == 0) {
                    float cxBtn = bx + bw/2.0f; float cyBtn = by + bh/2.0f;
                    
                    Engine::Renderer::SpriteDesc b1{}, b2{};
                    b1.color = iconColor; b2.color = iconColor;
                    b1.w = 5.0f; b1.h = 18.0f;
                    b2.w = 5.0f; b2.h = 18.0f;
                    b1.x = cxBtn - 5.5f - b1.w/2.0f; b1.y = cyBtn - b1.h/2.0f;
                    b2.x = cxBtn + 5.5f - b2.w/2.0f; b2.y = cyBtn - b2.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, b1); renderer_->DrawSprite(whiteTex_, b2);
                    
                } else if (strcmp(icon, "■") == 0) {
                    Engine::Renderer::SpriteDesc sq{};
                    sq.color = iconColor;
                    sq.w = 16.0f; sq.h = 16.0f;
                    sq.x = bx + bw/2.0f - sq.w/2.0f; sq.y = by + bh/2.0f - sq.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, sq);
                    
                } else {
                    const char* iconFont = "C:\\Windows\\Fonts\\meiryo.ttc";
                    float iw = renderer_->MeasureTextWidth(icon, scale, iconFont);
                    
                    float iconOffsetY = 0.0f;
                    float iconOffsetX = 0.0f;
                    if (strcmp(icon, "▶") == 0) {
                        iconOffsetY = scale * 23.5f; 
                        iconOffsetX = 3.0f;          
                    } else if (strcmp(icon, "↺") == 0) {
                        iconOffsetY = scale * 30.5f; // 最適な高さに微調整
                        iconOffsetX = 0.0f;
                    } else {
                        iconOffsetY = scale * 18.0f;
                    }
                    
                    float tx = bx + bw/2.0f - iw/2.0f + iconOffsetX;
                    float ty = by + bh/2.0f - iconOffsetY;
                    
                    // アイコン自体にはガウスをかけず、シャープに描画
                    renderer_->DrawString(icon, tx, ty, scale, iconColor, iconFont);
                }
            };
            
            // アイコンの色を統一するため、現在のテーマカラーを使用
            Engine::Vector4 themeCol = currentColor_;
            
            // ゲージが下がった分、ボタン全体の配置を5px下にずらす
            // 全て円形ボタン（isCircle = true）に統一し、美しいUIへ
            // Reset (X: 60, Y: 275)
            drawRoundedBtn(60.0f, 275.0f, 30.0f, 30.0f, "↺", 0.6f, themeCol, true);
            
            // Play/Pause (X: 105, Y: 270)
            drawRoundedBtn(105.0f, 270.0f, 40.0f, 40.0f, isPomodoroPaused_ ? "▶" : "||", 0.7f, themeCol, true);
            
            // Stop (X: 160, Y: 275)
            drawRoundedBtn(160.0f, 275.0f, 30.0f, 30.0f, "■", 0.55f, themeCol, true);
        } // if (hoverAlpha_ > 0.01f) の閉じカッコ
    } // if (drawMascot) の閉じカッコ

    if (drawImmersive) {

        // --- 心穏やかな場所 UI（ImGuiを使わず独自のDrawStringで描画） ---
        if (drawUI) {
            // スケールを全体的に倍増させる
            renderer_->DrawString("心穏やかな場所", 50.0f, 60.0f, 0.8f, {0.85f, 0.85f, 0.95f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("Tranquil Space", 50.0f, 110.0f, 0.5f, {0.8f, 0.8f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            // 文字が大きくなった分、X座標を左に寄せる
            float rightX = Engine::WindowDX::kW - 650.0f;
            float bottomY = Engine::WindowDX::kH - 180.0f;
            renderer_->DrawString("AIのささやき：", rightX, bottomY, 0.5f, {0.7f, 0.7f, 0.8f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("『ここは、あなたの心を休める場所です。』", rightX, bottomY + 45.0f, 0.4f, {0.7f, 0.7f, 0.8f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("※右クリックで小さな姿に戻ります", rightX, bottomY + 95.0f, 0.35f, {0.6f, 0.6f, 0.7f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        }

        if (drawUI) {
            // --- フェーズ1: エコモード＆ポモドーロステータス表示 ---
            float leftX = 50.0f;
            float infoY = Engine::WindowDX::kH - 180.0f; // 大きくした分少し上へずらす
            
            char loadText[256];
            snprintf(loadText, sizeof(loadText), "System Load: < 0.1%% | FrameTime: %.2fms", dt_ * 1000.0f);
            // 0.3f -> 0.5f へさらに拡大
            renderer_->DrawString(loadText, leftX, infoY, 0.5f, {0.5f, 0.5f, 0.6f, 0.8f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("=== 圧倒的静寂 ===", leftX, infoY + 60.0f, 0.5f, {0.4f, 0.4f, 0.5f, 0.6f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
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
            
            // ポモドーロのステータス色で描画（0.6f -> 1.2f にして超特大に）
            Engine::Vector4 pColor = currentColor_;
            renderer_->DrawString(pomoText, leftX, 160.0f, 1.2f, pColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        }
    }
}

void MainScene::DrawUI() {
    auto* input = Engine::Input::GetInstance();
    float mx = 0.0f, my = 0.0f;
    if (input) input->GetMousePos(mx, my);

#ifdef USE_IMGUI
    // このソフトウェアではImGuiのUIを一切使用しない
#endif

    if (!isMascotMode_) {
        // --- 独自実装の気持ち入力UI ---
        // 画面の論理サイズを基準にして完璧な下中央に配置
        float cx = Engine::WindowDX::kW / 2.0f;
        float cy = Engine::WindowDX::kH - 80.0f;
        float boxW = 600.0f;
        float boxH = 50.0f;
        
        // 背景枠を描画
        Engine::Renderer::SpriteDesc bg{};
        bg.w = boxW; bg.h = boxH;
        bg.x = cx - boxW/2.0f; bg.y = cy - boxH/2.0f;
        bg.color = {1.0f, 1.0f, 1.0f, 0.05f};
        renderer_->DrawSprite(whiteTex_, bg);
        
        // 枠線（上下）
        Engine::Renderer::SpriteDesc line{};
        line.color = {1.0f, 1.0f, 1.0f, 0.2f};
        line.w = boxW; line.h = 1.0f;
        line.x = bg.x; line.y = bg.y; renderer_->DrawSprite(whiteTex_, line);
        line.y = bg.y + boxH; renderer_->DrawSprite(whiteTex_, line);

        float btnW = 50.0f; float btnH = 50.0f;
        float btnX = Engine::WindowDX::kW - btnW - 30.0f;
        float btnY = 30.0f;

        bool gearHover = (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH);
        float gearBoost = gearHover ? 0.2f : 0.0f;

        // 設定ボタンの背景（画像が透明でなかったり見えづらい場合のための座布団）
        Engine::Renderer::SpriteDesc btnBg{};
        btnBg.w = btnW + 10.0f; btnBg.h = btnH + 10.0f;
        btnBg.x = btnX - 5.0f; btnBg.y = btnY - 5.0f;
        btnBg.color = {0.1f + gearBoost, 0.1f + gearBoost, 0.15f + gearBoost, 0.8f}; // ホバー時に明るくする
        btnBg.layer = 89;
        renderer_->DrawSprite(roundedRectTex_ ? roundedRectTex_ : whiteTex_, btnBg);

        // 設定ボタンの描画（プログラムによる歯車アイコン）
        auto drawGear = [&](float cx, float cy, float size, Engine::Vector4 color, int layer) {
            // 歯（十字と斜め十字）
            for(int i = 0; i < 4; ++i) {
                Engine::Renderer::SpriteDesc sd{};
                sd.w = size; sd.h = size * 0.25f;
                sd.x = cx - sd.w/2.0f; sd.y = cy - sd.h/2.0f;
                sd.rotationRad = (i * 3.14159f) / 4.0f;
                sd.color = color;
                sd.layer = layer;
                renderer_->DrawSprite(whiteTex_, sd);
            }
            // 中心の円
            float r2 = (size * 0.35f) * (size * 0.35f);
            for (float dy = -size*0.35f; dy <= size*0.35f; dy += 1.0f) {
                float dx = sqrtf(std::max(0.0f, r2 - dy * dy));
                Engine::Renderer::SpriteDesc sd{};
                sd.w = dx * 2.0f; sd.h = 1.5f;
                sd.x = cx - dx; sd.y = cy + dy - sd.h/2.0f;
                sd.color = color; sd.layer = layer;
                renderer_->DrawSprite(whiteTex_, sd);
            }
            // 穴（背景色でくり抜く）
            Engine::Vector4 holeColor = {0.1f, 0.1f, 0.15f, 0.8f}; // btnBgと同じ色
            float hr2 = (size * 0.15f) * (size * 0.15f);
            for (float dy = -size*0.15f; dy <= size*0.15f; dy += 1.0f) {
                float dx = sqrtf(std::max(0.0f, hr2 - dy * dy));
                Engine::Renderer::SpriteDesc sd{};
                sd.w = dx * 2.0f; sd.h = 1.5f;
                sd.x = cx - dx; sd.y = cy + dy - sd.h/2.0f;
                sd.color = holeColor; sd.layer = layer + 1; // 穴はギアより手前
                renderer_->DrawSprite(whiteTex_, sd);
            }
        };
        // ギアアイコンを描画
        float gearSize = gearHover ? 36.0f : 32.0f;
        drawGear(btnX + btnW/2.0f, btnY + btnH/2.0f, gearSize, {0.9f, 0.9f, 0.9f, 1.0f}, 90);

        // テキストの描画
        if (isSettingsOpen_) {
            // 設定メニューの背景パネル
            float panelW = 500.0f; float panelH = 340.0f;
            float panelX = Engine::WindowDX::kW / 2.0f - panelW / 2.0f;
            float panelY = Engine::WindowDX::kH / 2.0f - panelH / 2.0f;

            Engine::Renderer::SpriteDesc sbg{};
            sbg.w = panelW; sbg.h = panelH;
            sbg.x = panelX; sbg.y = panelY;
            sbg.color = {0.05f, 0.05f, 0.05f, 0.95f};
            sbg.layer = 100; // パネルの背景
            renderer_->DrawSprite(whiteTex_, sbg);
            
            renderer_->DrawString("Color Palette", sbg.x + 20.0f, sbg.y + 20.0f, 0.6f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("Select your environment", sbg.x + 20.0f, sbg.y + 55.0f, 0.35f, {0.6f,0.6f,0.6f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            // 角丸四角形を描画するラムダ
            auto drawRoundedRectUI = [&](float bx, float by, float bw, float bh, float r, Engine::Vector4 c, int layer) {
                // 十字の矩形を描画
                Engine::Renderer::SpriteDesc lr{};
                lr.color = c; lr.layer = layer;
                lr.x = bx + r; lr.y = by; lr.w = bw - r * 2.0f; lr.h = bh;
                renderer_->DrawSprite(whiteTex_, lr);
                lr.x = bx; lr.y = by + r; lr.w = bw; lr.h = bh - r * 2.0f;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 4つの角を円で描画 (スライス方式で完全に塗りつぶす)
                auto drawSolidCircleUI = [&](float cx, float cy, float radius) {
                    float r2 = radius * radius;
                    for (float dy = -radius; dy <= radius; dy += 1.0f) {
                        float dx = sqrtf(std::max(0.0f, r2 - dy * dy));
                        Engine::Renderer::SpriteDesc sd{};
                        sd.w = dx * 2.0f;
                        sd.h = 1.5f;
                        sd.x = cx - dx;
                        sd.y = cy + dy - sd.h/2.0f;
                        sd.color = c;
                        sd.layer = layer;
                        renderer_->DrawSprite(whiteTex_, sd);
                    }
                };
                drawSolidCircleUI(bx + r, by + r, r);
                drawSolidCircleUI(bx + bw - r, by + r, r);
                drawSolidCircleUI(bx + r, by + bh - r, r);
                drawSolidCircleUI(bx + bw - r, by + bh - r, r);
            };

            // 4つのカラースウォッチを描画
            for (int i = 0; i < (int)themes_.size(); ++i) {
                float sx = panelX + 50.0f + i * 110.0f;
                float sy = panelY + 80.0f;
                
                // 選択中の場合は枠を少し大きめに描画
                if (i == currentThemeIndex_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // スウォッチ本体
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, themes_[i].uiWork, 102);
                
                // テーマ名
                float tw = renderer_->MeasureTextWidth(themes_[i].name.c_str(), 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(themes_[i].name.c_str(), sx + 35.0f - tw/2.0f, sy + 75.0f, 0.3f, {0.8f,0.8f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
            
            // --- インタラクションモードのUI描画 ---
            renderer_->DrawString("Interaction Mode", sbg.x + 20.0f, sbg.y + 160.0f, 0.45f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            const char* modeNames[] = { "Repel", "Gravity", "Ripple", "Popcorn" };
            Engine::Vector4 modeColors[] = {
                {0.5f, 0.6f, 0.7f, 1.0f}, // Repel: 落ち着いたブルー系
                {0.8f, 0.3f, 0.5f, 1.0f}, // Gravity: 吸い込むような深い赤/紫系
                {0.3f, 0.8f, 0.8f, 1.0f}, // Ripple: 水の波紋のようなシアン系
                {1.0f, 0.8f, 0.3f, 1.0f}  // Popcorn: はじけるポップコーンのようなイエロー系
            };

            for (int i = 0; i < 4; ++i) {
                float sx = panelX + 50.0f + i * 110.0f;
                float sy = panelY + 200.0f;
                
                // 選択中の場合は枠を描画
                if (i == interactionMode_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // モードボタン本体
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, modeColors[i], 102);
                
                // モードごとの簡易アイコン（図形）
                Engine::Vector4 iconCol = {1.0f, 1.0f, 1.0f, 0.9f};
                if (i == 0) {
                    // Repel: 離れる矢印のような線
                    Engine::Renderer::SpriteDesc sd{};
                    sd.color = iconCol; sd.layer = 103;
                    sd.w = 20.0f; sd.h = 4.0f;
                    sd.x = sx + 35.0f - sd.w/2.0f; sd.y = sy + 35.0f - sd.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, sd);
                } else if (i == 1) {
                    // Gravity: 中心に向かうような表現（円）
                    Engine::Renderer::SpriteDesc sd{};
                    sd.color = iconCol; sd.layer = 103;
                    sd.w = 16.0f; sd.h = 16.0f;
                    sd.x = sx + 35.0f - sd.w/2.0f; sd.y = sy + 35.0f - sd.h/2.0f;
                    sd.shaderName = "GaussianUI";
                    renderer_->DrawSprite(whiteTex_, sd);
                } else if (i == 2) {
                    // Ripple: 水面の波紋のような枠
                    Engine::Renderer::SpriteDesc sd{};
                    sd.color = iconCol; sd.layer = 103;
                    sd.w = 24.0f; sd.h = 4.0f;
                    sd.x = sx + 35.0f - sd.w/2.0f; sd.y = sy + 35.0f - 10.0f;
                    renderer_->DrawSprite(whiteTex_, sd);
                    sd.x = sx + 35.0f - sd.w/2.0f; sd.y = sy + 35.0f + 6.0f;
                    renderer_->DrawSprite(whiteTex_, sd);
                } else if (i == 3) {
                    // Popcorn: 弾ける破片
                    for (int p = 0; p < 5; ++p) {
                        Engine::Renderer::SpriteDesc sd{};
                        sd.color = iconCol; sd.layer = 103;
                        sd.w = 6.0f; sd.h = 6.0f;
                        float ang = p * 1.25f;
                        sd.x = sx + 35.0f - 3.0f + cosf(ang) * 12.0f;
                        sd.y = sy + 35.0f - 3.0f + sinf(ang) * 12.0f;
                        sd.rotationRad = ang;
                        renderer_->DrawSprite(whiteTex_, sd);
                    }
                }
                
                // モード名
                float tw = renderer_->MeasureTextWidth(modeNames[i], 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(modeNames[i], sx + 35.0f - tw/2.0f, sy + 75.0f, 0.3f, {0.8f,0.8f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
        } else {
            // 気持ち入力UIの描画
            std::string displayStr = inputBuffer_;
            if (displayStr.empty()) {
                // プレースホルダー
                const char* hint = "Type your feelings and press Enter...";
                float hintW = renderer_->MeasureTextWidth(hint, 0.45f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(hint, cx - hintW/2.0f, cy - 25.0f, 0.45f, {0.6f, 0.6f, 0.6f, 0.8f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            } else {
                // カーソル点滅
                if ((transitionTimer_ / 30) % 2 == 0) {
                    displayStr += "_";
                }
                float textW = renderer_->MeasureTextWidth(displayStr.c_str(), 0.5f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(displayStr.c_str(), cx - textW/2.0f, cy - 28.0f, 0.5f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
        }
    }

    // 描画キューに溜まったUI用スプライトとテキストを正しい順序でフラッシュする
    if (renderer_) {
        renderer_->FlushSprites();
        renderer_->FlushText();
    }
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

// ==========================================
// Phase 1: ファイルI/O 処理
// ==========================================
void MainScene::LoadData() {
    std::ifstream f("tranquil_save.json");
    if (!f.is_open()) return;

    try {
        json j;
        f >> j;

        if (j.contains("themeIndex")) {
            currentThemeIndex_ = j["themeIndex"].get<int>();
            if (currentThemeIndex_ < 0 || currentThemeIndex_ >= (int)themes_.size()) currentThemeIndex_ = 0;
        }
        
        if (j.contains("totalWorkTime")) {
            totalWorkTime_ = j["totalWorkTime"].get<float>();
        }
        if (j.contains("feelings")) {
            for (const auto& feeling : j["feelings"]) {
                savedFeelings_.push_back(feeling.get<std::string>());
            }
            
            // ロードした過去の気持ちをランダムに宇宙の星（ノード）に散りばめる
            if (!savedFeelings_.empty() && !boids_.empty()) {
                // 最大15個の星に過去の記憶を宿らせる
                int count = std::min((int)savedFeelings_.size(), 15);
                for (int i = 0; i < count; ++i) {
                    int randIdx = rand() % boids_.size();
                    boids_[randIdx].customText = savedFeelings_[rand() % savedFeelings_.size()];
                    boids_[randIdx].scale = 5.0f; // やや大きく
                    boids_[randIdx].color = {0.8f, 0.8f, 1.0f, 0.8f}; // 淡い青白に
                }
            }
        }
    } catch (...) {
        // パースエラー時は何もしない（ファイル破損時など）
    }
}

void MainScene::SaveData() {
    try {
        json j;
        j["totalWorkTime"] = totalWorkTime_;
        j["themeIndex"] = currentThemeIndex_;
        j["feelings"] = savedFeelings_;
        
        std::ofstream f("tranquil_save.json");
        if (f.is_open()) {
            f << j.dump(4);
        }
    } catch (...) {
        // セーブ失敗時
    }
}

} // namespace Game