#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>

// ★追加: 時間計測用
#include <chrono>
#include <thread>

namespace Engine {

class WindowDX final {
public:
	static constexpr uint32_t kW = 1920;
	static constexpr uint32_t kH = 1080;
	static constexpr uint32_t kBackBufferCount = 2;

	// 現在のドロップ先ディレクトリを保持する静的変数
	static std::string s_DropDirectory;

public:
	bool Initialize(HINSTANCE hInst, int cmdShow, HWND& outHwnd);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	void WaitGPU(); // nullptr安全
	void WaitIdle() { WaitGPU(); }

	// ---- getters ----
	ID3D12Device* Dev() const { return dev_.Get(); }
	ID3D12GraphicsCommandList* List() const { return list_.Get(); }
	ID3D12CommandQueue* Queue() const { return que_.Get(); }

	ID3D12DescriptorHeap* SRV() const { return srvH_.Get(); }
	ID3D12DescriptorHeap* SRV_CPU_Heap() const { return srvH_CPU_.Get(); } // ★追加
	UINT SrvInc() const { return srvInc_; }
	UINT FrameIndex() const { return fi_; }

	ID3D12Resource* GetCurrentBackBufferResource() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;

	D3D12_CPU_DESCRIPTOR_HANDLE RTV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE DSV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE SRV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE SRV_CPU_Master(int offset) const; // ★追加
	D3D12_GPU_DESCRIPTOR_HANDLE SRV_GPU(int offset) const;

	HWND GetHwnd() const { return hwnd_; }
	void ToggleFullscreen();
	bool IsFullscreen() const { return isFullscreen_; }

private:
	bool InitWindow_(HINSTANCE hInst, int cmdShow, HWND& outHwnd);
	bool InitDX_();
	bool CreateSwapchain_();
	bool CreateRTVDSV_();
	bool CreateCommand_();
	bool CreateFence_();

private:
	HWND hwnd_ = nullptr;
	HINSTANCE hInst_ = nullptr;
	WNDCLASSEX wc_{};

	bool isFullscreen_ = false;
	RECT windowedRect_ = { 0, 0, (LONG)kW, (LONG)kH };
	LONG windowedStyle_ = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
	Microsoft::WRL::ComPtr<ID3D12Device> dev_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> que_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvH_CPU_; // ★追加: 非ShaderVisible

	Microsoft::WRL::ComPtr<ID3D12Resource> back_[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> depth_;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fenceVal_ = 0;
	HANDLE fev_ = nullptr;

	UINT rtvInc_ = 0;
	UINT dsvInc_ = 0;
	UINT srvInc_ = 0;

	UINT fi_ = 0;
	D3D12_VIEWPORT vp_{};
	D3D12_RECT sc_{};

	// ★追加: FPS制御用の変数
	std::chrono::steady_clock::time_point lastFrameTime_;
};

} // namespace Engine