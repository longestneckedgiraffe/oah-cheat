#include <filesystem>
#include "../Core/Manager.h"
#include "Gui.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!manager || !manager->pGui)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	if (!manager->pConfig || !manager->pConfig->menu.injected || manager->pGui->cleanupDone)
	{
		if (manager->pGui->oWndProc)
			return CallWindowProc(manager->pGui->oWndProc, hWnd, uMsg, wParam, lParam);

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	if (!manager->pGui->oWndProc)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	static bool hiddenMouse = false;
	if (manager->pConfig->menu.enabled)
	{
		if (Vars::MyController)
		{
			if(!Vars::MyController->bShowMouseCursor)
				hiddenMouse = true;
			Vars::MyController->bShowMouseCursor = true;
		}
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	if(hiddenMouse && Vars::MyController)
	{
		Vars::MyController->bShowMouseCursor = false;
		hiddenMouse = false;
	}

	return CallWindowProc(manager->pGui->oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT __stdcall Gui::HkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	InterlockedIncrement(&manager->pGui->activePresentCalls);

	if (manager->pGui->cleanupDone)
	{
		HRESULT result = manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);
		InterlockedDecrement(&manager->pGui->activePresentCalls);
		return result;
	}

	if (!manager->pGui->initDx)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&manager->pGui->pDevice)))
		{
			manager->pGui->pDevice->GetImmediateContext(&manager->pGui->pContext);

			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			manager->pGui->window = sd.OutputWindow;

			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

			manager->pGui->pDevice->CreateRenderTargetView(pBackBuffer, NULL, &manager->pGui->mainRenderTargetView);

			pBackBuffer->Release();

			manager->pGui->oWndProc = (WNDPROC)SetWindowLongPtr(manager->pGui->window, GWLP_WNDPROC, (LONG_PTR)WndProc);

			manager->pGui->InitImGui();
			manager->pGui->initDx = true;
		}
		else
		{
			HRESULT result = manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);
			InterlockedDecrement(&manager->pGui->activePresentCalls);
			return result;
		}
	}

	if (!manager->pConfig->menu.injected)
	{
		manager->pGui->Cleanup();
		HRESULT result = manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);
		InterlockedDecrement(&manager->pGui->activePresentCalls);
		return result;
	}

	manager->pConfig->Hotkeys();
	const bool shouldRenderOverlay = manager->pConfig->menu.enabled || manager->pEsp->NeedsOverlayRender();

	__try
	{
		manager->UpdateSDK();
		manager->pEsp->Tick();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		manager->ClearSDK();
	}

	if (shouldRenderOverlay)
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		__try
		{
			manager->pGui->RenderImGui();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			manager->ClearSDK();
		}

		ImGui::Render();
	}

	__try
	{
		manager->pHacks->RunHacks();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		manager->ClearSDK();
	}

	if (shouldRenderOverlay)
	{
		__try
		{
			manager->pGui->pContext->OMSetRenderTargets(1, &manager->pGui->mainRenderTargetView, NULL);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	HRESULT result = manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);
	InterlockedDecrement(&manager->pGui->activePresentCalls);
	return result;
}

void Gui::InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = nullptr;

	SetupImGuiFonts();
	SetupImGuiStyle();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

void Gui::RenderImGui()
{
	manager->pGui->RenderMainWindow();
	manager->pEsp->RenderOverlay();
}

void Gui::Cleanup()
{
	if (InterlockedCompareExchange(&cleanupStarted, 1, 0) != 0)
		return;

	if (window && oWndProc)
	{
		SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));
		oWndProc = nullptr;
	}

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	if (mainRenderTargetView)
	{
		mainRenderTargetView->Release();
		mainRenderTargetView = nullptr;
	}

	if (pContext)
	{
		pContext->Release();
		pContext = nullptr;
	}
	if (pDevice)
	{
		pDevice->Release();
		pDevice = nullptr;
	}

	cleanupDone = true;
}
