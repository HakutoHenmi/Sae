#include "Input.h"
#include "WindowDX.h"
#include <cassert>
#include <algorithm>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace Engine {

Input* Input::instance_ = nullptr;

void Input::Initialize(HINSTANCE hInst, HWND hwnd) {
	instance_ = this;
	hwnd_ = hwnd;
	HRESULT hr;

	// --- DirectInput本体 ---
	hr = DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)di_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr) && "DirectInput8Create failed");

	// --- Keyboard ---
	hr = di_->CreateDevice(GUID_SysKeyboard, kb_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr) && "Keyboard CreateDevice failed");

	hr = kb_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr) && "Keyboard SetDataFormat failed");

	hr = kb_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr) && "Keyboard SetCooperativeLevel failed");

	kb_->Acquire();

	// --- Mouse ---
	hr = di_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr) && "Mouse CreateDevice failed");

	hr = mouse_->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(hr) && "Mouse SetDataFormat failed");

	hr = mouse_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr) && "Mouse SetCooperativeLevel failed");

	mouse_->Acquire();

	// 初期化
	ZeroMemory(keyState_, 256);
	ZeroMemory(prevKey_, 256);
	ZeroMemory(&mouseState_, sizeof(mouseState_));
	ZeroMemory(&prevMouseState_, sizeof(prevMouseState_));
	mouseX_ = mouseY_ = wheel_ = 0.0f;
	absMouseX_ = absMouseY_ = 0.0f;
}

void Input::Update() {
	// --- Keyboard更新 ---
	memcpy(prevKey_, keyState_, 256);
	if (FAILED(kb_->GetDeviceState(256, keyState_))) {
		kb_->Acquire();
		kb_->GetDeviceState(256, keyState_);
	}

	// --- Mouse更新 ---
	prevMouseState_ = mouseState_;
	if (FAILED(mouse_->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState_))) {
		mouse_->Acquire();
		mouse_->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState_);
	}

	// マウスの移動差分
	mouseX_ = static_cast<float>(mouseState_.lX);
	mouseY_ = static_cast<float>(mouseState_.lY);

	// ★絶対座標の更新（OSカーソルと同期し、内部解像度にスケーリング）
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(hwnd_, &pt);

	// クライアント領域のサイズを取得してスケーリング
	RECT rc;
	GetClientRect(hwnd_, &rc);
	float clientW = static_cast<float>(rc.right - rc.left);
	float clientH = static_cast<float>(rc.bottom - rc.top);

	if (clientW > 0 && clientH > 0) {
		absMouseX_ = static_cast<float>(pt.x) * (static_cast<float>(WindowDX::kW) / clientW);
		absMouseY_ = static_cast<float>(pt.y) * (static_cast<float>(WindowDX::kH) / clientH);
	} else {
		absMouseX_ = static_cast<float>(pt.x);
		absMouseY_ = static_cast<float>(pt.y);
	}

	// ホイール量（上:+、下:-）
	wheel_ = static_cast<float>(mouseState_.lZ) / WHEEL_DELTA;
}

void Input::Shutdown() {
	if (kb_)
		kb_->Unacquire();
	if (mouse_)
		mouse_->Unacquire();
	kb_.Reset();
	mouse_.Reset();
	di_.Reset();
	instance_ = nullptr;
}

} // namespace Engine
