// Engine/ImGuiLayer.cpp
#include "ImGuiLayer.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#include <cassert>
#include <filesystem>
namespace fs = std::filesystem;

namespace Engine {

bool ImGuiLayer::Initialize(
    HWND hwnd, WindowDX& dx, ID3D12DescriptorHeap* srvHeap, D3D12_CPU_DESCRIPTOR_HANDLE fontCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE fontGpuHandle, float jpFontSize, const char* jpFontPath) {

	(void)dx;
	(void)jpFontSize;
	(void)jpFontPath;

	if (fontGpuHandle.ptr == 0) {
		assert(false && "ImGui Font GPU Handle is INVALID.");
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.8f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

	srvHeap_ = srvHeap;
	
	// Load Japanese Font
	ImFontConfig config;
	config.MergeMode = false;
	if (jpFontPath && fs::exists(jpFontPath)) {
		io.Fonts->AddFontFromFileTTF(jpFontPath, jpFontSize, &config, io.Fonts->GetGlyphRangesJapanese());
	} else {
		// Fallback to embedded font if file missing
		io.Fonts->AddFontDefault();
	}

	if (!ImGui_ImplWin32_Init(hwnd))
		return false;
	if (!ImGui_ImplDX12_Init(dx.Dev(), WindowDX::kBackBufferCount, DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap_, fontCpuHandle, fontGpuHandle))
		return false;

	{
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	}

	if (!ImGui_ImplDX12_CreateDeviceObjects())
		return false;

	return true;
}

void ImGuiLayer::NewFrame(WindowDX& dx) {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	RECT rect;
	GetClientRect(dx.GetHwnd(), &rect);
	float clientW = (float)(rect.right - rect.left);
	float clientH = (float)(rect.bottom - rect.top);

	if (clientW > 0 && clientH > 0) {
		float scaleX = (float)WindowDX::kW / clientW;
		float scaleY = (float)WindowDX::kH / clientH;
		io.DisplayFramebufferScale = ImVec2(scaleX, scaleY);
	}

	ImGui::NewFrame();
}

void ImGuiLayer::Render(WindowDX& dx) {
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx.List());
}

void ImGuiLayer::Shutdown() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

} // namespace Engine