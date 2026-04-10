#pragma once

#include "../Libs/Includes.h"

class Gui
{
public:
	Present oPresent = nullptr;
	HWND window = nullptr;
	WNDPROC oWndProc = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainRenderTargetView;

	bool initDx = false;
	volatile LONG cleanupDone = 0;
	volatile LONG unloadRequested = 0;
	volatile LONG worldCleanupStarted = 0;
	volatile LONG worldCleanupDone = 0;
	volatile LONG cleanupStarted = 0;
	volatile LONG hookDisableStarted = 0;
	volatile LONG activePresentCalls = 0;
	SDK::UWorld* trackedWorld = nullptr;
	SDK::ULevel* trackedLevel = nullptr;

	static HRESULT __stdcall HkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

	void InitImGui();
	void Cleanup();
	void RenderImGui();

	void RenderMainWindow();

	ImFont* tahomaFont = nullptr;

	void SetupImGuiFonts();
	void SetupImGuiStyle();

	void MultiCombo(const char* label, const std::vector<const char*>& titles, const std::vector<bool*>& options, float width = 0.f);
	void MultiCombo(const char* label, const std::vector<const char*>& titles, const std::vector<int>& values, int* flag);
};
