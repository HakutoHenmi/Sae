#pragma once
// ===============================
//  ImGuiLayer : ImGui初期化/描画/エディターレイアウト
// ===============================
#include "GameObject.h"
#include "WindowDX.h"
#include "imgui.h"
#include <Windows.h>
#include <d3d12.h>
#include <string>
#include <vector>


namespace Engine {

class ImGuiLayer {
public:
	bool Initialize(
	    HWND hwnd, WindowDX& dx, ID3D12DescriptorHeap* srvHeap, D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle, float jpFontSize = 15.0f,
	    const char* jpFontPath = "Resources/Textures/fonts/Huninn/Huninn-Regular.ttf");

	void NewFrame(WindowDX& dx);

	void Render(WindowDX& dx);
	void Shutdown();

	ImFont* GetDefaultFont() const { return fontDefault_; }
	ImFont* GetJPFont() const { return fontJP_; }

private:
	ID3D12DescriptorHeap* srvHeap_ = nullptr;
	ImFont* fontDefault_ = nullptr;
	ImFont* fontJP_ = nullptr;

	int selectedObjectIndex_ = -1;
	std::string currentDir_ = "Resources";
};

} // namespace Engine
