#include "WindowDX.h"

#include <cassert>
#include <d3d12sdklayers.h>
#include <d3dx12.h>

#include <cctype> // tolower
#include <filesystem>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// ImGuiのメッセージハンドラ
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Engine {

// 静的変数の実体定義（初期値は "Resources"）
std::string WindowDX::s_DropDirectory = "Resources";

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wp, lp)) {
		return true;
	}

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// ファイルドロップの処理
	case WM_DROPFILES: {
		HDROP hDrop = (HDROP)wp;
		UINT count = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);

		for (UINT i = 0; i < count; i++) {
			char filePath[MAX_PATH];
			DragQueryFileA(hDrop, i, filePath, MAX_PATH);

			std::filesystem::path srcPath(filePath);
			std::string ext = srcPath.extension().string();

			// 小文字変換してチェック
			for (auto& c : ext) {
				c = static_cast<char>(tolower(c));
			}

			// 対応拡張子なら現在のディレクトリへコピー
			if (ext == ".obj" || ext == ".mtl" || ext == ".gltf" || ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga") {

				// WindowDX::s_DropDirectory をコピー先として使う
				std::filesystem::path destDir = WindowDX::s_DropDirectory;

				// フォルダがない場合は作成
				if (!std::filesystem::exists(destDir)) {
					std::filesystem::create_directories(destDir);
				}

				// コピー先パス: currentDir / filename
				std::filesystem::path destPath = destDir / srcPath.filename();

				try {
					// 上書きコピー
					std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing);
				} catch (...) {
					// エラーハンドリング
				}
			}
		}
		DragFinish(hDrop);
		return 0;
	}

	default:
		return DefWindowProc(hWnd, msg, wp, lp);
	}
}

bool WindowDX::Initialize(HINSTANCE hInst, int cmdShow, HWND& outHwnd) {
	hInst_ = hInst;

	if (!InitWindow_(hInst, cmdShow, outHwnd))
		return false;
	if (!InitDX_())
		return false;

	vp_.TopLeftX = 0;
	vp_.TopLeftY = 0;
	vp_.Width = (float)kW;
	vp_.Height = (float)kH;
	vp_.MinDepth = 0.0f;
	vp_.MaxDepth = 1.0f;

	sc_.left = 0;
	sc_.top = 0;
	sc_.right = (LONG)kW;
	sc_.bottom = (LONG)kH;

	// 初回時刻を記録
	lastFrameTime_ = std::chrono::steady_clock::now();

	return true;
}

void WindowDX::BeginFrame() {
	alloc_->Reset();
	list_->Reset(alloc_.Get(), nullptr);
}

void WindowDX::EndFrame() {
	auto res = back_[swap_->GetCurrentBackBufferIndex()].Get();
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(res, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	list_->ResourceBarrier(1, &barrier);

	HRESULT hr = list_->Close();
	(void)hr;
	assert(SUCCEEDED(hr));

	ID3D12CommandList* lists[] = {list_.Get()};
	que_->ExecuteCommandLists(1, lists);

	// ★修正ポイント1: VSyncをOFFにする (1,0 -> 0,0)
	// CPU側でWaitを行うため、GPU側(Present)での待機を無効化して競合を防ぎます。
	swap_->Present(0, 0);

	WaitGPU();

	fi_ = swap_->GetCurrentBackBufferIndex();

	// 60FPS固定ロジック (1,000,000us / 60fps = 16666.6us)
	constexpr long long kMinFrameTime = 1000000 / 60;

	// ★修正ポイント2: スリープを使わず、Busy Wait (空ループ) のみに変更
	// OSのスリープ精度によるカクつきを完全に排除します。
	while (true) {
		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameTime_).count();

		if (duration >= kMinFrameTime) {
			lastFrameTime_ = now;
			break;
		}

		// CPUリソースを過剰に占有しすぎないよう、ごく短いYieldを入れる（お好みで外してもOK）
		// std::this_thread::yield();
	}
}

void WindowDX::WaitGPU() {
	if (!que_ || !fence_)
		return;

	fenceVal_++;
	que_->Signal(fence_.Get(), fenceVal_);

	if (fence_->GetCompletedValue() < fenceVal_) {
		fence_->SetEventOnCompletion(fenceVal_, fev_);
		WaitForSingleObject(fev_, INFINITE);
	}
}

void WindowDX::ToggleFullscreen() {
	if (!hwnd_) return;

	isFullscreen_ = !isFullscreen_;

	if (isFullscreen_) {
		// ウィンドウ情報をバックアップ
		windowedStyle_ = GetWindowLong(hwnd_, GWL_STYLE);
		GetWindowRect(hwnd_, &windowedRect_);

		// ボーダレススタイルに変更
		SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_ & ~WS_OVERLAPPEDWINDOW);

		// モニター情報を取得してリサイズ
		MONITORINFO mi = { sizeof(mi) };
		if (GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowPos(hwnd_, HWND_TOP,
				mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		// 元のスタイルと座標に復元
		SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_);
		SetWindowPos(hwnd_, NULL,
			windowedRect_.left, windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}
}

void WindowDX::Shutdown() {
	WaitGPU();

	if (fev_) {
		CloseHandle(fev_);
		fev_ = nullptr;
	}

	depth_.Reset();
	for (auto& b : back_)
		b.Reset();

	srvH_.Reset();
	srvH_CPU_.Reset(); // ★追加
	dsvH_.Reset();
	rtvH_.Reset();

	list_.Reset();
	alloc_.Reset();
	que_.Reset();
	swap_.Reset();
	fence_.Reset();
	dev_.Reset();
	factory_.Reset();

	if (hwnd_) {
		UnregisterClass(wc_.lpszClassName, hInst_);
		hwnd_ = nullptr;
	}
	hInst_ = nullptr;
}

// ---- Private Init Helpers ----

bool WindowDX::InitWindow_(HINSTANCE hInst, int cmdShow, HWND& outHwnd) {
	wc_.cbSize = sizeof(WNDCLASSEX);
	wc_.lpfnWndProc = WndProc;
	wc_.lpszClassName = L"WindowDX_Class";
	wc_.hInstance = hInst;
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc_.style = CS_VREDRAW | CS_HREDRAW;

	RegisterClassEx(&wc_);

	DWORD style = WS_POPUP;
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST;

	RECT rc = {0, 0, (LONG)kW, (LONG)kH};
	AdjustWindowRectEx(&rc, style, FALSE, exStyle);

	// WS_POPUP では CW_USEDEFAULT が失敗する可能性があるため固定座標を指定
	hwnd_ = CreateWindowExW(exStyle, wc_.lpszClassName, L"心穏やかな場所", style, 100, 100, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

	if (!hwnd_)
		return false;

	// ウィンドウの中身（クライアント領域）が指定サイズとズレていないか確認し、ズレていれば補正する
	RECT rcClient;
	GetClientRect(hwnd_, &rcClient);
	int clientW = rcClient.right - rcClient.left;
	int clientH = rcClient.bottom - rcClient.top;

	if (clientW != kW || clientH != kH) {
		RECT rcWindow;
		GetWindowRect(hwnd_, &rcWindow);
		int windowW = rcWindow.right - rcWindow.left;
		int windowH = rcWindow.bottom - rcWindow.top;

		// 足りない分（または多い分）を計算
		int diffW = (int)kW - clientW;
		int diffH = (int)kH - clientH;

		// 正しいサイズにリサイズ
		SetWindowPos(hwnd_, 0, 0, 0, windowW + diffW, windowH + diffH, SWP_NOMOVE | SWP_NOZORDER);
	}

	DragAcceptFiles(hwnd_, TRUE);

	ShowWindow(hwnd_, cmdShow);
	UpdateWindow(hwnd_);

	outHwnd = hwnd_;
	return true;
}

bool WindowDX::InitDX_() {
#ifdef _DEBUG
	{
		Microsoft::WRL::ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
			debug->EnableDebugLayer();
		}
	}
#endif

	if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_))))
		return false;
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev_))))
		return false;

	if (!CreateCommand_())
		return false;
	if (!CreateSwapchain_())
		return false;
	if (!CreateRTVDSV_())
		return false;
	if (!CreateFence_())
		return false;

	return true;
}

bool WindowDX::CreateSwapchain_() {
	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Width = kW;
	sd.Height = kH;
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 背景透過のAlphaModeに対応するためB8G8R8A8を使用
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = kBackBufferCount;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // CreateSwapChainForHwndではUNSPECIFIEDである必要がある（DWMが自動で透過処理する）
	sd.Scaling = DXGI_SCALING_NONE; 

	Microsoft::WRL::ComPtr<IDXGISwapChain1> sc;
	if (FAILED(factory_->CreateSwapChainForHwnd(que_.Get(), hwnd_, &sd, nullptr, nullptr, &sc)))
		return false;
	sc.As(&swap_);
	fi_ = swap_->GetCurrentBackBufferIndex();
	return true;
}

bool WindowDX::CreateRTVDSV_() {
	{
		D3D12_DESCRIPTOR_HEAP_DESC d{};
		d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		d.NumDescriptors = kBackBufferCount + 64; // ★拡張: カスタムRTV用に余裕を持たせる
		if (FAILED(dev_->CreateDescriptorHeap(&d, IID_PPV_ARGS(&rtvH_))))
			return false;
		rtvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		auto h = rtvH_->GetCPUDescriptorHandleForHeapStart();
		for (UINT i = 0; i < kBackBufferCount; ++i) {
			if (FAILED(swap_->GetBuffer(i, IID_PPV_ARGS(&back_[i]))))
				return false;
			dev_->CreateRenderTargetView(back_[i].Get(), nullptr, h);
			h.ptr += rtvInc_;
		}
	}
	{
		D3D12_DESCRIPTOR_HEAP_DESC d{};
		d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		d.NumDescriptors = 1 + 64; // ★拡張: カスタムDSV用に余裕を持たせる
		if (FAILED(dev_->CreateDescriptorHeap(&d, IID_PPV_ARGS(&dsvH_))))
			return false;
		dsvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, kW, kH, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
		D3D12_CLEAR_VALUE c{};
		c.Format = DXGI_FORMAT_D32_FLOAT;
		c.DepthStencil.Depth = 1.0f;

		CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
		if (FAILED(dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &c, IID_PPV_ARGS(&depth_))))
			return false;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvd{};
		dsvd.Format = DXGI_FORMAT_D32_FLOAT;
		dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dev_->CreateDepthStencilView(depth_.Get(), &dsvd, dsvH_->GetCPUDescriptorHandleForHeapStart());
	}
	{
		D3D12_DESCRIPTOR_HEAP_DESC d{};
		d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		d.NumDescriptors = 2048;
		d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if (FAILED(dev_->CreateDescriptorHeap(&d, IID_PPV_ARGS(&srvH_))))
			return false;
		
		// ★追加: 非ShaderVisibleなマスターヒープを作成 (CopyDescriptorsのソース用)
		d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		if (FAILED(dev_->CreateDescriptorHeap(&d, IID_PPV_ARGS(&srvH_CPU_))))
			return false;

		srvInc_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	return true;
}

bool WindowDX::CreateCommand_() {
	if (FAILED(dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc_))))
		return false;
	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(dev_->CreateCommandQueue(&qd, IID_PPV_ARGS(&que_))))
		return false;
	if (FAILED(dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_.Get(), nullptr, IID_PPV_ARGS(&list_))))
		return false;
	list_->Close();
	return true;
}

bool WindowDX::CreateFence_() {
	if (FAILED(dev_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_))))
		return false;
	fev_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	return (fev_ != nullptr);
}

ID3D12Resource* WindowDX::GetCurrentBackBufferResource() const { return back_[swap_->GetCurrentBackBufferIndex()].Get(); }

D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::GetCurrentRTV() const { return RTV_CPU(swap_->GetCurrentBackBufferIndex()); }

D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::RTV_CPU(int offset) const {
	auto h = rtvH_->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)offset * rtvInc_;
	return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::DSV_CPU(int offset) const {
	auto h = dsvH_->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)offset * dsvInc_;
	return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::SRV_CPU(int offset) const {
	auto h = srvH_->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)offset * srvInc_;
	return h;
}
// ★追加: CPUマスターハンドル取得
D3D12_CPU_DESCRIPTOR_HANDLE WindowDX::SRV_CPU_Master(int offset) const {
	auto h = srvH_CPU_->GetCPUDescriptorHandleForHeapStart();
	h.ptr += (SIZE_T)offset * srvInc_;
	return h;
}
D3D12_GPU_DESCRIPTOR_HANDLE WindowDX::SRV_GPU(int offset) const {
	auto h = srvH_->GetGPUDescriptorHandleForHeapStart();
	h.ptr += (UINT64)offset * srvInc_;
	return h;
}

} // namespace Engine