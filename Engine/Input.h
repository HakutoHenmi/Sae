#pragma once
// ===============================
//  Input : DirectInput (Keyboard + Mouse)
// ===============================
#include <Windows.h>
#include <dinput.h>
#include <wrl.h>

namespace Engine {

class Input {
public:
	void Initialize(HINSTANCE hInst, HWND hwnd);
	void Update();
	void Shutdown();

	static Input* GetInstance() { return instance_; }

	// ===== Keyboard =====
	bool Down(BYTE k) const { return (keyState_[k] & 0x80) != 0; }
	bool Trigger(BYTE k) const { return Down(k) && !(prevKey_[k] & 0x80); }

	// ===== Mouse =====
	float GetMouseDeltaX() const { return mouseX_; }
	float GetMouseDeltaY() const { return mouseY_; }
	float GetMouseWheelDelta() const { return wheel_; }

	// ★追加: 絶対座標とボタン状態
	void GetMousePos(float& x, float& y) const { x = absMouseX_; y = absMouseY_; }
	bool IsMouseDown(int button) const { return (mouseState_.rgbButtons[button] & 0x80) != 0; }
	bool IsMouseTrigger(int button) const { return IsMouseDown(button) && !(prevMouseState_.rgbButtons[button] & 0x80); }

private:
	// --- DirectInput Core ---
	Microsoft::WRL::ComPtr<IDirectInput8> di_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> kb_;
	Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;

	// --- Keyboard ---
	BYTE keyState_[256]{};
	BYTE prevKey_[256]{};

	// --- Mouse ---
	DIMOUSESTATE2 mouseState_{};
	DIMOUSESTATE2 prevMouseState_{};

	float mouseX_ = 0.0f;
	float mouseY_ = 0.0f;
	float absMouseX_ = 0.0f; // ★追加
	float absMouseY_ = 0.0f; // ★追加
	float wheel_ = 0.0f;
	HWND hwnd_ = nullptr; // ★追加

	static Input* instance_;
};

} // namespace Engine
