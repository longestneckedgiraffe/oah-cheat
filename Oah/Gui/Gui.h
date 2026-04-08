#pragma once

#include "../Libs/Includes.h"

class Gui
{
public:
	Present oPresent = nullptr;
	HWND window = NULL;
	WNDPROC oWndProc = nullptr;
	ID3D11Device* pDevice = NULL;
	ID3D11DeviceContext* pContext = NULL;
	ID3D11RenderTargetView* mainRenderTargetView = NULL;

	bool initDx = false;
	bool cleanupDone = false;
	volatile LONG cleanupStarted = 0;
	volatile LONG activePresentCalls = 0;

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
