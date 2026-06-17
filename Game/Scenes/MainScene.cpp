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
#include "../../Engine/Audio.h"

#include <fstream>
#include <complex>
#include <cmath>
#include <thread>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include "../../Engine/ThirdParty/nlohmann/json.hpp"
using json = nlohmann::json;

#define APP_VERSION "Beta v0.1.0"

namespace Game {

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring wstr(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

static void CooleyTukeyFFT(std::vector<std::complex<float>>& a) {
    size_t n = a.size();
    if (n <= 1) return;

    std::vector<std::complex<float>> a0(n / 2), a1(n / 2);
    for (size_t i = 0; i * 2 < n; ++i) {
        a0[i] = a[i * 2];
        a1[i] = a[i * 2 + 1];
    }

    CooleyTukeyFFT(a0);
    CooleyTukeyFFT(a1);

    float angle = 2.0f * 3.1415926535f / n;
    std::complex<float> w(1.0f), wn(cosf(angle), -sinf(angle));
    for (size_t i = 0; i * 2 < n; ++i) {
        a[i] = a0[i] + w * a1[i];
        a[i + n / 2] = a0[i] - w * a1[i];
        w *= wn;
    }
}

MainScene::~MainScene() {
    if (audioCapture_) {
        audioCapture_->Shutdown();
    }
    if (smtcMonitor_) {
        smtcMonitor_->Shutdown();
    }
}

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

    // 背景透過のためSkyboxを無効化、アウトライン用のPostProcessを有効化
    if (renderer_) {
        renderer_->SetUseCubemapBackground(false);
        renderer_->SetPostProcessEnabled(true);
        renderer_->SetPostEffect("OutlinePost");
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
        
        // 天球上のランダムな座標（極座標）
        float theta = (rand() % 360) * 3.14159f / 180.0f;
        float phi = acosf(2.0f * (rand() % 1000) / 1000.0f - 1.0f);
        b.baseGlobePos.x = sinf(phi) * cosf(theta);
        b.baseGlobePos.y = cosf(phi);
        b.baseGlobePos.z = sinf(phi) * sinf(theta);
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
        dx_->SetSourceSize(250, 320); // 起動時もマスコットモード用にスケーリングソースを設定
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

    // 音楽連携モニターの初期化
    smtcMonitor_ = std::make_unique<SMTCMonitor>();
    smtcMonitor_->Initialize();

    // 音声キャプチャの初期化
    audioCapture_ = std::make_unique<AudioLoopbackCapture>();
    audioCapture_->Initialize();

    auto* audio = Engine::Audio::GetInstance();
    if (audio) {
        soundSend_ = audio->Load("Assets/Sounds/send.wav");
        soundZen_ = audio->Load("Assets/Sounds/zen_mode.wav");
        soundFocus_ = audio->Load("Assets/Sounds/focus_finish.wav");
        soundHit_ = audio->Load("Assets/Sounds/hit.wav");
    }

    // 起動時にアップデートを確認
    CheckForUpdateInfo();
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
        float targetDuration = isPomodoroWork_ ? (pWorkDurationMinutes_ * 60.0f) : (pRelaxDurationMinutes_ * 60.0f);
        if (pomodoroTimer_ >= targetDuration) {
            pomodoroTimer_ = 0.0f; // 完全に0にリセットし、フレーム落ちによるループバックバグを防ぐ
            isPomodoroWork_ = !isPomodoroWork_;
            // セッション終了時（集中完了時）にセーブ
            SaveData();
            if (Engine::Audio::GetInstance()) Engine::Audio::GetInstance()->Play(soundFocus_, false, seVolume_);
        }
    }

    // 色の緩やかな遷移 (Lerp)
    float targetTransition = isPomodoroWork_ ? 0.0f : 1.0f;
    pomodoroTransition_ += (targetTransition - pomodoroTransition_) * 1.0f * dt_;

    // 作業中（集中を促す寒色） -> 休憩中（リラックスを促す暖色）
    Engine::Vector4 workColor = themes_[currentThemeIndex_].uiWork;
    Engine::Vector4 restColor = themes_[currentThemeIndex_].uiRelax;
    
    if (healingWordCooldown_ > 0.0f) {
        healingWordCooldown_ -= dt_;
    }
    
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

    if (isMascotMode_ && transitionTimer_ == 0) {
        mascotFadeAlpha_ += (1.0f - mascotFadeAlpha_) * 10.0f * dt_;
    } else {
        mascotFadeAlpha_ = 0.0f;
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
                dx_->SetSourceSize(250, 320); // マスコットモード時は1:1クロップにする
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
                    
                    // アニメーション中は一時的に枠を完全に消してスムーズに展開する
                    SetWindowRgn(dx_->GetHwnd(), NULL, TRUE); // 角丸を解除して四角にする
                    dx_->SetSourceSize(1920, 1080); // フルスクリーン描画の全体ストレッチに戻す
                    
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
                
                SetWindowPos(dx_->GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }
            
            // --- 天球儀モードのマウス操作 ---
            if (visualMode_ == 1 && !isSettingsOpen_) {
                if (input->IsMouseTrigger(0)) {
                    isGlobeDragging_ = true;
                    globeLastMouseX_ = mx;
                    globeLastMouseY_ = my;
                    globeVelX_ = 0.0f;
                    globeVelY_ = 0.0f;
                } else if (!input->IsMouseDown(0)) {
                    isGlobeDragging_ = false;
                }
                
                if (isGlobeDragging_ && input->IsMouseDown(0)) {
                    float dxM = mx - globeLastMouseX_;
                    float dyM = my - globeLastMouseY_;
                    
                    // マウス移動量から速度を計算し、そのまま勢いとして保持
                    globeVelY_ = dxM * 0.005f; // X軸移動でY軸回転
                    globeVelX_ = dyM * 0.005f; // Y軸移動でX軸回転
                    
                    globeRotY_ += globeVelY_;
                    globeRotX_ += globeVelX_;
                    
                    // X軸の回転は上下反転しすぎないように制限
                    if (globeRotX_ > 1.5f) { globeRotX_ = 1.5f; globeVelX_ = 0.0f; }
                    if (globeRotX_ < -1.5f) { globeRotX_ = -1.5f; globeVelX_ = 0.0f; }
                    
                    globeLastMouseX_ = mx;
                    globeLastMouseY_ = my;
                } else if (!isGlobeDragging_) {
                    // ドラッグしていないときは慣性（ハンドスピナーの勢い）で回る
                    globeRotY_ += globeVelY_;
                    globeRotX_ += globeVelX_;
                    if (globeRotX_ > 1.5f) { globeRotX_ = 1.5f; globeVelX_ = 0.0f; }
                    if (globeRotX_ < -1.5f) { globeRotX_ = -1.5f; globeVelX_ = 0.0f; }
                    
                    // 摩擦で勢いを減衰させる
                    globeVelX_ *= 0.92f; 
                    // Y軸は元のゆっくりとした自転速度（-0.0005f）に少しずつ戻っていく
                    globeVelY_ = globeVelY_ * 0.96f + (-0.0005f) * 0.04f;
                }
            } else {
                isGlobeDragging_ = false;
                if (visualMode_ == 1) {
                    // 設定パネルを開いていても慣性・自転は維持する
                    globeRotY_ += globeVelY_;
                    globeRotX_ += globeVelX_;
                    if (globeRotX_ > 1.5f) { globeRotX_ = 1.5f; globeVelX_ = 0.0f; }
                    if (globeRotX_ < -1.5f) { globeRotX_ = -1.5f; globeVelX_ = 0.0f; }
                    globeVelX_ *= 0.92f;
                    globeVelY_ = globeVelY_ * 0.96f + (-0.0005f) * 0.04f;
                }
            }

            // --- 設定メニューとカラーパレットのマウス判定 ---
            float btnW = 50.0f; float btnH = 50.0f;
            float btnX = Engine::WindowDX::kW - btnW - 30.0f;
            float btnY = 30.0f;

            // アニメーション更新 (イージング)
            if (isSettingsOpen_) {
                settingsSlideOffset_ += (1.0f - settingsSlideOffset_) * 0.15f;
            } else {
                settingsSlideOffset_ += (0.0f - settingsSlideOffset_) * 0.15f;
            }

            float panelW = 620.0f; 
            float contentH = 1900.0f; // コンテンツ自体の高さを拡張
            
            // 右寄せで表示するためのターゲットX座標
            float targetPanelX = Engine::WindowDX::kW - panelW;
            // 非表示時の画面外X座標
            float offscreenPanelX = Engine::WindowDX::kW;
            float panelX = offscreenPanelX + (targetPanelX - offscreenPanelX) * settingsSlideOffset_;
            
            if (isSettingsOpen_) {
                float wheel = input->GetMouseWheelDelta();
                if (wheel != 0.0f) {
                    menuScrollY_ += wheel * 30.0f;
                }
                // 上下マージン50pxずつ考慮
                float maxScroll = (contentH + 100.0f > (float)Engine::WindowDX::kH) ? (contentH + 100.0f - (float)Engine::WindowDX::kH) : 0.0f;
                if (menuScrollY_ > 0.0f) menuScrollY_ = 0.0f;
                if (menuScrollY_ < -maxScroll) menuScrollY_ = -maxScroll;
            }
            float panelY = menuScrollY_ + 50.0f;

            bool handledMouse = false;
            
            // スライダーのドラッグ判定
            if (isSettingsOpen_) {
                static int draggingSliderId = -1;
                auto handleSlider = [&](int id, float sx, float sy, float w, float h, float& val) {
                    if (input->IsMouseTrigger(0)) {
                        if (mx >= sx - 15.0f && mx <= sx + w + 15.0f && my >= sy - 15.0f && my <= sy + h + 15.0f) {
                            draggingSliderId = id;
                        }
                    }
                    if (input->IsMouseDown(0) && draggingSliderId == id) {
                        val = (mx - sx) / w;
                        if (val < 0.0f) val = 0.0f;
                        if (val > 1.0f) val = 1.0f;
                        if (val < 0.01f) val = 0.0f;
                        if (val > 0.99f) val = 1.0f;
                        return true;
                    }
                    if (!input->IsMouseDown(0) && draggingSliderId == id) {
                        draggingSliderId = -1;
                    }
                    return false;
                };
                
                bool sliderChanged = false;
                if (useCustomNodeColor_) {
                    if (handleSlider(1, panelX + 140.0f, panelY + 940.0f, 110.0f, 15.0f, customNodeColor_.x)) sliderChanged = true;
                    if (handleSlider(2, panelX + 140.0f, panelY + 980.0f, 110.0f, 15.0f, customNodeColor_.y)) sliderChanged = true;
                    if (handleSlider(3, panelX + 140.0f, panelY + 1020.0f, 110.0f, 15.0f, customNodeColor_.z)) sliderChanged = true;
                }
                if (useCustomTextColor_) {
                    if (handleSlider(4, panelX + 440.0f, panelY + 940.0f, 110.0f, 15.0f, customTextColor_.x)) sliderChanged = true;
                    if (handleSlider(5, panelX + 440.0f, panelY + 980.0f, 110.0f, 15.0f, customTextColor_.y)) sliderChanged = true;
                    if (handleSlider(6, panelX + 440.0f, panelY + 1020.0f, 110.0f, 15.0f, customTextColor_.z)) sliderChanged = true;
                }

                // ポモドーロWork時間 (10〜100分、1分刻み)
                float workVal = (pWorkDurationMinutes_ - 10.0f) / 90.0f;
                if (handleSlider(7, panelX + 40.0f, panelY + 1260.0f, 220.0f, 15.0f, workVal)) {
                    pWorkDurationMinutes_ = 10.0f + workVal * 90.0f;
                    sliderChanged = true;
                }
                // ポモドーロRelax時間 (1〜60分、1分刻み)
                float relaxVal = (pRelaxDurationMinutes_ - 1.0f) / 59.0f;
                if (handleSlider(8, panelX + 320.0f, panelY + 1260.0f, 220.0f, 15.0f, relaxVal)) {
                    pRelaxDurationMinutes_ = 1.0f + relaxVal * 59.0f;
                    sliderChanged = true;
                }

                // 音量 SE Vol
                if (handleSlider(9, panelX + 130.0f, panelY + 1570.0f, 410.0f, 15.0f, seVolume_)) {
                    auto* audio = Engine::Audio::GetInstance();
                    if (audio) audio->SetMasterSEVolume(seVolume_);
                    sliderChanged = true;
                }

                if (sliderChanged) {
                    handledMouse = true;
                    // 保存は頻繁に行うと重いかもしれないので一旦省略、離した時に保存するのが良いが...
                    // 色反映のためにここではSaveData()を呼ばず、変数を更新するのみ
                    // 次のフレームで色が更新される
                }
            }
            
            // ドラッグ終了時にセーブするため
            static bool wasSliderDragging = false;
            if (isSettingsOpen_ && !input->IsMouseDown(0) && wasSliderDragging) {
                SaveData();
                wasSliderDragging = false;
            } else if (isSettingsOpen_ && input->IsMouseDown(0) && handledMouse) {
                wasSliderDragging = true;
            }

            if (input->IsMouseTrigger(0)) {
                if (isSettingsOpen_) {
                    // カラーパレットのクリック判定
                    for (int i = 0; i < (int)themes_.size(); ++i) {
                        float sx = panelX + 50.0f + i * 110.0f;
                        float sy = panelY + 90.0f;
                        if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                            currentThemeIndex_ = i;
                            SaveData();
                            handledMouse = true;
                            break;
                        }
                    }
                    
                    // Visual Modeのクリック判定
                    if (!handledMouse) {
                        for (int i = 0; i < 4; ++i) {
                            float sx = panelX + 35.0f + i * 115.0f;
                            float sy = panelY + 250.0f;
                            if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                                int oldMode = visualMode_;
                                visualMode_ = i;
                                if (visualMode_ == 1) {
                                    interactionMode_ = 3; // 天球儀のときはPopcorn固定
                                }
                                
                                // ツリーモード(3)へ切り替わった時の特別な初期化
                                if (visualMode_ == 3 && oldMode != 3) {
                                    // 画面下から長い線が伸びっぱなしになるのを防ぐため、
                                    // 全てのノードを一旦ツリーの根元に集め、速度をリセットする。
                                    // これにより「根元から一気に枝が開花する」美しいアニメーションになる。
                                    float rootX = Engine::WindowDX::kW / 2.0f;
                                    float rootY = Engine::WindowDX::kH - 120.0f;
                                    for (auto& b : boids_) {
                                        b.position.x = rootX;
                                        b.position.y = rootY;
                                        b.velocity = {0.0f, 0.0f, 0.0f};
                                    }
                                }

                                // ツリー(3)以外のモードへ移行した際は、ノード数やテキストを適正な状態にリセットする。
                                // 特に1500個まで増えたツリーから切り替えた際、そのまま引き継ぐとカオスになるため。
                                if (visualMode_ != 3) {
                                    // ノード数自体を適正値（400）に間引く
                                    if (boids_.size() > 400) {
                                        boids_.resize(400); 
                                    }
                                    
                                    // さらに、テキストを持っているノードを大幅に減らし、大半を「ただの星/ブロック」に戻す
                                    int keptTextCount = 0;
                                    for (auto& b : boids_) {
                                        if (!b.customText.empty()) {
                                            // 残すテキストの上限を40個程度とし、さらにランダムで間引く
                                            if (keptTextCount >= 40 || (rand() % 100) < 75) {
                                                b.customText.clear();
                                                b.scale = 2.0f + (rand() % 60) / 10.0f; // 通常の星（ブロック）のサイズに戻す
                                            } else {
                                                keptTextCount++;
                                            }
                                        }
                                    }
                                }
                                SaveData();
                                handledMouse = true;
                                break;
                            }
                        }
                    }
                    
                    // インタラクションモードのクリック判定
                    if (!handledMouse) {
                        for (int i = 0; i < 5; ++i) {
                            float sx = panelX + 50.0f + (i % 4) * 110.0f;
                            float sy = panelY + 410.0f + (i / 4) * 110.0f; // 410にずらす
                            if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                                // 天球儀モード中はポップコーン（3）以外選択不可
                                if (visualMode_ == 1 && i != 3) {
                                    continue;
                                }
                                interactionMode_ = i;
                                SaveData();
                                handledMouse = true;
                                break;
                            }
                        }
                    }
                    
                    // Node Shapeのクリック判定
                    if (!handledMouse) {
                        for (int i = 0; i < 4; ++i) {
                            float sx = panelX + 50.0f + i * 110.0f;
                            float sy = panelY + 700.0f; // さらに下にずらす
                            if (mx >= sx && mx <= sx + 70.0f && my >= sy && my <= sy + 70.0f) {
                                currentShapeMode_ = i;
                                SaveData();
                                handledMouse = true;
                                break;
                            }
                        }
                    }
                    
                    // Custom Color & New Settings Toggle のクリック判定
                    if (!handledMouse) {
                        // Node Color Toggle
                        if (mx >= panelX + 20.0f && mx <= panelX + 290.0f && my >= panelY + 890.0f && my <= panelY + 915.0f) {
                            useCustomNodeColor_ = !useCustomNodeColor_;
                            SaveData();
                            handledMouse = true;
                        }
                        // Text Color Toggle
                        else if (mx >= panelX + 310.0f && mx <= panelX + 580.0f && my >= panelY + 890.0f && my <= panelY + 915.0f) {
                            useCustomTextColor_ = !useCustomTextColor_;
                            SaveData();
                            handledMouse = true;
                        }
                        // Forced Break Toggle
                        else if (mx >= panelX + 450.0f && mx <= panelX + 570.0f && my >= panelY + 1270.0f && my <= panelY + 1330.0f) {
                            isForcedBreakMode_ = !isForcedBreakMode_;
                            SaveData();
                            handledMouse = true;
                        }
                        // Graphics Quality (Low: 0, Medium: 1, High: 2)
                        else if (my >= panelY + 1490.0f && my <= panelY + 1530.0f) {
                            if (mx >= panelX + 40.0f && mx <= panelX + 130.0f) {
                                graphicsQuality_ = 0;
                                boids_.resize(100);
                                SaveData();
                                handledMouse = true;
                            } else if (mx >= panelX + 150.0f && mx <= panelX + 240.0f) {
                                graphicsQuality_ = 1;
                                int oldSize = (int)boids_.size();
                                boids_.resize(200);
                                if (oldSize < 200) {
                                    for (int i = oldSize; i < 200; ++i) {
                                        boids_[i].position.x = swarmCenter_.x + ((rand() % 1600) - 800.0f);
                                        boids_[i].position.y = swarmCenter_.y + ((rand() % 800) - 400.0f);
                                        boids_[i].position.z = 0.0f;
                                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                                        boids_[i].velocity.x = cosf(angle) * boidSpeed_;
                                        boids_[i].velocity.y = sinf(angle) * boidSpeed_;
                                        boids_[i].velocity.z = 0.0f;
                                        boids_[i].color = { 0.2f + (rand()%30)/100.0f, 0.6f + (rand()%40)/100.0f, 0.9f + (rand()%10)/100.0f, 0.8f };
                                        boids_[i].scale = 2.0f + (rand() % 60) / 10.0f;
                                        // Globe coordinates
                                        float theta = (rand() % 360) * 3.14159f / 180.0f;
                                        float phi = acosf(2.0f * (rand() % 1000) / 1000.0f - 1.0f);
                                        boids_[i].baseGlobePos.x = sinf(phi) * cosf(theta);
                                        boids_[i].baseGlobePos.y = cosf(phi);
                                        boids_[i].baseGlobePos.z = sinf(phi) * sinf(theta);
                                    }
                                }
                                SaveData();
                                handledMouse = true;
                            } else if (mx >= panelX + 260.0f && mx <= panelX + 350.0f) {
                                graphicsQuality_ = 2;
                                int oldSize = (int)boids_.size();
                                boids_.resize(350);
                                if (oldSize < 350) {
                                    for (int i = oldSize; i < 350; ++i) {
                                        boids_[i].position.x = swarmCenter_.x + ((rand() % 1600) - 800.0f);
                                        boids_[i].position.y = swarmCenter_.y + ((rand() % 800) - 400.0f);
                                        boids_[i].position.z = 0.0f;
                                        float angle = (rand() % 360) * 3.14159f / 180.0f;
                                        boids_[i].velocity.x = cosf(angle) * boidSpeed_;
                                        boids_[i].velocity.y = sinf(angle) * boidSpeed_;
                                        boids_[i].velocity.z = 0.0f;
                                        boids_[i].color = { 0.2f + (rand()%30)/100.0f, 0.6f + (rand()%40)/100.0f, 0.9f + (rand()%10)/100.0f, 0.8f };
                                        boids_[i].scale = 2.0f + (rand() % 60) / 10.0f;
                                        // Globe coordinates
                                        float theta = (rand() % 360) * 3.14159f / 180.0f;
                                        float phi = acosf(2.0f * (rand() % 1000) / 1000.0f - 1.0f);
                                        boids_[i].baseGlobePos.x = sinf(phi) * cosf(theta);
                                        boids_[i].baseGlobePos.y = cosf(phi);
                                        boids_[i].baseGlobePos.z = sinf(phi) * sinf(theta);
                                    }
                                }
                                SaveData();
                                handledMouse = true;
                            }
                        }
                        // Reset Window Position
                        else if (mx >= panelX + 50.0f && mx <= panelX + 250.0f && my >= panelY + 1730.0f && my <= panelY + 1775.0f) {
                            HWND hwnd = dx_->GetHwnd();
                            MONITORINFO mi = { sizeof(mi) };
                            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
                            int monitorW = mi.rcMonitor.right - mi.rcMonitor.left;
                            int monitorH = mi.rcMonitor.bottom - mi.rcMonitor.top;
                            
                            int winW = isMascotMode_ ? 250 : monitorW;
                            int winH = isMascotMode_ ? 320 : monitorH;
                            int winX = isMascotMode_ ? (mi.rcMonitor.left + (monitorW - winW) / 2) : mi.rcMonitor.left;
                            int winY = isMascotMode_ ? (mi.rcMonitor.top + (monitorH - winH) / 2) : mi.rcMonitor.top;
                            
                            SetWindowPos(hwnd, isMascotMode_ ? HWND_TOPMOST : HWND_NOTOPMOST, winX, winY, winW, winH, SWP_SHOWWINDOW);
                            handledMouse = true;
                        }
                    }


                    
                    // パネル外クリックで閉じる (背景は画面全体の高さなので、X座標のみで判定)
                    if (!handledMouse && (mx < panelX || mx > panelX + panelW)) {
                        isSettingsOpen_ = false;
                        handledMouse = true;
                    }

                    // ×ボタンのクリック判定 (右上・固定配置・少し大きく)
                    float closeBtnW = 50.0f;
                    float closeBtnH = 50.0f;
                    float closeBtnX = panelX + panelW - 50.0f;
                    float closeBtnY = 20.0f;
                    if (!handledMouse && mx >= closeBtnX && mx <= closeBtnX + closeBtnW && my >= closeBtnY && my <= closeBtnY + closeBtnH) {
                        isSettingsOpen_ = false;
                        handledMouse = true;
                    }
                }

                // 設定ボタンのクリック判定 (開いているときは無効化)
                if (!isSettingsOpen_ && !zenMode_ && !handledMouse && mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH) {
                    isSettingsOpen_ = true;
                    inputBuffer_.clear();
                    handledMouse = true;
                }

                // インタラクションモード：Rippleのトリガー（UI以外をクリックした場合）
                if (!handledMouse && interactionMode_ == 2 && !isSettingsOpen_) {
                    rippleActive_ = true;
                    rippleRadius_ = 0.0f;
                    ripplePos_ = { mx, my, 0.0f };
                }

                // 古いPopcornのトリガー（全消去版）は削除し、外側のIsMouseDownで長押し処理として実装します
                // (IsMouseTriggerブロック内のここは空にします)
            } // end if (input->IsMouseTrigger(0))

            // インタラクションモード：Popcornのトリガー（長押しでポコポコ外れる＆右下の全消去UI）
            static int popcornCooldown_ = 0;
            if (popcornCooldown_ > 0) popcornCooldown_--;
            
            if (interactionMode_ == 3 && !isSettingsOpen_) {
                auto spawnHealingWord = [&]() {
                    if (visualMode_ == 0 || visualMode_ == 1) {
                        std::vector<Boid*> emptyBoids;
                        for (auto& eb : boids_) {
                            if (eb.customText.empty()) emptyBoids.push_back(&eb);
                        }
                        if (!emptyBoids.empty()) {
                            int idx = rand() % emptyBoids.size();
                            const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち", "深呼吸", "光", "星屑", "まどろみ"};
                            emptyBoids[idx]->customText = healingWords[rand() % 11];
                            emptyBoids[idx]->scale = 4.5f;
                            emptyBoids[idx]->textAlpha = 0.0f;
                            emptyBoids[idx]->color = {1.0f, 0.95f, 0.9f, 0.8f};
                        }
                    }
                };
                
                bool hoverAllPopUI = false;
                
                // 右下の All Popcorn ボタンの判定
                if (!isMascotMode_) {
                    float allPopX = Engine::WindowDX::kW - 180.0f;
                    float allPopY = Engine::WindowDX::kH - 60.0f;
                    float allPopW = 160.0f;
                    float allPopH = 40.0f;
                    if (mx >= allPopX && mx <= allPopX + allPopW && my >= allPopY && my <= allPopY + allPopH) {
                        hoverAllPopUI = true;
                        if (input->IsMouseTrigger(0)) {
                            for (auto& b : boids_) {
                                if (b.customText.empty()) continue;
                                size_t ptr = 0;
                                while (ptr < b.customText.length()) {
                                    size_t charLen = 1;
                                    unsigned char c = b.customText[ptr];
                                    if ((c & 0x80) == 0) charLen = 1;
                                    else if ((c & 0xE0) == 0xC0) charLen = 2;
                                    else if ((c & 0xF0) == 0xE0) charLen = 3;
                                    else if ((c & 0xF8) == 0xF0) charLen = 4;
                                    
                                    FallingChar fc;
                                    fc.character = b.customText.substr(ptr, charLen);
                                    ptr += charLen;
                                    
                                    Engine::Vector4 baseCol;
                                    if (useCustomTextColor_) {
                                        baseCol = {customTextColor_.x, customTextColor_.y, customTextColor_.z, 1.0f};
                                    } else {
                                        baseCol = (b.scale > 6.0f) ? Engine::Vector4{1.0f, 0.95f, 0.8f, 1.0f} : Engine::Vector4{0.9f, 0.9f, 1.0f, 0.8f};
                                    }
                                    
                                    if (visualMode_ == 2) {
                                        float appearX = b.position.x + ((rand() % 400) - 200.0f);
                                        if (appearX < 50.0f) appearX = 50.0f;
                                        if (appearX > Engine::WindowDX::kW - 50.0f) appearX = Engine::WindowDX::kW - 50.0f;
                                        fc.position = { appearX, 100.0f + (rand() % 150) };
                                        fc.velocity = { ((rand()%100)-50) * 1.5f, (rand()%50) * 1.0f };
                                        baseCol.w = 0.0f;
                                        fc.color = baseCol;
                                    } else {
                                        fc.position = { b.position.x + ((rand()%60)-30.0f), b.position.y + ((rand()%40)-20.0f) };
                                        fc.velocity = { ((rand()%100)-50) * 4.0f, -400.0f - (rand()%400) };
                                        fc.color = baseCol;
                                    }
                                    fc.angle = 0.0f;
                                    fc.angularVelocity = ((rand()%100)-50) * 0.05f;
                                    float textScale = (b.scale > 6.0f) ? 0.5f : 0.4f;
                                    fc.scale = textScale + 0.1f;
                                    fc.life = 0.0f;
                                    fallingChars_.push_back(fc);
                                }
                                b.customText.clear();
                                spawnHealingWord(); // 全て外れたので1つ湧かせる
                            }
                        }
                    }
                }
                
                // 長押しでの個別ポコポコ外れる処理
                if (!hoverAllPopUI && input->IsMouseDown(0) && popcornCooldown_ == 0) {
                    bool poppedSomething = false;
                    for (auto& b : boids_) {
                        if (b.customText.empty()) continue;
                        
                        float hitX = b.position.x;
                        float hitY = b.position.y;
                        float hitRadius = 100.0f;
                        
                        if (visualMode_ == 1) { // 天球儀モード
                            float cxRotG = cosf(globeRotX_), sxRotG = sinf(globeRotX_);
                            float cyRotG = cosf(globeRotY_), syRotG = sinf(globeRotY_);
                            float RG = std::min(Engine::WindowDX::kW, Engine::WindowDX::kH) * 0.45f;
                            float cxG = Engine::WindowDX::kW / 2.0f;
                            float cyG = Engine::WindowDX::kH / 2.0f;
                            float px1 = b.baseGlobePos.x * cyRotG + b.baseGlobePos.z * syRotG;
                            float pz1 = -b.baseGlobePos.x * syRotG + b.baseGlobePos.z * cyRotG;
                            float py2 = b.baseGlobePos.y * cxRotG - pz1 * sxRotG;
                            float pz2 = b.baseGlobePos.y * sxRotG + pz1 * cxRotG;
                            if (pz2 <= -0.4f) continue;
                            hitX = cxG + px1 * RG;
                            hitY = cyG - py2 * RG;
                            float scaleMod = 1.0f + pz2 * 0.4f;
                            hitRadius = 40.0f * scaleMod; 
                        }
                        
                        float dx = hitX - mx;
                        float dy = hitY - my;
                        if (dx*dx + dy*dy < hitRadius * hitRadius) {
                            size_t charLen = 1;
                            unsigned char c = b.customText[0];
                            if ((c & 0x80) == 0) charLen = 1;
                            else if ((c & 0xE0) == 0xC0) charLen = 2;
                            else if ((c & 0xF0) == 0xE0) charLen = 3;
                            else if ((c & 0xF8) == 0xF0) charLen = 4;
                            
                            FallingChar fc;
                            fc.character = b.customText.substr(0, charLen);
                            b.customText = b.customText.substr(charLen);
                            
                            Engine::Vector4 baseCol;
                            if (useCustomTextColor_) {
                                baseCol = {customTextColor_.x, customTextColor_.y, customTextColor_.z, 1.0f};
                            } else {
                                baseCol = (b.scale > 6.0f) ? Engine::Vector4{1.0f, 0.95f, 0.8f, 1.0f} : Engine::Vector4{0.9f, 0.9f, 1.0f, 0.8f};
                            }
                            
                            if (visualMode_ == 2) {
                                float appearX = b.position.x + ((rand() % 400) - 200.0f);
                                if (appearX < 50.0f) appearX = 50.0f;
                                if (appearX > Engine::WindowDX::kW - 50.0f) appearX = Engine::WindowDX::kW - 50.0f;
                                fc.position = { appearX, 100.0f + (rand() % 150) };
                                fc.velocity = { ((rand()%100)-50) * 1.5f, (rand()%50) * 1.0f };
                                baseCol.w = 0.0f;
                                fc.color = baseCol;
                            } else {
                                fc.position = { b.position.x + ((rand()%60)-30.0f), b.position.y + ((rand()%40)-20.0f) };
                                fc.velocity = { ((rand()%100)-50) * 4.0f, -400.0f - (rand()%400) };
                                fc.color = baseCol;
                            }
                            fc.angle = 0.0f;
                            fc.angularVelocity = ((rand()%100)-50) * 0.05f;
                            float textScale = (b.scale > 6.0f) ? 0.5f : 0.4f;
                            fc.scale = textScale + 0.1f;
                            fc.life = 0.0f;
                            fallingChars_.push_back(fc);
                            
                            if (b.customText.empty()) {
                                spawnHealingWord(); // この1文字でノードが空になったら新しく湧かせる
                            }
                            
                            poppedSomething = true;
                            break; // 1回の処理で1つのBoidから1文字だけポコッと外す
                        }
                    }
                    if (poppedSomething) {
                        popcornCooldown_ = 4; // 4フレーム（秒間約15発）のペースでポコポコ外れる
                    }
                }
            }

            if (!isSettingsOpen_) {
                // --- IME対応の日本語入力処理 ---
                // WindowDX.cpp で捕捉した WM_CHAR のバッファを追加
                // Zen Mode トグル (F11キー)
                if (input->Trigger(DIK_F11)) {
                    zenMode_ = !zenMode_;
                    if (zenMode_) isSettingsOpen_ = false;
                    if (Engine::Audio::GetInstance()) Engine::Audio::GetInstance()->Play(soundZen_, false, seVolume_);
                }

                // --- IME対応の日本語入力処理 ---
                // WindowDX.cpp で捕捉した WM_CHAR のバッファを追加
                bool isForcedBreak = (isForcedBreakMode_ && !isPomodoroWork_);
                if (!isForcedBreak) {
                    if (!g_CharInputBuffer.empty()) {
                        if (inputBuffer_.length() < 100) { // 入力制限を少し緩和
                            inputBuffer_ += g_CharInputBuffer;
                        }
                        g_CharInputBuffer.clear();
                    }

                    if (input->Trigger(DIK_BACK) && !inputBuffer_.empty()) {
                        // 簡易的なバックスペース処理 (UTF-8マルチバイト対応 of 末尾削除)
                        while (!inputBuffer_.empty()) {
                            unsigned char c = inputBuffer_.back();
                            inputBuffer_.pop_back();
                            if ((c & 0xC0) != 0x80) break; // UTF-8の先頭バイトなら終了
                        }
                    }
                    
                    // 星を落とす（文字列の送信）処理
                    // IMEが変換中（g_ImeCompositionStringが空でない）の時のEnterキー（変換確定）は無視する
                    if (input->Trigger(DIK_RETURN) && !inputBuffer_.empty() && g_ImeCompositionString.empty()) {
                        if (Engine::Audio::GetInstance()) Engine::Audio::GetInstance()->Play(soundSend_, false, seVolume_);
                        if (!boids_.empty()) {
                            int randIdx = rand() % boids_.size();
                            boids_[randIdx].customText = inputBuffer_;
                            boids_[randIdx].scale = 6.5f; 
                            boids_[randIdx].color = {1.0f, 0.8f, 0.6f, 1.0f}; 
                            
                            if (visualMode_ == 2) {
                                // Falling Blocks の時は、上空からランダムな方向に落とす
                                boids_[randIdx].position.x = 50.0f + (rand() % (Engine::WindowDX::kW - 100));
                                boids_[randIdx].position.y = -100.0f; // 画面外の少し上
                                boids_[randIdx].velocity.x = ((rand() % 100) - 50) * 10.0f;
                                boids_[randIdx].velocity.y = 0.0f;
                                boids_[randIdx].freeTimer = 0.5f;
                            }
                        }
                        
                        // --- 星からのささやき（キーワードマッチングによる疑似AI） ---
                        std::string t = inputBuffer_;
                        // 英語やローマ字の大文字小文字の揺れを吸収するため小文字化
                        std::string t_lower = t;
                        for (char& c : t_lower) {
                            if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
                        }

                        auto containsAny = [&](const std::vector<std::string>& words) {
                            for (const auto& w : words) {
                                if (t_lower.find(w) != std::string::npos) return true;
                            }
                            return false;
                        };

                        if (containsAny({"疲れ", "つかれ", "ツカレ", "tsukare", "tukare", "辛い", "つらい", "ツライ", "tsurai", "turai", "しんどい", "シンドイ", "shindoi", "sindoi", "限界", "げんかい", "genkai", "tired", "exhausted", "hard"})) {
                            currentAIWhisper_ = "『今日一日 本当によく頑張りましたね 今はただ 休んでください』";
                        } else if (containsAny({"ミス", "みす", "misu", "miss", "失敗", "しっぱい", "shippai", "sippai", "ダメ", "だめ", "dame", "fail"})) {
                            currentAIWhisper_ = "『その悔しさは あなたが真剣に向き合っている証拠です 大丈夫』";
                        } else if (containsAny({"悲し", "かなし", "kanashi", "kanasi", "不安", "ふあん", "フアン", "huan", "fuan", "怖い", "こわい", "kowai", "sad", "anxious", "scared", "fear"})) {
                            currentAIWhisper_ = "『無理にポジティブにならなくていいんです ここに感情を置いていってください』";
                        } else if (containsAny({"怒", "おこ", "いかり", "ikari", "oko", "イライラ", "いらいら", "iraira", "むかつく", "ムカツク", "mukatsuku", "mukakuku", "angry", "mad", "hate"})) {
                            currentAIWhisper_ = "『感情の波はいずれ凪になります 今はここで ゆっくりと深呼吸を』";
                        } else if (containsAny({"嬉し", "うれし", "ureshi", "uresi", "楽し", "たのし", "tanoshi", "tanosi", "最高", "さいこう", "saikou", "happy", "fun", "great", "glad", "joy"})) {
                            currentAIWhisper_ = "『あなたの温かい感情が この場所をさらに明るくしてくれました』";
                        } else if (containsAny({"眠い", "ねむい", "nemui", "寝たい", "ねたい", "netai", "sleepy", "sleep"})) {
                            currentAIWhisper_ = "『星たちの瞬きを見ながら ゆっくりとまぶたを閉じてみませんか？』";
                        } else {
                            // どれにも当てはまらない場合は名言からランダム
                            const char* whispers[] = {
                                "『ここは あなたの心を休める場所です』",
                                "『あなたが落とした言葉は やがて星になって輝きます』",
                                "『静かな時間が あなたの心に平穏をもたらしますように』",
                                "『焦る必要はありません 自分のペースで息をしてください』",
                                "『空を見上げる余裕が 少しだけ心を軽くしてくれます』"
                            };
                            currentAIWhisper_ = whispers[rand() % 5];
                        }
                        
                        // 入力した気持ちをログに保存（最大50件）
                        savedFeelings_.push_back(inputBuffer_);
                        if (savedFeelings_.size() > 50) {
                            savedFeelings_.erase(savedFeelings_.begin());
                        }
                        SaveData();

                        inputBuffer_.clear();
                        g_CharInputBuffer.clear(); // 念のためクリア
                    }
                } else {
                    inputBuffer_.clear();
                    g_CharInputBuffer.clear();
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
            
            // 地面付近から湧くのを防ぐため、空中のランダムな位置にワープさせる
            emptyBoids[idx]->position.x = 50.0f + (rand() % (Engine::WindowDX::kW - 100));
            emptyBoids[idx]->position.y = 100.0f + (rand() % 200); // 空中（Y:100〜300）
            emptyBoids[idx]->velocity.x = ((rand() % 100) - 50) * 1.0f;
            emptyBoids[idx]->velocity.y = ((rand() % 100) - 50) * 1.0f;
        }
    }

    bool isInteractingFrame = !isMascotMode_ && !isSettingsOpen_ && !(isForcedBreakMode_ && !isPomodoroWork_);

    // Gravityの爆発ロジックの更新
    if (isInteractingFrame && interactionMode_ == 1) {
        bool isMouseDown = input->IsMouseDown(0);
        if (prevGravityMouseDown_ && !isMouseDown) {
            // 左クリックを離した瞬間：爆発を発動
            gravityExplosionTimer_ = 0.5f; 
            gravityExplosionPos_ = { mx, my, 0.0f };
        }
        prevGravityMouseDown_ = isMouseDown;
    }
    if (gravityExplosionTimer_ > 0.0f) {
        gravityExplosionTimer_ -= dt_;
    }

    // Slingshotモードの更新
    if (isInteractingFrame && interactionMode_ == 4) {
        bool isMouseDown = input->IsMouseDown(0);
        if (isMouseDown && draggedBoidIndex_ == -1) {
            float minDistSq = 400.0f; // 半径20px以内
            int targetIdx = -1;
            for (size_t i = 0; i < boids_.size(); ++i) {
                float dx = boids_[i].position.x - mx;
                float dy = boids_[i].position.y - my;
                float distSq = dx*dx + dy*dy;
                float hitRadius = std::max(20.0f, boids_[i].scale * 2.0f);
                if (distSq < hitRadius * hitRadius && distSq < minDistSq) {
                    minDistSq = distSq;
                    targetIdx = (int)i;
                }
            }
            if (targetIdx != -1) {
                draggedBoidIndex_ = targetIdx;
                slingshotStartPos_ = { mx, my };
                slingshotCurrentPos_ = { mx, my };
            }
        } else if (isMouseDown && draggedBoidIndex_ != -1) {
            slingshotCurrentPos_ = { mx, my };
        } else if (!isMouseDown && draggedBoidIndex_ != -1) {
            // ドラッグ終了、発射！
            float sdx = slingshotStartPos_.x - slingshotCurrentPos_.x;
            float sdy = slingshotStartPos_.y - slingshotCurrentPos_.y;
            if (draggedBoidIndex_ >= 0 && draggedBoidIndex_ < (int)boids_.size()) {
                boids_[draggedBoidIndex_].velocity.x += sdx * 20.0f;
                boids_[draggedBoidIndex_].velocity.y += sdy * 20.0f;
                boids_[draggedBoidIndex_].freeTimer = 2.0f; // 2秒間は群れのルールを無視して吹っ飛ぶ
            }
            draggedBoidIndex_ = -1;
        }
    } else {
        draggedBoidIndex_ = -1;
    }

    // Boidsアルゴリズムの更新
    float cxRotG = 0, sxRotG = 0, cyRotG = 0, syRotG = 0, RG = 0, cxG = 0, cyG = 0;
    if (visualMode_ == 1) {
        cxRotG = cosf(globeRotX_); sxRotG = sinf(globeRotX_);
        cyRotG = cosf(globeRotY_); syRotG = sinf(globeRotY_);
        RG = std::min(Engine::WindowDX::kW, Engine::WindowDX::kH) * 0.45f;
        cxG = Engine::WindowDX::kW / 2.0f;
        cyG = Engine::WindowDX::kH / 2.0f;
    }
    
    // Blooming Tree 用のツリー目標座標の事前計算
    std::vector<Engine::Vector2> treePos(boids_.size());
    treeZoom_ = 1.0f;
    if (visualMode_ == 3 && !boids_.empty()) {
        treePos[0] = { Engine::WindowDX::kW / 2.0f, Engine::WindowDX::kH - 120.0f }; // 根元
        std::vector<float> treeAngle(boids_.size(), -1.570796f); // rootは真上(-90度)
        
        // ツリーのノード数に応じて根元の長さを飛躍的に伸ばし、全体を巨大化する
        float baseLength = 160.0f + (boids_.size() * 0.2f);
        float decay = 0.90f; // 枝の短縮率を緩やかにして外側まで長く伸ばす
        
        // 画面に収めるためのズーム率を計算（カメラを引く）
        // ※ 枝が斜めに広がるため、実際の高さは全てが一直線に上に伸びた場合の理論値より低くなります。
        // そのため 0.6 を掛けて過剰にズームアウトされすぎる（小さくなりすぎる）現象を修正します。
        float estimatedHeight = (baseLength / (1.0f - decay)) * 0.6f;
        float maxAllowedHeight = Engine::WindowDX::kH - 100.0f; // 画面の高さより少し小さめ
        treeZoom_ = maxAllowedHeight / estimatedHeight;
        if (treeZoom_ > 1.0f) treeZoom_ = 1.0f; // ズームイン（拡大）はしない
        
        baseLength *= treeZoom_; // 全体の長さをズーム率で縮小
        
        for (size_t i = 0; i < boids_.size(); ++i) {
            size_t leftChild = 2 * i + 1;
            size_t rightChild = 2 * i + 2;
            int d = 0; int t = (int)i; while(t>0){t=(t-1)/2; d++;}
            float length = baseLength * powf(decay, (float)d);
            
            if (leftChild < boids_.size()) {
                // 階層(d)が深くなるほど枝の開く角度を小さくし、枝が1周回って上や下へ暴走するバグを防ぐ
                float angleDelta = 0.65f * powf(0.85f, (float)d);
                treeAngle[leftChild] = treeAngle[i] - angleDelta;
                treePos[leftChild].x = treePos[i].x + cosf(treeAngle[leftChild]) * length;
                treePos[leftChild].y = treePos[i].y + sinf(treeAngle[leftChild]) * length;
            }
            if (rightChild < boids_.size()) {
                float angleDelta = 0.65f * powf(0.85f, (float)d);
                treeAngle[rightChild] = treeAngle[i] + angleDelta;
                treePos[rightChild].x = treePos[i].x + cosf(treeAngle[rightChild]) * length;
                treePos[rightChild].y = treePos[i].y + sinf(treeAngle[rightChild]) * length;
            }
        }
    }

    for (auto& b : boids_) {
        // freeTimerの減衰
        if (b.freeTimer > 0.0f) {
            b.freeTimer -= dt_;
        }

        // フェードイン処理
        if (!b.customText.empty() && b.textAlpha < 1.0f) {
            b.textAlpha += 0.5f * dt_; // 2秒かけてフワッと現れる
            if (b.textAlpha > 1.0f) b.textAlpha = 1.0f;
        }
        if (visualMode_ == 2) {
            // ブロックモードではノードの図形（星など）は表示せず文字だけにするためフェードアウト
            if (b.nodeAlpha > 0.0f) {
                b.nodeAlpha -= 1.0f * dt_; // 1秒でふわっと消える
                if (b.nodeAlpha < 0.0f) b.nodeAlpha = 0.0f;
            }
        } else {
            if (b.nodeAlpha < 1.0f) {
                b.nodeAlpha += 0.4f * dt_; // 2.5秒かけてゆっくり現れる
                if (b.nodeAlpha > 1.0f) b.nodeAlpha = 1.0f;
            }
        }

        Engine::Vector3 acceleration{0,0,0};

        if (visualMode_ == 0) {
            Engine::Vector3 vSep{0,0,0}, vAli{0,0,0}, vCoh{0,0,0};
        int neighborCount = 0;

        for (auto& other : boids_) {
            if (&b == &other) continue;
            float dx = b.position.x - other.position.x;
            float dy = b.position.y - other.position.y;
            float distSq = dx*dx + dy*dy;

            // --- ビリヤード判定 ---
            if (b.freeTimer > 0.0f && distSq < (b.scale + other.scale + 10.0f) * (b.scale + other.scale + 10.0f) && distSq > 0.001f) {
                float dist = sqrtf(distSq);
                float nx = dx / dist;
                float ny = dy / dist;
                float dot = b.velocity.x * -nx + b.velocity.y * -ny;
                // 衝突のエネルギー（速度）が一定以上の場合のみ弾き飛ばす。これにより微小な衝突による無限連鎖暴走を防ぐ。
                if (dot > 200.0f) {
                    // エネルギーを大きく減衰させて相手に伝える（0.8 -> 0.3）
                    other.velocity.x += -nx * dot * 0.3f;
                    other.velocity.y += -ny * dot * 0.3f;
                    other.freeTimer = 0.5f; // 弾かれた側の自由時間を短くして、すぐ定位置に戻るようにする
                    
                    // ぶつかった自分自身のエネルギーも大きく減らす（衝撃吸収）
                    b.velocity.x -= -nx * dot * 0.6f;
                    b.velocity.y -= -ny * dot * 0.6f;
                }
            }

            if (b.freeTimer > 0.0f) continue; // 自分が自由状態の時は引力を受けない

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
            
            // 左上のタイマーUI（"Focus Time"等）から反発させる
            // 矩形: X(0 〜 700), Y(0 〜 240)
            const float uiRectW = 700.0f;
            const float uiRectH = 240.0f;
            if (b.position.x < uiRectW && b.position.y < uiRectH) {
                // 右か下、どちらに逃げるのが早いか
                float escapeRight = uiRectW - b.position.x;
                float escapeDown  = uiRectH - b.position.y;
                
                if (escapeRight < escapeDown) {
                    acceleration.x += escapeRight * 0.8f;
                } else {
                    acceleration.y += escapeDown * 0.8f;
                }
            }
            
            // 右下のAIささやきUIからも反発させる
            // 矩形: X(screenW - 750 〜 screenW), Y(screenH - 240 〜 screenH)
            const float uiRightBoxW = 750.0f;
            const float uiRightBoxH = 240.0f;
            if (b.position.x > screenW - uiRightBoxW && b.position.y > screenH - uiRightBoxH) {
                float escapeLeft = b.position.x - (screenW - uiRightBoxW);
                float escapeUp   = b.position.y - (screenH - uiRightBoxH);
                
                if (escapeLeft < escapeUp) {
                    acceleration.x -= escapeLeft * 0.8f;
                } else {
                    acceleration.y -= escapeUp * 0.8f;
                }
            }
        } // end of isMascotMode_ else
        } // end of visualMode_ == 0
        else if (visualMode_ == 1) {
            // 天球儀モード: ベース座標の投影位置へバネのように引き寄せる
            Engine::Vector3 p = b.baseGlobePos;
            
            float px1 = p.x * cyRotG + p.z * syRotG;
            float pz1 = -p.x * syRotG + p.z * cyRotG;
            float py1 = p.y;
            
            float py2 = py1 * cxRotG - pz1 * sxRotG;
            float px2 = px1;
            
            float targetX = cxG + px2 * RG;
            float targetY = cyG - py2 * RG;
            
            float springForce = 15.0f;
            float friction = 0.8f;
            
            // --- インタラクションの影響を許可するための調整 ---
            // 自由状態（Slingshotやビリヤードで吹っ飛んでいる時）
            if (b.freeTimer > 0.0f) {
                springForce = 1.0f;  // バネを大幅に弱め、ゆっくり元の位置へ戻るようにする
                friction = 0.98f;    // 摩擦を通常のBoidsと同じにする
            } 
            // Slingshotで掴まれている星
            else if (interactionMode_ == 4 && draggedBoidIndex_ == (int)(&b - &boids_[0])) {
                springForce = 0.0f;  // バネ完全オフ（マウスの動きに追従させる）
                friction = 0.95f;
            }
            // Gravity中やRipple中
            else if ((interactionMode_ == 1 && input->IsMouseDown(0)) || 
                     (interactionMode_ == 1 && gravityExplosionTimer_ > 0.0f) || 
                     (interactionMode_ == 2 && rippleActive_)) {
                springForce = 1.5f;  // 引力や衝撃波に負けるようにバネを弱める
                friction = 0.96f;
            }

            // 目標座標への引力（バネ）
            acceleration.x += (targetX - b.position.x) * springForce;
            acceleration.y += (targetY - b.position.y) * springForce;
            
            // 摩擦（安定化のため強め）
            b.velocity.x *= friction;
            b.velocity.y *= friction;
        }
        else if (visualMode_ == 2) {
            // --- Falling Blocks モード ---
            
            // テキストの幅を大まかに計算するラムダ関数
            auto getApproxTextWidth = [](const std::string& text) -> float {
                float width = 0.0f;
                for (size_t i = 0; i < text.length(); ) {
                    unsigned char c = text[i];
                    if ((c & 0x80) == 0) { width += 1.0f; i += 1; }
                    else if ((c & 0xE0) == 0xC0) { width += 2.0f; i += 2; }
                    else if ((c & 0xF0) == 0xE0) { width += 2.0f; i += 3; }
                    else if ((c & 0xF8) == 0xF0) { width += 2.0f; i += 4; }
                    else { width += 1.0f; i += 1; }
                }
                return width;
            };

            if (b.customText.empty()) {
                // 文字を持たないノードは衝突せず、ゆっくり落ちてそのまま消える
                acceleration.y += 200.0f;
                b.velocity.x *= 0.98f;
                b.velocity.y *= 0.98f;
            } else {
                // 文字を持つノードのみブロックとして物理演算する
                acceleration.y += 800.0f; // 重力を少し弱める（反発力とバランスを取るため）
                
                for (auto& other : boids_) {
                    if (&b == &other) continue;
                    if (other.customText.empty()) continue; // 相手が文字を持たない場合は衝突しない
                
                // ブロックの当たり判定（文字の描画サイズに正確に合わせる）
                // ブロックモードでは文字のスケールを2倍（0.8）にするため、半角1文字の幅を約16.0pxとする
                float bTextWidth = getApproxTextWidth(b.customText) * 16.0f;
                float oTextWidth = getApproxTextWidth(other.customText) * 16.0f;
                
                // 中心間の最小距離。+2.0f はマージン
                float minDistX = (bTextWidth / 2.0f) + (oTextWidth / 2.0f) + 2.0f;
                float minDistY = 32.0f; // 2倍サイズのフォントの描画高さ分。
                
                float dx = b.position.x - other.position.x;
                float dy = b.position.y - other.position.y;
                float absDx = std::abs(dx);
                float absDy = std::abs(dy);
                
                // AABB衝突判定
                if (absDx < minDistX && absDy < minDistY && (absDx > 0.001f || absDy > 0.001f)) {
                    float overlapX = minDistX - absDx;
                    float overlapY = minDistY - absDy;
                    
                    // めり込みが浅い方の軸で押し出す
                    if (overlapY < overlapX) {
                        // Y軸方向の押し出し
                        if (dy < 0) {
                            // 自分が相手より上にいる -> 相手の上に乗る
                            b.position.y -= overlapY * 0.6f; // 振動を防ぐため0.6程度で補正
                            if (b.velocity.y > 0) b.velocity.y *= -0.05f; // バウンドを極力減らしてピタッと止める
                            b.velocity.x *= 0.1f; // 強力な摩擦で横滑りを防ぎ、高く積めるようにする
                            
                            // 中心から大きくズレている場合のみ横に滑らせて山を崩す
                            if (absDx > minDistX * 0.8f) {
                                acceleration.x += (dx > 0 ? 1.0f : -1.0f) * 40.0f;
                            }
                        } else {
                            // 自分が相手より下にいる
                            b.position.y += overlapY * 0.6f;
                            if (b.velocity.y < 0) b.velocity.y *= -0.1f;
                        }
                    } else {
                        // X軸方向の押し出し
                        if (dx < 0) {
                            b.position.x -= overlapX * 0.6f;
                            b.velocity.x *= 0.1f;
                        } else {
                            b.position.x += overlapX * 0.6f;
                            b.velocity.x *= 0.1f;
                        }
                    }
                }
            }
            b.velocity.x *= 0.98f;
            b.velocity.y *= 0.98f; // 空気抵抗
            
            // 床と壁（文字があるブロックのみ）
            float floorY = Engine::WindowDX::kH - 50.0f;
            float textHalfHeight = 16.0f; // 大きくした文字の高さ(32.0)の半分
            if (b.position.y > floorY - textHalfHeight) {
                b.position.y = floorY - textHalfHeight;
                if (b.velocity.y > 0) b.velocity.y *= -0.05f; // バウンドさせない
                b.velocity.x *= 0.1f; // 床の摩擦も強くして滑らないように
            }
            float marginX = 50.0f;
            float bHalfWidth = getApproxTextWidth(b.customText) * 8.0f; // 半分の幅
            if (b.position.x < marginX + bHalfWidth) {
                b.position.x = marginX + bHalfWidth;
                b.velocity.x *= -0.3f;
            } else if (b.position.x > Engine::WindowDX::kW - marginX - bHalfWidth) {
                b.position.x = Engine::WindowDX::kW - marginX - bHalfWidth;
                b.velocity.x *= -0.3f;
            }
            } // end of else (!b.customText.empty())
        }
        else if (visualMode_ == 3) {
            // --- Blooming Tree モード ---
            size_t bIdx = &b - &boids_[0];
            float targetX = treePos[bIdx].x;
            float targetY = treePos[bIdx].y;
            
            float springForce = 15.0f;
            float friction = 0.8f;
            
            if (b.freeTimer > 0.0f) { springForce = 1.0f; friction = 0.98f; }
            else if (interactionMode_ == 4 && draggedBoidIndex_ == (int)bIdx) { springForce = 0.0f; friction = 0.95f; }
            else if ((interactionMode_ == 1 && input->IsMouseDown(0)) || (interactionMode_ == 1 && gravityExplosionTimer_ > 0.0f) || (interactionMode_ == 2 && rippleActive_)) { springForce = 1.5f; friction = 0.96f; }

            acceleration.x += (targetX - b.position.x) * springForce;
            acceleration.y += (targetY - b.position.y) * springForce;
            b.velocity.x *= friction;
            b.velocity.y *= friction;
        }

        // --- インタラクションモードごとのマウス干渉処理 ---
        float mdx = b.position.x - mx;
        float mdy = b.position.y - my;
        float mDistSq = mdx*mdx + mdy*mdy;
        float mDist = sqrtf(mDistSq);

        // UI操作中（設定画面が開いている等）は一部のインタラクションを無効化する
        bool isInteracting = !isMascotMode_ && !isSettingsOpen_ && !(isForcedBreakMode_ && !isPomodoroWork_);

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
            // 爆発力
            if (gravityExplosionTimer_ > 0.0f) {
                float edx = b.position.x - gravityExplosionPos_.x;
                float edy = b.position.y - gravityExplosionPos_.y;
                float eDistSq = edx*edx + edy*edy;
                float eDist = sqrtf(eDistSq);
                float explodeRadius = 2500.0f;
                if (eDist > 0.001f && eDist < explodeRadius) {
                    float force = (explodeRadius - eDist) / explodeRadius;
                    float blastPower = 15000.0f * (gravityExplosionTimer_ / 0.5f); // 強力な反発力
                    acceleration.x += (edx / eDist) * force * blastPower;
                    acceleration.y += (edy / eDist) * force * blastPower;
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
        if (b.freeTimer <= 0.0f) {
            b.velocity.x += acceleration.x * dt_;
            b.velocity.y += acceleration.y * dt_;
        } else {
            b.velocity.x *= 0.98f;
            b.velocity.y *= 0.98f;
            
            if (isMascotMode_) {
                float currentRadius = 130.0f;
                float cx = swarmCenter_.x;
                float cy = swarmCenter_.y;
                float bdx = b.position.x - cx;
                float bdy = b.position.y - cy;
                float bdistSq = bdx*bdx + bdy*bdy;
                if (bdistSq > currentRadius * currentRadius && bdistSq > 0.001f) {
                    float bdist = sqrtf(bdistSq);
                    float nx = bdx / bdist;
                    float ny = bdy / bdist;
                    float dot = b.velocity.x * nx + b.velocity.y * ny;
                    if (dot > 0) {
                        b.velocity.x -= 2.0f * dot * nx;
                        b.velocity.y -= 2.0f * dot * ny;
                        b.position.x = cx + nx * currentRadius;
                        b.position.y = cy + ny * currentRadius;
                    }
                }
            } else {
                const float margin = 20.0f;
                const float screenW = (float)Engine::WindowDX::kW;
                const float screenH = (float)Engine::WindowDX::kH;
                if (b.position.x < margin) {
                    b.position.x = margin;
                    b.velocity.x *= -0.8f;
                } else if (b.position.x > screenW - margin) {
                    b.position.x = screenW - margin;
                    b.velocity.x *= -0.8f;
                }
                if (b.position.y < margin) {
                    b.position.y = margin;
                    b.velocity.y *= -0.8f;
                } else if (b.position.y > screenH - margin) {
                    b.position.y = screenH - margin;
                    b.velocity.y *= -0.8f;
                }
            }
        }

        float speedSq = b.velocity.x*b.velocity.x + b.velocity.y*b.velocity.y;
        float maxSpeed = (b.freeTimer > 0.0f) ? 3000.0f : (boidSpeed_ * 2.5f); // 自由状態は制限解除
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
        // アルファのフェードイン（パッと現れる）
        if (it->color.w < 1.0f) {
            it->color.w += 4.0f * dt_;
            if (it->color.w > 1.0f) it->color.w = 1.0f;
        }

        // scaleを利用して重力（落下速度）を調整する。文字数が多かった単語ほど重い。
        float gravityMulti = (it->scale / 0.4f); 
        it->velocity.y += (980.0f * gravityMulti) * dt_;
        
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
                float combinedRadius = nodeRadius + (charRadius * (it->scale / 0.4f));
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
                    b.velocity.x -= nx * 20.0f;
                    b.velocity.y -= ny * 20.0f;
                    b.scale = 6.0f; // ぶつかると一瞬大きくなる
                    if (Engine::Audio::GetInstance()) {
                        // ファイル自体で音量を調整済みのためseVolume_で再生
                        Engine::Audio::GetInstance()->Play(soundHit_, false, seVolume_);
                    }
                }
            }
        }
        
        // 落下文字同士の当たり判定（重なり解消）
        // ブロックモード（2）の時のみ文字同士を衝突させて積み上げる。
        // それ以外のモードでは文字同士の衝突をなくし、下で詰まらないようにする。
        if (visualMode_ == 2) {
            for (auto otherIt = fallingChars_.begin(); otherIt != fallingChars_.end(); ++otherIt) {
                if (it == otherIt) continue;
                float dx = it->position.x - otherIt->position.x;
                float dy = it->position.y - otherIt->position.y;
                float absDx = std::abs(dx);
                float absDy = std::abs(dy);
                
                // 隙間を狭く調整（文字の実測サイズに近づける）
                float minDistX = 13.0f * (it->scale / 0.4f);
                float minDistY = 15.0f * (it->scale / 0.4f);
                
                if (absDx < minDistX && absDy < minDistY && (absDx > 0.001f || absDy > 0.001f)) {
                    float overlapX = minDistX - absDx;
                    float overlapY = minDistY - absDy;
                    
                    if (overlapY < overlapX) {
                        if (dy < 0) {
                            it->position.y -= overlapY * 0.6f;
                            if (it->velocity.y > 0) it->velocity.y *= -0.1f; // バウンドを抑える
                            it->velocity.x *= 0.1f; // 強い摩擦で滑らないようにする
                            
                            // 山を作るための横滑り（力を弱めて高く積めるようにする）
                            if (absDx > minDistX * 0.5f) {
                                it->velocity.x += (dx > 0 ? 1.0f : -1.0f) * 2.0f;
                            }
                        } else {
                            it->position.y += overlapY * 0.6f;
                            if (it->velocity.y < 0) it->velocity.y *= -0.1f;
                        }
                    } else {
                        if (dx < 0) {
                            it->position.x -= overlapX * 0.6f;
                            it->velocity.x *= 0.1f;
                        } else {
                            it->position.x += overlapX * 0.6f;
                            it->velocity.x *= 0.1f;
                        }
                    }
                }
            }
        }
        
        // ブロックモード（visualMode == 2）の時だけ、床と壁があって積み重なる特殊なモード
        if (visualMode_ == 2) {
            float floorY = Engine::WindowDX::kH - 120.0f;
            if (it->position.y > floorY) {
                it->position.y = floorY;
                if (it->velocity.y > 0) it->velocity.y *= -0.2f;
                it->velocity.x *= 0.3f; // 床の摩擦を強くする
            }
            
            float marginX = 20.0f;
            if (it->position.x < marginX) {
                it->position.x = marginX;
                it->velocity.x *= -0.5f;
            } else if (it->position.x > Engine::WindowDX::kW - marginX) {
                it->position.x = Engine::WindowDX::kW - marginX;
                it->velocity.x *= -0.5f;
            }
        }
        
        // お片付け条件：画面外に出たらすぐ消す
        // 下で詰まらないように、左右にはみ出した場合も消去する
        if (it->position.y > Engine::WindowDX::kH + 20.0f || 
            it->position.x < -100.0f || 
            it->position.x > Engine::WindowDX::kW + 100.0f) {
            // 【復活・調整版】癒やしの言葉が湧く処理（クールダウン付きで適度に）
            // デフォルトと地球儀モードではポップコーンと連動するため、自動湧きは無効にする
            if (healingWordCooldown_ <= 0.0f && visualMode_ != 0 && visualMode_ != 1) {
                std::vector<Boid*> emptyBoids;
                for (auto& b : boids_) {
                    if (b.customText.empty()) {
                        emptyBoids.push_back(&b);
                    }
                }
                
                if (!emptyBoids.empty()) {
                    int idx = rand() % emptyBoids.size();
                    const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち", "深呼吸", "光", "星屑", "まどろみ"};
                    emptyBoids[idx]->customText = healingWords[rand() % 11]; // 落ちた破片をきっかけに、新しい単語が湧く
                    emptyBoids[idx]->scale = 4.5f;
                    emptyBoids[idx]->textAlpha = 0.0f; // フェードイン開始
                    emptyBoids[idx]->color = it->color;
                    
                    // クールダウンを設定（例: 8秒に1回だけ発生するようにする）
                    healingWordCooldown_ = 8.0f;
                }
            }
            
            // 【全モード共通】星からのエネルギーを蓄積
            stardustEnergy_ += 1.0f;
            
            // 5文字落ちる（エネルギーが5溜まる）ごとに、新しい空ノードを追加して群れを少しずつ大きくする
            if (stardustEnergy_ >= 5.0f) {
                stardustEnergy_ -= 5.0f; // ゲージを消費
                
                Boid newStar;
                // 下の落ちたところから現れる
                newStar.position = { it->position.x, Engine::WindowDX::kH + 20.0f, 0.0f };
                // 上に向かってフワッと昇る初速
                newStar.velocity = { ((rand()%100)-50) * 1.5f, -250.0f - (rand()%200) };
                newStar.color = it->color;
                newStar.scale = 4.5f;
                newStar.angle = 0.0f;
                newStar.textAlpha = 0.0f;
                newStar.nodeAlpha = 0.0f; // 移動中は透明にして線を引かせない
                
                float theta = (rand() % 360) * 3.14159f / 180.0f;
                float phi = acosf(2.0f * (rand() % 1000) / 1000.0f - 1.0f);
                newStar.baseGlobePos.x = sinf(phi) * cosf(theta);
                newStar.baseGlobePos.y = cosf(phi);
                newStar.baseGlobePos.z = sinf(phi) * sinf(theta);
                
                // 上限を超えない範囲で群れに追加（上限2000に変更して余裕を持たせる）
                if (boids_.size() < 2000) {
                    boids_.push_back(newStar);
                } else {
                    int replaceIdx = rand() % boids_.size();
                    boids_[replaceIdx] = newStar;
                }
            }
            
            it = fallingChars_.erase(it);
        } else {
            ++it;
        }
    }
    
    // 落下文字（ポップコーン）の表示上限を設定（例：2000文字まで）
    // これにより時間で消えなくなり、たくさん積み上げられるようになる
    const size_t maxFallingChars = 2000;
    if (fallingChars_.size() > maxFallingChars) {
        size_t overCount = fallingChars_.size() - maxFallingChars;
        fallingChars_.erase(fallingChars_.begin(), fallingChars_.begin() + overCount);
    }

    // 次のフレームの風力計算用にマウス座標を保存
    prevMx_ = mx;
    prevMy_ = my;

    // 音楽連携（SMTC）情報の更新
    if (smtcMonitor_) {
        SMTCInfo newInfo;
        if (smtcMonitor_->CheckUpdated(newInfo)) {
            bool thumbnailChanged = (currentMusicInfo_.thumbnailPath != newInfo.thumbnailPath) || (musicArtworkTex_ == 0);
            currentMusicInfo_ = newInfo;
            
            // アルバムアート画像を再ロード（パスが変更された時のみ実行してフリーズを防ぐ）
            if (currentMusicInfo_.hasThumbnail && renderer_ && thumbnailChanged) {
                musicArtworkTex_ = renderer_->LoadTexture2D(currentMusicInfo_.thumbnailPath, true);
            }
        }
    }

    // 音声キャプチャサンプルの取得
    if (audioCapture_) {
        audioCapture_->GetSamples(audioSamples_);
        
        // FFT処理によりスペクトラムデータを取得
        if (!audioSamples_.empty()) {
            size_t n = audioSamples_.size(); // 256
            std::vector<std::complex<float>> a(n);
            for (size_t i = 0; i < n; ++i) {
                // ハニング窓を適用してサイドローブ（周波数の漏れ）を抑制
                float window = 0.5f * (1.0f - cosf(2.0f * 3.1415926535f * i / (n - 1)));
                a[i] = std::complex<float>(audioSamples_[i] * window, 0.0f);
            }

            CooleyTukeyFFT(a);

            if (fftHeights_.size() != 64) {
                fftHeights_.assign(64, 0.0f);
            }

            // 自動ゲイン制御 (AGC) 用の最大振幅検出
            float maxAmp = 0.0f;
            for (int i = 0; i < 64; ++i) {
                float amp = (std::abs(a[i * 2]) + std::abs(a[i * 2 + 1])) / 2.0f;
                if (amp > maxAmp) maxAmp = amp;
            }
            
            // スムーズな最大振幅を保持して急激なゲイン変化を抑える
            static float s_smoothMaxAmp = 0.05f; // デフォルト値を小さくして立ち上がりを良くする
            if (maxAmp > s_smoothMaxAmp) {
                s_smoothMaxAmp += (maxAmp - s_smoothMaxAmp) * 12.0f * dt_; // 上昇はより早く追従
            } else {
                s_smoothMaxAmp += (maxAmp - s_smoothMaxAmp) * 0.8f * dt_;  // 下降は自然なフェードアウト
            }
            if (s_smoothMaxAmp < 0.0005f) s_smoothMaxAmp = 0.0005f; // 下限を少し下げて極小音もより拾うように

            for (int i = 0; i < 64; ++i) {
                float targetHeight = 0.0f;

                // --- リアルタイムキャプチャモード（本物の音声解析） ---
                // 2つのビンの平均振幅を取得
                float amp = (std::abs(a[i * 2]) + std::abs(a[i * 2 + 1])) / 2.0f;
                
                // 現在の最大音量を基準に正規化 (音量設定によらず常に同じ比率で揺れるようになる)
                float normalizedAmp = amp / s_smoothMaxAmp;
                
                // ダイナミックレンジをさらに圧縮（小さい音も拾いやすく、対数的な聴覚特性に近づける）
                float visualAmp = 0.0f;
                if (normalizedAmp > 0.0001f) {
                    visualAmp = sqrtf(normalizedAmp);
                    // 小さい音量をさらに持ち上げるための微調整 (4乗根を混ぜる)
                    visualAmp = visualAmp * 0.7f + powf(normalizedAmp, 0.25f) * 0.3f; 
                }
                
                // 人間の聴覚特性に合わせ、高域（右側）の感度を大きく上げる
                // 一般的に高音域はエネルギーが小さいため、スケールを補正する
                float freqScale = 1.0f + (i / 64.0f) * 2.5f;
                
                // ターゲット高さを計算（最大付近の音が60px前後になるようにスケール）
                targetHeight = visualAmp * 60.0f * freqScale;
                
                if (targetHeight > 80.0f) targetHeight = 80.0f; // 最大高さを80pxに制限

                // 滑らかに更新（アタックは非常に早く、リリースは適度に早く）
                if (targetHeight > fftHeights_[i]) {
                    fftHeights_[i] += (targetHeight - fftHeights_[i]) * 45.0f * dt_; // 急上昇（キレを良く）
                } else {
                    fftHeights_[i] += (targetHeight - fftHeights_[i]) * 20.0f * dt_; // やや早めに下降
                }
            }
        }
    }
    
    // 音楽情報が存在し、かつ再生中であればフェードイン。それ以外はゆっくりフェードアウト。
    if (!currentMusicInfo_.title.empty() && currentMusicInfo_.isPlaying) {
        musicNotificationAlpha_ += (1.0f - musicNotificationAlpha_) * 3.0f * dt_;
    } else {
        musicNotificationAlpha_ += (0.0f - musicNotificationAlpha_) * 1.5f * dt_;
        // 波形の平坦化処理（audioSamples_ と fftHeights_ を0に近づける処理）は
        // 実際の音声キャプチャ側で自然に行われるため、ここからは削除しました。
    }
    if (musicNotificationAlpha_ < 0.001f) {
        musicNotificationAlpha_ = 0.0f;
    }
} // ★ Update() の閉じカッコを追加

void MainScene::Draw() {
    if (!renderer_) return;
    
    // フェードアルファの計算
    float immersiveAlpha = 1.0f;
    float mascotAlpha = mascotFadeAlpha_; // Updateで計算された滑らかなフェードインを使用
    
    if (transitionTimer_ > 0) {
        float p = transitionTimer_ / 30.0f; // 1.0(開始) -> 0.0(終了)
        if (!nextMascotMode_) {
            // Mascot -> Immersive (拡大中): 
            // 拡大中はBoids(ノード)がじわじわ現れる
            immersiveAlpha = 1.0f - p;
        } else {
            // Immersive -> Mascot (縮小中):
            // 縮小中はBoids(ノード)がじわじわ消える
            immersiveAlpha = p;
        }
    } else {
        if (isMascotMode_) immersiveAlpha = 0.0f;
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
        if (visualMode_ == 0 || visualMode_ == 2 || visualMode_ == 3) { // --- 2Dベースモード ---
            // ノードの数が増えるほど接続距離を短くし、ごちゃごちゃした蜘蛛の巣になるのを防ぐ
            float densityFactor = sqrtf(300.0f / std::max(300.0f, (float)boids_.size()));
            const float connectDist = 130.0f * densityFactor;
            const float connectDistSq = connectDist * connectDist;
            const char* healingWords[] = {"安らぎ", "静寂", "癒やし", "ゆらゆら", "ぬくもり", "希望", "優しい気持ち"};
    
            for (size_t i = 0; i < boids_.size(); i++) {
            if (visualMode_ == 0) { // --- Constellation の星座線描画 ---
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
            } else if (visualMode_ == 3) { // --- Blooming Tree の枝描画 ---
                if (i > 0) {
                    size_t parentIdx = (i - 1) / 2;
                    if (parentIdx < boids_.size()) {
                        float dx = boids_[i].position.x - boids_[parentIdx].position.x;
                        float dy = boids_[i].position.y - boids_[parentIdx].position.y;
                        float dist = sqrtf(dx*dx + dy*dy);
                        
                        // どちらかのノードが完全に透明な場合は線を引かない（移動中の乱れ防止）
                        float lineAlphaBase = std::min(boids_[i].nodeAlpha, boids_[parentIdx].nodeAlpha);
                        
                        if (lineAlphaBase > 0.05f) {
                            Engine::Renderer::SpriteDesc line{};
                            line.x = (boids_[i].position.x + boids_[parentIdx].position.x) / 2.0f;
                            line.y = (boids_[i].position.y + boids_[parentIdx].position.y) / 2.0f;
                            line.w = dist;
                            line.h = 2.0f; // 枝は少し太め
                            line.rotationRad = atan2f(dy, dx);
                            line.x -= line.w / 2.0f;
                            line.y -= line.h / 2.0f;
                            // ノードの透明度に連動して線もフェードイン
                            line.color = {0.3f, 0.8f, 0.4f, 0.4f * immersiveAlpha * lineAlphaBase}; 
                            renderer_->DrawSprite(whiteTex_, line);
                        }
                    }
                }
            }

            Engine::Renderer::SpriteDesc bDesc{};
            // ツリーモードの場合はノードのサイズにもズームを適用して「カメラを引いた」感を出す
            float currentScale = boids_[i].scale * (visualMode_ == 3 ? treeZoom_ : 1.0f);
            bDesc.w = currentScale;
            bDesc.h = currentScale;
            bDesc.rotationRad = boids_[i].angle;
            
            if (interactionMode_ == 4 && draggedBoidIndex_ == (int)i) {
                // 強調表示：カラーパレットの反転色
                Engine::Vector4 themeCol = themes_[currentThemeIndex_].uiWork;
                bDesc.color = { 1.0f - themeCol.x, 1.0f - themeCol.y, 1.0f - themeCol.z, 1.0f * boids_[i].nodeAlpha };
                // 掴んでいる間は少し大きくする
                bDesc.w *= 1.5f;
                bDesc.h *= 1.5f;
            } else {
                if (useCustomNodeColor_) {
                    bDesc.color = { customNodeColor_.x, customNodeColor_.y, customNodeColor_.z, boids_[i].color.w };
                } else {
                    bDesc.color = boids_[i].color;
                }
                bDesc.color.w *= boids_[i].nodeAlpha; // ノード自体の透明度を適用
            }
            
            // 形状による描画の分岐
            if (currentShapeMode_ == 0) {
                // 四角 (従来通り)
                bDesc.x = boids_[i].position.x - bDesc.w/2.0f;
                bDesc.y = boids_[i].position.y - bDesc.h/2.0f;
                renderer_->DrawSprite(whiteTex_, bDesc);
            } else if (currentShapeMode_ == 1) {
                // 柔らかい光の円
                bDesc.w *= 1.5f; bDesc.h *= 1.5f;
                bDesc.x = boids_[i].position.x - bDesc.w/2.0f;
                bDesc.y = boids_[i].position.y - bDesc.h/2.0f;
                renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, bDesc);
            } else if (currentShapeMode_ == 2) {
                // ダイヤモンド (45度傾けて縦長に)
                bDesc.h *= 1.5f; // 縦長
                bDesc.rotationRad += 3.141592f / 4.0f; // +45度
                bDesc.x = boids_[i].position.x - bDesc.w/2.0f;
                bDesc.y = boids_[i].position.y - bDesc.h/2.0f;
                renderer_->DrawSprite(whiteTex_, bDesc);
            } else if (currentShapeMode_ == 3) {
                // キラキラ星 (十字架の光)
                Engine::Renderer::SpriteDesc s1 = bDesc;
                s1.w *= 0.3f; s1.h *= 1.8f;
                s1.x = boids_[i].position.x - s1.w/2.0f;
                s1.y = boids_[i].position.y - s1.h/2.0f;
                renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s1);
                
                Engine::Renderer::SpriteDesc s2 = bDesc;
                s2.w *= 1.8f; s2.h *= 0.3f;
                s2.x = boids_[i].position.x - s2.w/2.0f;
                s2.y = boids_[i].position.y - s2.h/2.0f;
                renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s2);
                
                Engine::Renderer::SpriteDesc s3 = bDesc;
                s3.w *= 0.5f; s3.h *= 0.5f;
                s3.x = boids_[i].position.x - s3.w/2.0f;
                s3.y = boids_[i].position.y - s3.h/2.0f;
                s3.color.w = std::min(1.0f, s3.color.w * 1.5f);
                renderer_->DrawSprite(whiteTex_, s3);
            }

            if (drawUI) {
                if (!boids_[i].customText.empty()) {
                    float alpha = boids_[i].textAlpha;
                    float currentZoom = (visualMode_ == 3) ? treeZoom_ : 1.0f;
                    
                    if (boids_[i].scale > 6.0f) {
                        // ユーザーが入力した気持ちを常時表示（目立たせるため少し大きく、色も明るく）
                        const char* word = boids_[i].customText.c_str();
                        float textScale = (visualMode_ == 2) ? 0.8f : 0.5f * currentZoom;
                        float wordW = renderer_->MeasureTextWidth(word, textScale, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                        Engine::Vector4 tColor = useCustomTextColor_ ? Engine::Vector4{customTextColor_.x, customTextColor_.y, customTextColor_.z, alpha} : Engine::Vector4{1.0f, 0.95f, 0.8f, alpha};
                        
                        // ブロックモード(2)の時は物理演算の中心と合わせるためオフセットを減らす
                        float yOffset = (visualMode_ == 2) ? 16.0f : 35.0f * currentZoom;
                        renderer_->DrawString(word, boids_[i].position.x - wordW / 2.0f, boids_[i].position.y - yOffset, textScale, tColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    } else {
                        // デフォルトの癒やしの言葉
                        const char* word = boids_[i].customText.c_str();
                        float textScale = (visualMode_ == 2) ? 0.8f : 0.4f * currentZoom;
                        float wordW = renderer_->MeasureTextWidth(word, textScale, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                        Engine::Vector4 tColor = useCustomTextColor_ ? Engine::Vector4{customTextColor_.x, customTextColor_.y, customTextColor_.z, 0.6f * alpha} : Engine::Vector4{0.9f, 0.9f, 1.0f, 0.6f * alpha};
                        
                        float yOffset = (visualMode_ == 2) ? 16.0f : 25.0f * currentZoom;
                        renderer_->DrawString(word, boids_[i].position.x - wordW / 2.0f, boids_[i].position.y - yOffset, textScale, tColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    }
                }
            }
            }
        } else if (visualMode_ == 1) { // --- Celestial Globe モード ---
            // 天球儀の半径
            float R = std::min(Engine::WindowDX::kW, Engine::WindowDX::kH) * 0.45f;
            float cx = Engine::WindowDX::kW / 2.0f;
            float cy = Engine::WindowDX::kH / 2.0f;
            
            // 天球儀全体の回転行列を計算
            float cxRot = cosf(globeRotX_), sxRot = sinf(globeRotX_);
            float cyRot = cosf(globeRotY_), syRot = sinf(globeRotY_);
            
            struct ProjectedBoid {
                float px, py, pz;    // 回転後の3D座標
                float sx, sy;        // スクリーン座標
                float scaleMod;
                float alphaMod;
            };
            std::vector<ProjectedBoid> proj(boids_.size());
            
            // 1. すべてのノードの投影座標を事前計算
            for (size_t i = 0; i < boids_.size(); i++) {
                Engine::Vector3 p = boids_[i].baseGlobePos;
                // Y軸回転
                float px1 = p.x * cyRot + p.z * syRot;
                float pz1 = -p.x * syRot + p.z * cyRot;
                float py1 = p.y;
                // X軸回転
                float py2 = py1 * cxRot - pz1 * sxRot;
                float pz2 = py1 * sxRot + pz1 * cxRot;
                float px2 = px1;
                
                proj[i].px = px2; proj[i].py = py2; proj[i].pz = pz2;
                proj[i].scaleMod = 1.0f + pz2 * 0.4f;
                proj[i].alphaMod = std::max(0.05f, 0.5f + pz2 * 0.5f);
                proj[i].sx = cx + px2 * R;
                proj[i].sy = cy - py2 * R;
            }
            
            // 2. 星座線（近傍接続）の描画
            // ノード数が増えるほど結ぶ距離を短くして密度を調整
            float densityFactor = sqrtf(300.0f / std::max(300.0f, (float)boids_.size()));
            const float connectDistGlobe = 0.4f * densityFactor;
            const float connectDistGlobeSq = connectDistGlobe * connectDistGlobe;
            for (size_t i = 0; i < boids_.size(); i++) {
                if (proj[i].pz < -0.2f) continue; // 奥にある星は線を結ばない
                
                int lineCount = 0;
                for (size_t j = i + 1; j < boids_.size(); j++) {
                    if (lineCount > 3) break;
                    if (proj[j].pz < -0.2f) continue;
                    
                    float dx = proj[i].px - proj[j].px;
                    float dy = proj[i].py - proj[j].py;
                    float dz = proj[i].pz - proj[j].pz;
                    float distSq = dx*dx + dy*dy + dz*dz;
                    
                    if (distSq < connectDistGlobeSq) {
                        float dist = sqrtf(distSq);
                        float lineAlpha = 1.0f - (dist / connectDistGlobe);
                        float avgAlphaMod = (proj[i].alphaMod + proj[j].alphaMod) * 0.5f;
                        
                        Engine::Renderer::SpriteDesc line{};
                        float sdx = proj[j].sx - proj[i].sx;
                        float sdy = proj[j].sy - proj[i].sy;
                        float sDist = sqrtf(sdx*sdx + sdy*sdy);
                        
                        line.x = (proj[i].sx + proj[j].sx) / 2.0f;
                        line.y = (proj[i].sy + proj[j].sy) / 2.0f;
                        line.w = sDist;
                        line.h = 1.0f;
                        line.rotationRad = atan2f(sdy, sdx);
                        line.x -= line.w / 2.0f;
                        line.y -= line.h / 2.0f;
                        line.color = {0.8f, 0.9f, 1.0f, lineAlpha * avgAlphaMod * 0.35f * immersiveAlpha};
                        renderer_->DrawSprite(whiteTex_, line);
                        lineCount++;
                    }
                }
            }
            
            // 3. ノードと文字の描画
            for (size_t i = 0; i < boids_.size(); i++) {
                Engine::Renderer::SpriteDesc bDesc{};
                bDesc.w = boids_[i].scale * proj[i].scaleMod;
                bDesc.h = boids_[i].scale * proj[i].scaleMod;
                bDesc.rotationRad = boids_[i].angle;
                
                bDesc.x = proj[i].sx - bDesc.w/2.0f;
                bDesc.y = proj[i].sy - bDesc.h/2.0f;
                
                if (useCustomNodeColor_) {
                    bDesc.color = { customNodeColor_.x, customNodeColor_.y, customNodeColor_.z, boids_[i].color.w };
                } else {
                    bDesc.color = boids_[i].color;
                }
                bDesc.color.w *= proj[i].alphaMod;
                
                // --- 文字の描画 ---
                if (!boids_[i].customText.empty() && proj[i].pz > -0.4f) { // 奥すぎない場合のみ描画
                    float baseScale = (boids_[i].scale > 6.0f) ? 0.5f : 0.4f;
                    if (visualMode_ == 2) baseScale = 0.8f; // ブロックモード時は文字を大きくする
                    float textScale = baseScale * proj[i].scaleMod;
                    float textAlpha = boids_[i].textAlpha * proj[i].alphaMod;
                    
                    if (textAlpha > 0.05f) {
                        float textW = renderer_->MeasureTextWidth(boids_[i].customText.c_str(), textScale, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                        float ty = proj[i].sy + bDesc.h / 2.0f + 8.0f; // ノードの少し下
                        Engine::Vector4 tColor = useCustomTextColor_ ? Engine::Vector4{customTextColor_.x, customTextColor_.y, customTextColor_.z, textAlpha} : Engine::Vector4{0.9f, 0.9f, 1.0f, textAlpha};
                        renderer_->DrawString(boids_[i].customText.c_str(), proj[i].sx - textW/2.0f, ty, textScale, tColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    }
                }
                
                // 形状による描画
                if (currentShapeMode_ == 0) {
                    renderer_->DrawSprite(whiteTex_, bDesc);
                } else if (currentShapeMode_ == 1) {
                    bDesc.w *= 1.5f; bDesc.h *= 1.5f;
                    bDesc.x = proj[i].sx - bDesc.w/2.0f;
                    bDesc.y = proj[i].sy - bDesc.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, bDesc);
                } else if (currentShapeMode_ == 2) {
                    bDesc.h *= 1.5f;
                    bDesc.rotationRad += 3.141592f / 4.0f;
                    bDesc.x = proj[i].sx - bDesc.w/2.0f;
                    bDesc.y = proj[i].sy - bDesc.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, bDesc);
                } else if (currentShapeMode_ == 3) {
                    Engine::Renderer::SpriteDesc s1 = bDesc;
                    s1.w *= 0.3f; s1.h *= 1.8f;
                    s1.x = proj[i].sx - s1.w/2.0f;
                    s1.y = proj[i].sy - s1.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s1);
                    Engine::Renderer::SpriteDesc s2 = bDesc;
                    s2.w *= 1.8f; s2.h *= 0.3f;
                    s2.x = proj[i].sx - s2.w/2.0f;
                    s2.y = proj[i].sy - s2.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s2);
                    Engine::Renderer::SpriteDesc s3 = bDesc;
                    s3.w *= 0.6f; s3.h *= 0.6f;
                    s3.x = proj[i].sx - s3.w/2.0f;
                    s3.y = proj[i].sy - s3.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, s3);
                }
            }
        }
        
        // --- 落下するパチンコ文字の描画 ---
        if (drawUI) {
            float currentZoom = (visualMode_ == 3) ? treeZoom_ : 1.0f;
            for (const auto& fc : fallingChars_) {
                renderer_->DrawString(fc.character.c_str(), fc.position.x, fc.position.y, fc.scale * currentZoom, fc.color, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
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
        // ゲージと上の文字が近すぎないよう、さらに下へ配置
        float cy = 148.0f;
        float radius = 80.0f;
            
        float currentTarget = isPomodoroWork_ ? (pWorkDurationMinutes_ * 60.0f) : (pRelaxDurationMinutes_ * 60.0f);
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
        // 数字が円の上下中央に配置されるようにオフセットを調整 (cy - 12.0f -> cy - 28.0f)
        renderer_->DrawString(timeText, cx - timeW / 2.0f, cy - 28.0f, 0.85f, timeCol, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        
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

        // --- UI（ImGuiを使わず独自のDrawStringで描画） ---
        if (drawUI && !zenMode_) {

            // 文字が大きくなった分、X座標を右揃えにして画面外にはみ出さないようにする
            float rightMarginX = Engine::WindowDX::kW - 50.0f;
            float bottomY = Engine::WindowDX::kH - 180.0f;
            
            float titleW = renderer_->MeasureTextWidth("星からのささやき：", 0.5f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("星からのささやき：", rightMarginX - titleW, bottomY, 0.5f, {0.7f, 0.7f, 0.8f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            float textW = renderer_->MeasureTextWidth(currentAIWhisper_.c_str(), 0.4f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(currentAIWhisper_.c_str(), rightMarginX - textW, bottomY + 45.0f, 0.4f, {0.7f, 0.7f, 0.8f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            float subW = renderer_->MeasureTextWidth("※右クリックで小さな姿に戻ります", 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("※右クリックで小さな姿に戻ります", rightMarginX - subW, bottomY + 95.0f, 0.35f, {0.6f, 0.6f, 0.7f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            // Popcorn All UI Button
            if (interactionMode_ == 3) {
                float allPopX = Engine::WindowDX::kW - 180.0f;
                float allPopY = Engine::WindowDX::kH - 60.0f;
                float allPopW = 160.0f;
                float allPopH = 40.0f;
                
                bool isHover = (mx >= allPopX && mx <= allPopX + allPopW && my >= allPopY && my <= allPopY + allPopH);
                
                Engine::Renderer::SpriteDesc allPopBgDesc{};
                allPopBgDesc.x = allPopX; allPopBgDesc.y = allPopY; allPopBgDesc.w = allPopW; allPopBgDesc.h = allPopH;
                allPopBgDesc.color = isHover ? Engine::Vector4{0.8f, 0.5f, 0.3f, 0.8f} : Engine::Vector4{0.6f, 0.3f, 0.2f, 0.6f};
                renderer_->DrawSprite(whiteTex_, allPopBgDesc);
                
                const char* btnText = "Popcorn ALL";
                float tw = renderer_->MeasureTextWidth(btnText, 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(btnText, allPopX + allPopW/2.0f - tw/2.0f, allPopY + 10.0f, 0.35f, {1.0f, 0.95f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
        }

        if (drawUI && !zenMode_) {
            // --- 左上の情報パネル（現在時刻＆ポモドーロタイマー） ---
            
            // 1. 文字列の準備
            std::time_t t = std::time(nullptr);
            std::tm now;
            localtime_s(&now, &t);
            char timeText[256];
            int hour = now.tm_hour;
            const char* ampm = (hour < 12) ? "AM" : "PM";
            int hour12 = (hour % 12 == 0) ? 12 : (hour % 12);
            snprintf(timeText, sizeof(timeText), "%02d:%02d %s", hour12, now.tm_min, ampm);
            
            float currentTarget = isPomodoroWork_ ? (pWorkDurationMinutes_ * 60.0f) : (pRelaxDurationMinutes_ * 60.0f);
            int remainingSec = (int)(currentTarget - pomodoroTimer_);
            int m = remainingSec / 60;
            int s = remainingSec % 60;
            char pomoText[256];
            if (isPomodoroWork_) {
                snprintf(pomoText, sizeof(pomoText), "Focus Time - %02d:%02d", m, s);
            } else {
                snprintf(pomoText, sizeof(pomoText), "Relax Time - %02d:%02d", m, s);
            }
            
            // 2. サイズ計測とレイアウト計算
            float pomoScale = 0.9f; // 大きさは前のまま
            float timeScale = 0.45f;
            float pomoW = renderer_->MeasureTextWidth(pomoText, pomoScale, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            float timeW = renderer_->MeasureTextWidth(timeText, timeScale, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            float panelX = 40.0f;
            float panelY = 40.0f;
            float paddingX = 25.0f;
            float paddingY = 20.0f;
            float panelW = std::max(pomoW, timeW) + paddingX * 2.0f;
            float panelH = 115.0f; // 両方を綺麗に囲む高さ
            
            // 3. 背景パネルの描画（黒いプレートにならないよう、完全なすりガラス風の透け感）
            // 半透明時の重なり（アルファの蓄積による黒ずみ）を完全に防ぐ、精密な角丸描画アルゴリズム
            auto drawCleanRoundedRect = [&](float bx, float by, float bw, float bh, float r, Engine::Vector4 c, int layer) {
                Engine::Renderer::SpriteDesc lr{};
                lr.color = c; lr.layer = layer;
                
                // 1. 中央の大きな矩形 (左右の角丸半径分を引いた幅、上下は全高)
                lr.x = bx + r; lr.y = by; lr.w = bw - r * 2.0f; lr.h = bh;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 2. 左側の矩形 (上下の角丸半径分を引いた高さ)
                lr.x = bx; lr.y = by + r; lr.w = r; lr.h = bh - r * 2.0f;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 3. 右側の矩形 (上下の角丸半径分を引いた高さ)
                lr.x = bx + bw - r; lr.y = by + r; lr.w = r; lr.h = bh - r * 2.0f;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 4. 四隅の滑らかな丸み（各象限のみを描画し、中央・左右の矩形と1ピクセルも重ねない）
                float r2 = r * r;
                auto drawQuadrant = [&](float cx, float cy, bool isLeft, bool isTop) {
                    float startY = isTop ? -r : 0.0f;
                    float endY   = isTop ? 0.0f : r;
                    for (float dy = startY; dy < endY; dy += 1.0f) {
                        float dx = sqrtf(std::max(0.0f, r2 - dy * dy));
                        Engine::Renderer::SpriteDesc sd{};
                        sd.w = dx; sd.h = 1.05f; // 重なりによるアルファ蓄積を防ぐため高さを1ピクセルに近づける
                        sd.x = isLeft ? (cx - dx) : cx; 
                        sd.y = cy + dy;
                        sd.color = c; sd.layer = layer;
                        renderer_->DrawSprite(whiteTex_, sd);
                    }
                };
                drawQuadrant(bx + r, by + r, true, true);         // 左上
                drawQuadrant(bx + bw - r, by + r, false, true);   // 右上
                drawQuadrant(bx + r, by + bh - r, true, false);   // 左下
                drawQuadrant(bx + bw - r, by + bh - r, false, false); // 右下
            };
            drawCleanRoundedRect(panelX, panelY, panelW, panelH, 15.0f, {1.0f, 1.0f, 1.0f, 0.08f}, 88);
            
            // 4. テキストの描画
            // 1段目: 現在時刻 (右寄せにしてレイアウトを引き締める)
            float timeX = panelX + panelW - paddingX - timeW;
            renderer_->DrawString(timeText, timeX, panelY + paddingY, timeScale, {0.65f, 0.7f, 0.8f, 0.9f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            // 2段目: メインのポモドーロタイマー (左寄せ)
            Engine::Vector4 pColor = currentColor_;
            renderer_->DrawString(pomoText, panelX + paddingX, panelY + paddingY + 35.0f, pomoScale, pColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
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

    if (!isMascotMode_ && !zenMode_) {
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
        // ギアアイコンを描画 (設定が開いている時はフェードアウト)
        float gearAlpha = 1.0f - settingsSlideOffset_;
        if (gearAlpha > 0.01f) {
            float gearSize = gearHover ? 36.0f : 32.0f;
            drawGear(btnX + btnW/2.0f, btnY + btnH/2.0f, gearSize, {0.9f, 0.9f, 0.9f, gearAlpha}, 90);
        }

        // テキストの描画
        if (isSettingsOpen_ || settingsSlideOffset_ > 0.01f) {
            // 設定メニューの背景パネル
            float panelW = 620.0f; 
            float targetPanelX = Engine::WindowDX::kW - panelW;
            float offscreenPanelX = Engine::WindowDX::kW;
            float panelX = offscreenPanelX + (targetPanelX - offscreenPanelX) * settingsSlideOffset_;
            float panelY = menuScrollY_ + 50.0f;

            Engine::Renderer::SpriteDesc sbg{};
            sbg.w = panelW; sbg.h = (float)Engine::WindowDX::kH;
            sbg.x = panelX; sbg.y = 0.0f; // 背景は常に画面全体
            sbg.color = {0.02f, 0.02f, 0.03f, 0.85f};
            sbg.layer = 100; // パネルの背景
            renderer_->DrawSprite(whiteTex_, sbg);

            // パネルの左端に細い光沢ライン (Glassmorphism highlight)
            Engine::Renderer::SpriteDesc sLine{};
            sLine.x = panelX; sLine.y = 0.0f; sLine.w = 1.0f; sLine.h = (float)Engine::WindowDX::kH;
            sLine.color = {1.0f, 1.0f, 1.0f, 0.1f}; sLine.layer = 101;
            renderer_->DrawSprite(whiteTex_, sLine);
            
            auto drawSectionHeader = [&](float x, float y, const char* title) {
                renderer_->DrawString(title, x, y, 0.45f, {0.85f, 0.85f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                Engine::Renderer::SpriteDesc sep{};
                sep.x = x; sep.y = y + 26.0f; sep.w = 480.0f; sep.h = 1.0f;
                sep.color = {1.0f, 1.0f, 1.0f, 0.08f}; sep.layer = 101;
                renderer_->DrawSprite(whiteTex_, sep);
            };

            drawSectionHeader(panelX + 20.0f, panelY + 20.0f, "Color Palette");
            renderer_->DrawString("Select your environment", panelX + 20.0f, panelY + 55.0f, 0.35f, {0.6f,0.6f,0.6f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            // ×ボタンの描画 (右上・固定配置・少し大きく)
            Engine::Renderer::SpriteDesc crossDesc{};
            float crossX = panelX + panelW - 25.0f;
            float crossY = 45.0f;
            bool crossHover = (mx >= crossX - 25.0f && mx <= crossX + 25.0f && my >= crossY - 25.0f && my <= crossY + 25.0f);
            crossDesc.color = crossHover ? Engine::Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Engine::Vector4{0.6f, 0.6f, 0.6f, 1.0f};
            crossDesc.layer = 105;
            
            // Line 1: /
            crossDesc.w = 32.0f; crossDesc.h = 5.0f; // 大きく、太く
            crossDesc.x = crossX - crossDesc.w/2.0f; crossDesc.y = crossY - crossDesc.h/2.0f;
            crossDesc.rotationRad = 3.14159f / 4.0f;
            renderer_->DrawSprite(whiteTex_, crossDesc);
            
            // Line 2: (backslash)
            crossDesc.rotationRad = -3.14159f / 4.0f;
            renderer_->DrawSprite(whiteTex_, crossDesc);

            // 角丸四角形を描画するラムダ
            auto drawRoundedRectUI = [&](float bx, float by, float bw, float bh, float r, Engine::Vector4 c, int layer) {
                Engine::Renderer::SpriteDesc lr{};
                lr.color = c; lr.layer = layer;
                
                // 1. 中央の縦長矩形 (上から下まで、左右の角丸幅を除く)
                lr.x = bx + r; lr.y = by; lr.w = bw - r * 2.0f; lr.h = bh;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 2. 左側の矩形 (角丸分を除いた高さ)
                lr.x = bx; lr.y = by + r; lr.w = r; lr.h = bh - r * 2.0f;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 3. 右側の矩形 (角丸分を除いた高さ)
                lr.x = bx + bw - r; lr.y = by + r; lr.w = r; lr.h = bh - r * 2.0f;
                renderer_->DrawSprite(whiteTex_, lr);
                
                // 4. 4つの角を1/4円スライスで描画 (重なり防止)
                auto drawCorner = [&](float cx, float cy, float radius, int cornerType) {
                    float r2 = radius * radius;
                    for (float dy = 0.0f; dy <= radius; dy += 1.0f) {
                        float dx = sqrtf(std::max(0.0f, r2 - dy * dy));
                        Engine::Renderer::SpriteDesc sd{};
                        sd.w = dx; sd.h = 1.5f;
                        sd.color = c; sd.layer = layer;
                        
                        if (cornerType == 0) { // 左上
                            sd.x = cx - dx; sd.y = cy - dy - sd.h/2.0f;
                        } else if (cornerType == 1) { // 右上
                            sd.x = cx; sd.y = cy - dy - sd.h/2.0f;
                        } else if (cornerType == 2) { // 左下
                            sd.x = cx - dx; sd.y = cy + dy - sd.h/2.0f;
                        } else if (cornerType == 3) { // 右下
                            sd.x = cx; sd.y = cy + dy - sd.h/2.0f;
                        }
                        renderer_->DrawSprite(whiteTex_, sd);
                    }
                };
                
                drawCorner(bx + r, by + r, r, 0); // 左上
                drawCorner(bx + bw - r, by + r, r, 1); // 右上
                drawCorner(bx + r, by + bh - r, r, 2); // 左下
                drawCorner(bx + bw - r, by + bh - r, r, 3); // 右下
            };

            // 4つのカラースウォッチを描画
            for (int i = 0; i < (int)themes_.size(); ++i) {
                float sx = panelX + 50.0f + i * 110.0f;
                float sy = panelY + 90.0f;
                
                // 選択中の場合は枠を少し大きめに描画
                if (i == currentThemeIndex_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // スウォッチ本体
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, themes_[i].uiWork, 102);
                
                // テーマ名
                float tw = renderer_->MeasureTextWidth(themes_[i].name.c_str(), 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(themes_[i].name.c_str(), sx + 35.0f - tw/2.0f, sy + 78.0f, 0.3f, {0.8f,0.8f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
            
            // --- Visual ModeのUI描画 ---
            drawSectionHeader(panelX + 20.0f, panelY + 210.0f, "Visual Mode");
            
            const char* vModeNames[] = { "Constellation", "CelestialGlobe", "FallingBlocks", "BloomingTree" };
            Engine::Vector4 vModeColors[] = {
                {0.2f, 0.5f, 0.8f, 1.0f}, // 星座（青）
                {0.5f, 0.3f, 0.8f, 1.0f}, // 天球儀（紫）
                {0.8f, 0.6f, 0.2f, 1.0f}, // 積み木（オレンジ/黄）
                {0.3f, 0.8f, 0.4f, 1.0f}  // 命の樹（緑）
            };

            for (int i = 0; i < 4; ++i) {
                float sx = panelX + 35.0f + i * 115.0f;
                float sy = panelY + 250.0f;
                
                // 選択中の場合は枠を描画
                if (i == visualMode_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // モードボタン本体
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, vModeColors[i], 102);
                
                // モード名
                float tw = renderer_->MeasureTextWidth(vModeNames[i], 0.25f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(vModeNames[i], sx + 35.0f - tw/2.0f, sy + 78.0f, 0.25f, {0.8f,0.8f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }

            // --- インタラクションモードのUI描画 ---
            drawSectionHeader(panelX + 20.0f, panelY + 370.0f, "Interaction Mode");
            
            const char* modeNames[] = { "Repel", "Gravity", "Ripple", "Popcorn", "Slingshot" };
            Engine::Vector4 modeColors[] = {
                {0.5f, 0.6f, 0.7f, 1.0f}, // Repel: 落ち着いたブルー系
                {0.8f, 0.3f, 0.5f, 1.0f}, // Gravity: 吸い込むような深い赤/紫系
                {0.3f, 0.8f, 0.8f, 1.0f}, // Ripple: 水の波紋のようなシアン系
                {1.0f, 0.8f, 0.3f, 1.0f}, // Popcorn: はじけるポップコーンのようなイエロー系
                {1.0f, 0.5f, 0.0f, 1.0f}  // Slingshot: モンスト風オレンジ系
            };

            for (int i = 0; i < 5; ++i) {
                float sx = panelX + 50.0f + (i % 4) * 110.0f;
                float sy = panelY + 410.0f + (i / 4) * 110.0f;
                
                // 天球儀モード中はポップコーン以外を暗くする
                Engine::Vector4 color = modeColors[i];
                float textAlpha = 1.0f;
                if (visualMode_ == 1 && i != 3) {
                    color = { 0.3f, 0.3f, 0.3f, 0.5f }; // 暗いグレー
                    textAlpha = 0.3f;
                }

                // 選択中の場合は枠を描画
                if (i == interactionMode_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // モードボタン本体
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, color, 102);
                
                // モードごとの簡易アイコン（図形）
                Engine::Vector4 iconCol = {1.0f, 1.0f, 1.0f, 0.9f};
                if (visualMode_ == 1 && i != 3) {
                    iconCol.w = 0.3f; // アイコンも半透明に
                }
                
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
                } else if (i == 4) {
                    // Slingshot: 弓や引っ張る矢印のような表現
                    Engine::Renderer::SpriteDesc sd{};
                    sd.color = iconCol; sd.layer = 103;
                    sd.w = 20.0f; sd.h = 4.0f;
                    sd.x = sx + 35.0f - sd.w/2.0f; sd.y = sy + 35.0f - sd.h/2.0f;
                    sd.rotationRad = -0.785398f;
                    renderer_->DrawSprite(whiteTex_, sd);
                    
                    Engine::Renderer::SpriteDesc sd2{};
                    sd2.color = iconCol; sd2.layer = 103;
                    sd2.w = 12.0f; sd2.h = 4.0f;
                    sd2.x = sx + 35.0f + 5.0f - sd2.w/2.0f; sd2.y = sy + 35.0f - 5.0f - sd2.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, sd2);
                }
                
                // モード名
                float tw = renderer_->MeasureTextWidth(modeNames[i], 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(modeNames[i], sx + 35.0f - tw/2.0f, sy + 78.0f, 0.3f, {0.8f,0.8f,0.8f,textAlpha}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }

            // --- Node ShapeのUI描画 ---
            drawSectionHeader(panelX + 20.0f, panelY + 640.0f, "Node Shape");
            
            const char* shapeNames[] = { "Square", "Circle", "Diamond", "Star" };
            
            for (int i = 0; i < 4; ++i) {
                float sx = panelX + 50.0f + i * 110.0f;
                float sy = panelY + 700.0f; // さらに下にずらす
                
                // 選択中の場合は枠を描画
                if (i == currentShapeMode_) {
                    drawRoundedRectUI(sx - 4.0f, sy - 4.0f, 78.0f, 78.0f, 12.0f, {1.0f, 1.0f, 1.0f, 0.8f}, 101);
                }

                // モードボタン本体（色はグレー系）
                drawRoundedRectUI(sx, sy, 70.0f, 70.0f, 10.0f, {0.3f, 0.3f, 0.35f, 1.0f}, 102);
                
                // アイコン描画
                Engine::Renderer::SpriteDesc bDesc{};
                bDesc.w = 20.0f; bDesc.h = 20.0f;
                bDesc.color = {1.0f, 1.0f, 1.0f, 0.9f};
                bDesc.layer = 103;
                bDesc.x = sx + 35.0f - bDesc.w/2.0f;
                bDesc.y = sy + 35.0f - bDesc.h/2.0f;
                
                if (i == 0) {
                    // Square
                    renderer_->DrawSprite(whiteTex_, bDesc);
                } else if (i == 1) {
                    // Circle
                    bDesc.w = 30.0f; bDesc.h = 30.0f;
                    bDesc.x = sx + 35.0f - bDesc.w/2.0f;
                    bDesc.y = sy + 35.0f - bDesc.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, bDesc);
                } else if (i == 2) {
                    // Diamond
                    bDesc.h = 30.0f;
                    bDesc.rotationRad = 3.141592f / 4.0f;
                    bDesc.x = sx + 35.0f - bDesc.w/2.0f;
                    bDesc.y = sy + 35.0f - bDesc.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, bDesc);
                } else if (i == 3) {
                    // Star
                    Engine::Renderer::SpriteDesc s1 = bDesc;
                    s1.w = 6.0f; s1.h = 36.0f;
                    s1.x = sx + 35.0f - s1.w/2.0f; s1.y = sy + 35.0f - s1.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s1);
                    
                    Engine::Renderer::SpriteDesc s2 = bDesc;
                    s2.w = 36.0f; s2.h = 6.0f;
                    s2.x = sx + 35.0f - s2.w/2.0f; s2.y = sy + 35.0f - s2.h/2.0f;
                    renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, s2);
                    
                    Engine::Renderer::SpriteDesc s3 = bDesc;
                    s3.w = 10.0f; s3.h = 10.0f;
                    s3.x = sx + 35.0f - s3.w/2.0f; s3.y = sy + 35.0f - s3.h/2.0f;
                    renderer_->DrawSprite(whiteTex_, s3);
                }
                
                float tw = renderer_->MeasureTextWidth(shapeNames[i], 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(shapeNames[i], sx + 35.0f - tw/2.0f, sy + 85.0f, 0.3f, {0.8f,0.8f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
            

            // --- Custom Color UI ---
            auto drawToggle = [&](float tx, float ty, const char* label, bool enabled, Engine::Vector4 color) {
                renderer_->DrawString(label, tx, ty + 12.0f, 0.35f, {0.9f, 0.9f, 0.95f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                float swX = tx + 160.0f;
                float swY = ty + 10.0f;
                float swW = 40.0f;
                float swH = 20.0f;
                bool isHover = (mx >= tx && mx <= tx + 200.0f && my >= ty && my <= ty + 40.0f);
                Engine::Vector4 trackColor = enabled ? color : (isHover ? Engine::Vector4{0.25f, 0.25f, 0.3f, 1.0f} : Engine::Vector4{0.15f, 0.15f, 0.18f, 1.0f});
                drawRoundedRectUI(swX, swY, swW, swH, 10.0f, trackColor, 102);
                
                Engine::Renderer::SpriteDesc handle{};
                handle.w = 14.0f; handle.h = 14.0f;
                handle.x = enabled ? (swX + swW - 17.0f) : (swX + 3.0f);
                handle.y = swY + 3.0f;
                handle.color = {1.0f, 1.0f, 1.0f, 1.0f}; handle.layer = 103;
                renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, handle);
            };
            
            auto drawCard = [&](float cx, float cy, float cw, float ch) {
                drawRoundedRectUI(cx, cy, cw, ch, 10.0f, {0.15f, 0.15f, 0.2f, 0.8f}, 101);
            };

            auto drawSlider = [&](float sx, float sy, float trackW, const char* label, float val, Engine::Vector4 col, const char* valStr = nullptr) {
                if (label && label[0] != '\0') {
                    float tw = renderer_->MeasureTextWidth(label, 0.25f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                    renderer_->DrawString(label, sx - tw - 15.0f, sy - 2.0f, 0.25f, {0.8f, 0.85f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                }
                
                float trackH = 10.0f;
                float trackY = sy + 1.0f;
                bool isHover = (mx >= sx - 10.0f && mx <= sx + trackW + 10.0f && my >= sy - 15.0f && my <= sy + 25.0f);
                
                drawRoundedRectUI(sx, trackY, trackW, trackH, 5.0f, {0.2f, 0.2f, 0.25f, 1.0f}, 102);
                if (val > 0.0f) {
                    drawRoundedRectUI(sx, trackY, val * trackW, trackH, 5.0f, col, 103);
                }
                
                Engine::Renderer::SpriteDesc handle{};
                float hSize = isHover ? 24.0f : 20.0f;
                handle.w = hSize; handle.h = hSize;
                handle.x = sx + val * trackW - hSize / 2.0f; 
                handle.y = trackY + trackH / 2.0f - hSize / 2.0f;
                handle.color = {1.0f, 1.0f, 1.0f, 1.0f}; handle.layer = 104;
                renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, handle);

                if (valStr) {
                    renderer_->DrawString(valStr, sx + trackW + 15.0f, sy - 2.0f, 0.25f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                }
            };

            // --- Custom Color UI ---
            drawSectionHeader(panelX + 20.0f, panelY + 840.0f, "COLOR CUSTOMIZATION");
            
            char rStr[16], gStr[16], bStr[16];
            
            // Node Color Card
            float ncX = panelX + 25.0f;
            float ncY = panelY + 890.0f;
            drawCard(ncX, ncY, 280.0f, 190.0f);
            renderer_->DrawString("NODE COLOR", ncX + 10.0f, ncY + 10.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            Engine::Renderer::SpriteDesc nPrev{};
            nPrev.x = ncX + 15.0f; nPrev.y = ncY + 50.0f; nPrev.w = 60.0f; nPrev.h = 60.0f;
            nPrev.color = useCustomNodeColor_ ? Engine::Vector4{customNodeColor_.x, customNodeColor_.y, customNodeColor_.z, 1.0f} : Engine::Vector4{0.3f, 0.3f, 0.3f, 1.0f};
            nPrev.layer = 102;
            renderer_->DrawSprite(whiteTex_, nPrev);
            renderer_->DrawString("ノードカラー", ncX + 10.0f, ncY + 140.0f, 0.3f, {0.6f, 0.6f, 0.6f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            if (useCustomNodeColor_) {
                snprintf(rStr, sizeof(rStr), "%d", (int)(customNodeColor_.x * 255));
                snprintf(gStr, sizeof(gStr), "%d", (int)(customNodeColor_.y * 255));
                snprintf(bStr, sizeof(bStr), "%d", (int)(customNodeColor_.z * 255));
                drawSlider(panelX + 140.0f, panelY + 940.0f, 110.0f, "R", customNodeColor_.x, {1.0f, 0.3f, 0.3f, 1.0f}, rStr);
                drawSlider(panelX + 140.0f, panelY + 980.0f, 110.0f, "G", customNodeColor_.y, {0.3f, 1.0f, 0.3f, 1.0f}, gStr);
                drawSlider(panelX + 140.0f, panelY + 1020.0f, 110.0f, "B", customNodeColor_.z, {0.3f, 0.3f, 1.0f, 1.0f}, bStr);
            } else {
                renderer_->DrawString("Enabled by Theme", panelX + 140.0f, panelY + 980.0f, 0.3f, {0.5f, 0.5f, 0.5f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }

            // Text Color Card
            float tcX = panelX + 315.0f;
            float tcY = panelY + 890.0f;
            drawCard(tcX, tcY, 280.0f, 190.0f);
            renderer_->DrawString("TEXT COLOR", tcX + 10.0f, tcY + 10.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            
            Engine::Renderer::SpriteDesc tPrev{};
            tPrev.x = tcX + 15.0f; tPrev.y = tcY + 50.0f; tPrev.w = 60.0f; tPrev.h = 60.0f;
            tPrev.color = useCustomTextColor_ ? Engine::Vector4{customTextColor_.x, customTextColor_.y, customTextColor_.z, 1.0f} : Engine::Vector4{0.3f, 0.3f, 0.3f, 1.0f};
            tPrev.layer = 102;
            renderer_->DrawSprite(whiteTex_, tPrev);
            renderer_->DrawString("テキストカラー", tcX + 10.0f, tcY + 140.0f, 0.3f, {0.6f, 0.6f, 0.6f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            if (useCustomTextColor_) {
                snprintf(rStr, sizeof(rStr), "%d", (int)(customTextColor_.x * 255));
                snprintf(gStr, sizeof(gStr), "%d", (int)(customTextColor_.y * 255));
                snprintf(bStr, sizeof(bStr), "%d", (int)(customTextColor_.z * 255));
                drawSlider(panelX + 440.0f, panelY + 940.0f, 110.0f, "R", customTextColor_.x, {1.0f, 0.3f, 0.3f, 1.0f}, rStr);
                drawSlider(panelX + 440.0f, panelY + 980.0f, 110.0f, "G", customTextColor_.y, {0.3f, 1.0f, 0.3f, 1.0f}, gStr);
                drawSlider(panelX + 440.0f, panelY + 1020.0f, 110.0f, "B", customTextColor_.z, {0.3f, 0.3f, 1.0f, 1.0f}, bStr);
            } else {
                renderer_->DrawString("Enabled by Theme", panelX + 440.0f, panelY + 980.0f, 0.3f, {0.5f, 0.5f, 0.5f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }

            // --- POMODORO CONFIGURATION ---
            drawSectionHeader(panelX + 20.0f, panelY + 1150.0f, "POMODORO CONFIGURATION");
            float pcY = panelY + 1200.0f;
            drawCard(panelX + 20.0f, pcY, 560.0f, 150.0f);
            
            float workProgress = (pWorkDurationMinutes_ - 10.0f) / 90.0f;
            renderer_->DrawString("Focus:", panelX + 40.0f, pcY + 15.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            char workValStr[16]; snprintf(workValStr, sizeof(workValStr), "%d min", (int)pWorkDurationMinutes_);
            renderer_->DrawString(workValStr, panelX + 110.0f, pcY + 15.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            drawSlider(panelX + 40.0f, panelY + 1260.0f, 220.0f, "", workProgress, themes_[currentThemeIndex_].uiWork);

            float relaxProgress = (pRelaxDurationMinutes_ - 1.0f) / 59.0f;
            renderer_->DrawString("Relax:", panelX + 320.0f, pcY + 15.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            char relaxValStr[16]; snprintf(relaxValStr, sizeof(relaxValStr), "%d min", (int)pRelaxDurationMinutes_);
            renderer_->DrawString(relaxValStr, panelX + 390.0f, pcY + 15.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            drawSlider(panelX + 320.0f, panelY + 1260.0f, 220.0f, "", relaxProgress, themes_[currentThemeIndex_].uiRelax);

            renderer_->DrawString("Forced Break", panelX + 40.0f, panelY + 1305.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString("ON", panelX + 450.0f, panelY + 1305.0f, 0.35f, {0.8f, 0.8f, 0.8f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            float swX = panelX + 490.0f;
            float swY = panelY + 1302.0f;
            float swW = 50.0f; float swH = 24.0f;
            drawRoundedRectUI(swX, swY, swW, swH, 12.0f, isForcedBreakMode_ ? Engine::Vector4{0.3f, 0.8f, 0.4f, 1.0f} : Engine::Vector4{0.3f, 0.3f, 0.35f, 1.0f}, 102);
            Engine::Renderer::SpriteDesc fbh{};
            fbh.w = 18.0f; fbh.h = 18.0f;
            fbh.x = isForcedBreakMode_ ? (swX + swW - 21.0f) : (swX + 3.0f);
            fbh.y = swY + 3.0f;
            fbh.color = {1.0f, 1.0f, 1.0f, 1.0f}; fbh.layer = 103;
            renderer_->DrawSprite(softCircleTex_ ? softCircleTex_ : whiteTex_, fbh);

            // --- 環境設定 (画質と音量) ---
            drawSectionHeader(panelX + 20.0f, panelY + 1390.0f, "GRAPHICS & SOUND SETTINGS");
            float gsY = panelY + 1440.0f;
            drawCard(panelX + 20.0f, gsY, 560.0f, 170.0f);

            // Graphics Quality
            renderer_->DrawString("Graphics Quality", panelX + 40.0f, gsY + 15.0f, 0.35f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            const char* qualityNames[] = { "Low", "Medium", "High" };
            for (int i = 0; i < 3; ++i) {
                float qx = panelX + 40.0f + i * 110.0f;
                float qy = gsY + 55.0f;
                bool isSel = (graphicsQuality_ == i);
                bool isHover = (mx >= qx && mx <= qx + 90.0f && my >= qy && my <= qy + 40.0f);
                Engine::Vector4 btnCol = isSel ? Engine::Vector4{0.2f, 0.5f, 0.9f, 1.0f} : (isHover ? Engine::Vector4{0.2f, 0.2f, 0.25f, 1.0f} : Engine::Vector4{0.15f, 0.15f, 0.18f, 1.0f});
                drawRoundedRectUI(qx, qy, 90.0f, 40.0f, 8.0f, btnCol, 102);
                float tw = renderer_->MeasureTextWidth(qualityNames[i], 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                renderer_->DrawString(qualityNames[i], qx + 45.0f - tw/2.0f, qy + 12.0f, 0.3f, isSel ? Engine::Vector4{1,1,1,1} : Engine::Vector4{0.7f,0.7f,0.75f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }

            // Audio Volume Sliders
            drawSlider(panelX + 130.0f, gsY + 130.0f, 410.0f, "SE Vol", seVolume_, {0.3f, 0.7f, 0.9f, 1.0f});

            // --- ディスプレイ設定 ---
            drawSectionHeader(panelX + 20.0f, panelY + 1680.0f, "Display settings");
            
            // Window Reset Button
            float rx = panelX + 50.0f;
            float ry = panelY + 1730.0f;
            float rw = 250.0f;
            float rh = 45.0f;
            bool rHover = (mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh);
            drawRoundedRectUI(rx, ry, rw, rh, 8.0f, rHover ? Engine::Vector4{0.4f, 0.4f, 0.45f, 1.0f} : Engine::Vector4{0.25f, 0.25f, 0.3f, 1.0f}, 102);
            const char* btnStr = "Reset Window Position";
            float rtw = renderer_->MeasureTextWidth(btnStr, 0.3f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(btnStr, rx + rw/2.0f - rtw/2.0f, ry + 12.0f, 0.3f, {1,1,1,0.9f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        }
        
        if (!isSettingsOpen_) {
            // 気持ち入力UIの描画（確定済みの文字＋IME変換中の文字）
            std::string displayStr = inputBuffer_ + g_ImeCompositionString;
            if (displayStr.empty()) {
                // プレースホルダー
                const char* hint = "Type your feelings and press Enter...";
                float hintW = renderer_->MeasureTextWidth(hint, 0.45f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                // 文字位置を少し下げる（cy - 25.0f から cy - 10.0f に変更）
                renderer_->DrawString(hint, cx - hintW/2.0f, cy - 10.0f, 0.45f, {0.6f, 0.6f, 0.6f, 0.8f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            } else {
                // カーソル点滅
                if ((transitionTimer_ / 30) % 2 == 0) {
                    displayStr += "_";
                }
                float textW = renderer_->MeasureTextWidth(displayStr.c_str(), 0.5f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
                // 文字位置を少し下げる（cy - 28.0f から cy - 13.0f に変更）
                renderer_->DrawString(displayStr.c_str(), cx - textW/2.0f, cy - 13.0f, 0.5f, {0.9f, 0.9f, 0.9f, 1.0f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            }
        }
    }

    // 音楽連携（SMTC）通知の描画
    if (!isMascotMode_ && musicNotificationAlpha_ > 0.0f) {
        float panelW = 380.0f;
        float panelH = 80.0f;
        float panelX = 40.0f; // 左下へ配置
        float panelY = Engine::WindowDX::kH - panelH - 40.0f;

        // 波形（スペクトラムアナライザ）の描画
        if (renderer_ && !fftHeights_.empty()) {
            int numPoints = 64;
            float startX = panelX + 5.0f;
            float baseY = panelY - 3.0f; // パネルの3px上を基準（ベースライン）にする
            float stepX = (panelW - 10.0f) / (float)numPoints;

            for (int i = 0; i < numPoints; ++i) {
                float height = fftHeights_[i];
                if (height < 1.5f) height = 1.5f; // 最低でも1.5pxの高さ（ドット）を表示して、波形ラインの存在感を出す

                Engine::Renderer::SpriteDesc barDesc{};
                barDesc.w = 3.0f; // バーの太さ
                barDesc.h = height;
                barDesc.x = startX + i * stepX - barDesc.w / 2.0f;
                barDesc.y = baseY - height; // 上方向に伸ばすため、ベースラインから高さを引く
                
                // 低音（左）から高音（右）へ、グラデーションカラー（淡いシアン〜淡い水色〜白）
                float tColor = (float)i / (float)numPoints;
                float r = 0.5f + tColor * 0.4f;
                float g = 0.8f + (1.0f - tColor) * 0.2f;
                float b = 1.0f;
                
                // 星屑のように優しく瞬く
                float glow = 0.7f + 0.3f * sinf(totalTime_ * 12.0f + i * 0.2f);
                barDesc.color = { r, g, b, musicNotificationAlpha_ * glow };
                barDesc.layer = 100;
                
                renderer_->DrawSprite(whiteTex_, barDesc);
            }
        }

        // 薄い半透明の座布団
        Engine::Renderer::SpriteDesc bg{};
        bg.w = panelW;
        bg.h = panelH;
        bg.x = panelX;
        bg.y = panelY;
        bg.color = { 0.08f, 0.1f, 0.15f, 0.5f * musicNotificationAlpha_ };
        bg.layer = 99;
        renderer_->DrawSprite(roundedRectTex_ ? roundedRectTex_ : whiteTex_, bg);

        float contentX = panelX + 15.0f;
        float contentY = panelY + 10.0f;

        if (currentMusicInfo_.hasThumbnail && musicArtworkTex_ != 0) {
            float imgW = 1.0f, imgH = 1.0f;
            renderer_->GetTextureSize(musicArtworkTex_, imgW, imgH);
            
            // アスペクト比を維持してセンタークロップ
            Engine::Renderer::SpriteDesc artworkDesc{};
            artworkDesc.x = contentX;
            artworkDesc.y = panelY + 8.0f;
            artworkDesc.w = 64.0f;
            artworkDesc.h = 64.0f;
            artworkDesc.color = { 1.0f, 1.0f, 1.0f, musicNotificationAlpha_ };
            artworkDesc.layer = 102; // テキストより前面
            
            float aspect = imgW / imgH;
            if (aspect > 1.0f) {
                // 横長画像：UVの左右をカット
                float cropW = 1.0f / aspect;
                artworkDesc.uvScaleOffset = { cropW, 1.0f, (1.0f - cropW) * 0.5f, 0.0f };
            } else {
                // 縦長画像：UVの上下をカット
                float cropH = aspect;
                artworkDesc.uvScaleOffset = { 1.0f, cropH, 0.0f, (1.0f - cropH) * 0.5f };
            }
            
            renderer_->DrawSprite(musicArtworkTex_, artworkDesc);
            
            // テキストの裏写り防止用マスク座布団をジャケット画像の裏（レイヤー101）に配置
            Engine::Renderer::SpriteDesc artworkMask{};
            artworkMask.x = contentX;
            artworkMask.y = panelY + 8.0f;
            artworkMask.w = 64.0f;
            artworkMask.h = 64.0f;
            artworkMask.color = { 0.08f, 0.1f, 0.15f, musicNotificationAlpha_ }; // 座布団と同色
            artworkMask.layer = 101;
            renderer_->DrawSprite(roundedRectTex_ ? roundedRectTex_ : whiteTex_, artworkMask);
            
            contentX += 80.0f;
        } else {
            contentX += 10.0f;
        }

        // 星のような淡い青白色
        Engine::Vector4 titleColor{ 0.9f, 0.95f, 1.0f, musicNotificationAlpha_ };
        Engine::Vector4 artistColor{ 0.7f, 0.75f, 0.85f, musicNotificationAlpha_ * 0.8f };

        // 往復スクロール（マーキー表示）の実装
        float maxTitleW = 250.0f;
        float titleW = renderer_->MeasureTextWidth(currentMusicInfo_.title.c_str(), 0.45f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        float titleScrollOffset = 0.0f;
        if (titleW > maxTitleW) {
            float scrollRange = titleW - maxTitleW;
            float t = fmodf(totalTime_, 6.0f); // 6秒周期
            float factor = 0.0f;
            if (t < 1.0f) {
                factor = 0.0f; // 左端で1秒停止
            } else if (t < 3.0f) {
                factor = (t - 1.0f) / 2.0f;
                factor = factor * factor * (3.0f - 2.0f * factor); // smoothstep
            } else if (t < 4.0f) {
                factor = 1.0f; // 右端で1秒停止
            } else {
                factor = 1.0f - (t - 4.0f) / 2.0f;
                factor = factor * factor * (3.0f - 2.0f * factor);
            }
            titleScrollOffset = factor * scrollRange;
        }

        float maxArtistW = 250.0f;
        float artistW = renderer_->MeasureTextWidth(currentMusicInfo_.artist.c_str(), 0.32f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        float artistScrollOffset = 0.0f;
        if (artistW > maxArtistW) {
            float scrollRange = artistW - maxArtistW;
            float t = fmodf(totalTime_ + 1.5f, 6.0f); // タイトルと1.5秒ずらしてスクロール開始
            float factor = 0.0f;
            if (t < 1.0f) {
                factor = 0.0f;
            } else if (t < 3.0f) {
                factor = (t - 1.0f) / 2.0f;
                factor = factor * factor * (3.0f - 2.0f * factor);
            } else if (t < 4.0f) {
                factor = 1.0f;
            } else {
                factor = 1.0f - (t - 4.0f) / 2.0f;
                factor = factor * factor * (3.0f - 2.0f * factor);
            }
            artistScrollOffset = factor * scrollRange;
        }

        // テキストを描画する前にここまでのUIとテキストをすべてフラッシュ（描画）しておく
        renderer_->FlushSprites();
        renderer_->FlushText();

        // テキストの描画領域（幅250pxのクリッピング領域）を設定
        renderer_->SetScissorRect(contentX, panelY, 250.0f, panelH);

        // スクロールオフセットを引いて描画（はみ出た部分はクリッピングにより切り捨てられる）
        renderer_->DrawString(currentMusicInfo_.title.c_str(), contentX - titleScrollOffset, contentY + 3.0f, 0.45f, titleColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        renderer_->DrawString(currentMusicInfo_.artist.c_str(), contentX - artistScrollOffset, contentY + 33.0f, 0.32f, artistColor, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

        // クリップが有効な状態でフラッシュして描画を確定
        renderer_->FlushText();

        // クリップを解除して元の状態に戻す
        renderer_->ResetScissorRect();
    }

    // 描画キューに溜まったUI用スプライトとテキストを正しい順序でフラッシュする
    if (renderer_) {
        renderer_->FlushSprites();
        renderer_->FlushText();
    }

    // --- バージョン表記の描画 ---
    if (!isMascotMode_) {
        float vw = renderer_->MeasureTextWidth(APP_VERSION, 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        renderer_->DrawString(APP_VERSION, Engine::WindowDX::kW - vw - 20.0f, Engine::WindowDX::kH - 30.0f, 0.35f, {0.6f, 0.6f, 0.7f, 0.8f}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        renderer_->FlushText();
    }

    // --- アップデート通知ダイアログ ---
    if (hasUpdateAvailable_ && !isMascotMode_) {
        float updW = 320.0f;
        float updH = 120.0f;
        float updX = Engine::WindowDX::kW - updW - 20.0f;
        float updY = Engine::WindowDX::kH - updH - 60.0f; // バージョン表記のさらに上
        
        Engine::Renderer::SpriteDesc uBg{};
        uBg.x = updX; uBg.y = updY; uBg.w = updW; uBg.h = updH;
        uBg.color = {0.1f, 0.12f, 0.15f, 0.95f};
        uBg.layer = 200;
        renderer_->DrawSprite(roundedRectTex_ ? roundedRectTex_ : whiteTex_, uBg);

        renderer_->DrawString("新しいバージョンがあります！", updX + 20.0f, updY + 15.0f, 0.4f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        std::string verMsg = std::string(APP_VERSION) + " -> " + latestVersion_;
        renderer_->DrawString(verMsg.c_str(), updX + 20.0f, updY + 45.0f, 0.3f, {0.7f,0.7f,0.7f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

        // 更新ボタン
        float btnW = 120.0f; float btnH = 35.0f;
        float btnX = updX + updW - btnW - 20.0f;
        float btnY = updY + updH - btnH - 15.0f;
        bool isHover = (mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH);
        
        Engine::Renderer::SpriteDesc bBg{};
        bBg.x = btnX; bBg.y = btnY; bBg.w = btnW; bBg.h = btnH;
        bBg.color = isHover ? Engine::Vector4{0.2f, 0.6f, 0.9f, 1.0f} : Engine::Vector4{0.15f, 0.4f, 0.7f, 1.0f};
        bBg.layer = 201;
        renderer_->DrawSprite(roundedRectTex_ ? roundedRectTex_ : whiteTex_, bBg);
        
        float tw = renderer_->MeasureTextWidth("更新する", 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        renderer_->DrawString("更新する", btnX + btnW/2.0f - tw/2.0f, btnY + 5.0f, 0.35f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

        if (isHover && input->IsMouseTrigger(0)) {
            // Updater.exe を起動して自身は終了
            // updateDownloadUrl_ を引数で渡すなど
            std::string cmdArgs = updateDownloadUrl_;
            ShellExecuteA(NULL, "open", "Updater.exe", cmdArgs.c_str(), NULL, SW_SHOW);
            PostMessage(dx_->GetHwnd(), WM_CLOSE, 0, 0); // 安全に終了
        }
        
        // 閉じるボタン（×）
        float closeX = updX + updW - 25.0f;
        float closeY = updY + 5.0f;
        bool cHover = (mx >= closeX && mx <= closeX + 20.0f && my >= closeY && my <= closeY + 20.0f);
        renderer_->DrawString("x", closeX, closeY, 0.4f, cHover ? Engine::Vector4{1,1,1,1} : Engine::Vector4{0.5f,0.5f,0.5f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        if (cHover && input->IsMouseTrigger(0)) {
            hasUpdateAvailable_ = false; // 非表示にする
        }
        
        renderer_->FlushSprites();
        renderer_->FlushText();
    }

    // --- 強制休憩モード用オーバーレイ描画 ---
    if (isForcedBreakMode_ && !isPomodoroWork_) {
        float currentTarget = pRelaxDurationMinutes_ * 60.0f;
        int remainingSec = (int)(currentTarget - pomodoroTimer_);
        if (remainingSec < 0) remainingSec = 0;
        int m = remainingSec / 60;
        int s = remainingSec % 60;
        char timerStr[64];
        snprintf(timerStr, sizeof(timerStr), "%02d:%02d", m, s);

        if (isMascotMode_) {
            // マスコットモード中の表示
            Engine::Renderer::SpriteDesc bg{};
            bg.w = 250.0f;
            bg.h = 320.0f;
            bg.x = 0.0f;
            bg.y = 0.0f;
            bg.color = {0.05f, 0.05f, 0.08f, 0.96f};
            bg.layer = 140;
            renderer_->DrawSprite(whiteTex_, bg);

            const char* title = "休憩時間です";
            float tw = renderer_->MeasureTextWidth(title, 0.5f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(title, 125.0f - tw/2.0f, 90.0f, 0.5f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            const char* sub = "少し目を休めましょう";
            float sw = renderer_->MeasureTextWidth(sub, 0.35f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(sub, 125.0f - sw/2.0f, 130.0f, 0.35f, {0.7f,0.7f,0.7f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            float timew = renderer_->MeasureTextWidth(timerStr, 0.9f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(timerStr, 125.0f - timew/2.0f, 180.0f, 0.9f, themes_[currentThemeIndex_].uiRelax, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        } else {
            // 没入モード中の表示
            Engine::Renderer::SpriteDesc bg{};
            bg.w = (float)Engine::WindowDX::kW;
            bg.h = (float)Engine::WindowDX::kH;
            bg.x = 0.0f;
            bg.y = 0.0f;
            bg.color = {0.03f, 0.04f, 0.06f, 0.96f};
            bg.layer = 140;
            renderer_->DrawSprite(whiteTex_, bg);

            const char* title = "強制休憩モード";
            float tw = renderer_->MeasureTextWidth(title, 1.2f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(title, Engine::WindowDX::kW/2.0f - tw/2.0f, Engine::WindowDX::kH/2.0f - 140.0f, 1.2f, {1,1,1,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            const char* sub = "作業時間が終了しました。画面から目を離し、リフレッシュしてください。";
            float sw = renderer_->MeasureTextWidth(sub, 0.45f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(sub, Engine::WindowDX::kW/2.0f - sw/2.0f, Engine::WindowDX::kH/2.0f - 70.0f, 0.45f, {0.7f,0.7f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            const char* sub2 = "ゆっくりと深呼吸をして、心と体を休めましょう。";
            float sw2 = renderer_->MeasureTextWidth(sub2, 0.45f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(sub2, Engine::WindowDX::kW/2.0f - sw2/2.0f, Engine::WindowDX::kH/2.0f - 30.0f, 0.45f, {0.7f,0.7f,0.8f,1}, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

            float timew = renderer_->MeasureTextWidth(timerStr, 2.0f, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
            renderer_->DrawString(timerStr, Engine::WindowDX::kW/2.0f - timew/2.0f, Engine::WindowDX::kH/2.0f + 60.0f, 2.0f, themes_[currentThemeIndex_].uiRelax, "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");
        }
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
std::string GetExeDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string strPath = path;
    size_t pos = strPath.find_last_of("\\/");
    if (pos != std::string::npos) {
        return strPath.substr(0, pos + 1);
    }
    return "";
}

void MainScene::LoadData() {
    std::string savePath = GetExeDirectory() + "tranquil_save.json";
    std::ifstream f(savePath);
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

        // 新しい設定項目
        if (j.contains("workDuration")) pWorkDurationMinutes_ = j["workDuration"].get<float>();
        if (j.contains("relaxDuration")) pRelaxDurationMinutes_ = j["relaxDuration"].get<float>();
        if (j.contains("forcedBreak")) isForcedBreakMode_ = j["forcedBreak"].get<bool>();
        
        if (j.contains("graphicsQuality")) {
            graphicsQuality_ = j["graphicsQuality"].get<int>();
            int targetCount = (graphicsQuality_ == 0) ? 100 : (graphicsQuality_ == 1 ? 200 : 350);
            boids_.resize(targetCount);
        }

        if (j.contains("bgmVolume")) {
            bgmVolume_ = j["bgmVolume"].get<float>();
            auto* audio = Engine::Audio::GetInstance();
            if (audio) audio->SetMasterBGMVolume(bgmVolume_);
        }
        if (j.contains("seVolume")) {
            seVolume_ = j["seVolume"].get<float>();
            auto* audio = Engine::Audio::GetInstance();
            if (audio) audio->SetMasterSEVolume(seVolume_);
        }

        if (j.contains("useCustomNodeColor")) useCustomNodeColor_ = j["useCustomNodeColor"].get<bool>();
        if (j.contains("customNodeColor")) {
            customNodeColor_.x = j["customNodeColor"][0];
            customNodeColor_.y = j["customNodeColor"][1];
            customNodeColor_.z = j["customNodeColor"][2];
        }
        if (j.contains("useCustomTextColor")) useCustomTextColor_ = j["useCustomTextColor"].get<bool>();
        if (j.contains("customTextColor")) {
            customTextColor_.x = j["customTextColor"][0];
            customTextColor_.y = j["customTextColor"][1];
            customTextColor_.z = j["customTextColor"][2];
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
        
        j["useCustomNodeColor"] = useCustomNodeColor_;
        j["customNodeColor"] = std::vector<float>{ customNodeColor_.x, customNodeColor_.y, customNodeColor_.z };
        j["useCustomTextColor"] = useCustomTextColor_;
        j["customTextColor"] = std::vector<float>{ customTextColor_.x, customTextColor_.y, customTextColor_.z };

        // 新しい設定項目
        j["workDuration"] = pWorkDurationMinutes_;
        j["relaxDuration"] = pRelaxDurationMinutes_;
        j["forcedBreak"] = isForcedBreakMode_;
        j["graphicsQuality"] = graphicsQuality_;
        j["bgmVolume"] = bgmVolume_;
        j["seVolume"] = seVolume_;

        std::string savePath = GetExeDirectory() + "tranquil_save.json";
        std::ofstream f(savePath);
        if (f.is_open()) {
            f << j.dump(4);
        }
    } catch (...) {
        // セーブ失敗時
    }
}

void MainScene::CheckForUpdateInfo() {
    if (isCheckingUpdate_) return;
    isCheckingUpdate_ = true;

    std::thread([this]() {
        HINTERNET hSession = WinHttpOpen(L"Sae Updater/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (hConnect) {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/repos/HakutoHenmi/Sae/releases/latest",
                                                        NULL, WINHTTP_NO_REFERER,
                                                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                        WINHTTP_FLAG_SECURE);
                if (hRequest) {
                    // GitHub API requires User-Agent
                    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                        if (WinHttpReceiveResponse(hRequest, NULL)) {
                            std::string responseData;
                            DWORD dwSize = 0;
                            DWORD dwDownloaded = 0;
                            do {
                                dwSize = 0;
                                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                                if (dwSize == 0) break;
                                char* pszOutBuffer = new char[dwSize + 1];
                                if (!pszOutBuffer) {
                                    dwSize = 0;
                                    break;
                                }
                                ZeroMemory(pszOutBuffer, dwSize + 1);
                                if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                                    responseData.append(pszOutBuffer, dwDownloaded);
                                }
                                delete[] pszOutBuffer;
                            } while (dwSize > 0);

                            try {
                                auto j = json::parse(responseData);
                                if (j.contains("tag_name")) {
                                    std::string tag = j["tag_name"].get<std::string>();
                                    // "v" や "Beta v" のフォーマット揺れを考慮する場合でも、完全一致しなければ更新とする
                                    if (tag != APP_VERSION) {
                                        latestVersion_ = tag;
                                        if (j.contains("assets") && j["assets"].is_array() && j["assets"].size() > 0) {
                                            updateDownloadUrl_ = j["assets"][0]["browser_download_url"].get<std::string>();
                                        }
                                        hasUpdateAvailable_ = true;
                                    }
                                }
                            } catch (...) {
                                // JSONパースエラー
                            }
                        }
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }
        isCheckingUpdate_ = false;
    }).detach();
}

} // namespace Game
