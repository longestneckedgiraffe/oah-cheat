#include <filesystem>
#include <utility>
#include "../Core/Manager.h"
#include "../Core/RenderHook.h"
#include "Gui.h"

namespace
{
	Present g_fallbackPresent = nullptr;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!manager || !manager->pGui)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	if (!manager->pConfig ||
		!manager->pConfig->menu.injected ||
		InterlockedCompareExchange(&manager->pGui->cleanupDone, 0, 0) != 0 ||
		InterlockedCompareExchange(&manager->pGui->unloadRequested, 0, 0) != 0)
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
	if (!manager || !manager->pGui)
		return g_fallbackPresent ? g_fallbackPresent(pSwapChain, SyncInterval, Flags) : S_OK;

	if (manager->pGui->oPresent)
		g_fallbackPresent = manager->pGui->oPresent;

	InterlockedIncrement(&manager->pGui->activePresentCalls);

	auto callOriginalPresent = [&]() -> HRESULT
	{
		HRESULT result = manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);
		InterlockedDecrement(&manager->pGui->activePresentCalls);
		return result;
	};

	auto finalizeUnloadPresent = [&]() -> HRESULT
	{
		if (InterlockedCompareExchange(&manager->pGui->worldCleanupStarted, 1, 0) == 0)
		{
			__try
			{
				const bool sdkReady = manager->UpdateSDK();
				if (sdkReady)
					manager->pHacks->DisableAll();
				manager->pEsp->DisableAll();
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				manager->ClearSDK();
			}

			InterlockedExchange(&manager->pGui->worldCleanupDone, 1);
		}

		manager->pGui->Cleanup();

		if (InterlockedCompareExchange(&manager->pGui->hookDisableStarted, 1, 0) == 0)
			RenderHook::Disable();

		return callOriginalPresent();
	};

	auto observeCurrentWorld = [&]() -> std::pair<SDK::UWorld*, SDK::ULevel*>
	{
		SDK::UWorld* observedWorld = nullptr;
		SDK::ULevel* observedLevel = nullptr;

		__try
		{
			observedWorld = SDK::UWorld::GetWorld();
			if (observedWorld && !Fns::IsBadPoint(observedWorld) && observedWorld->Levels.Num() > 0)
				observedLevel = observedWorld->Levels[0];
			else
				observedWorld = nullptr;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			observedWorld = nullptr;
			observedLevel = nullptr;
		}

		return { observedWorld, observedLevel };
	};

	auto cleanupForWorldTransition = [&](SDK::UWorld* observedWorld, SDK::ULevel* observedLevel)
	{
		if (!observedWorld)
			return;

		if (manager->pGui->trackedWorld &&
			(manager->pGui->trackedWorld != observedWorld || manager->pGui->trackedLevel != observedLevel))
		{
			manager->pHacks->OnWorldChanged();
			manager->pEsp->OnWorldChanged();
			manager->actorRegistry.Clear();
			manager->ClearSDK();
		}

		manager->pGui->trackedWorld = observedWorld;
		manager->pGui->trackedLevel = observedLevel;
	};

	if (InterlockedCompareExchange(&manager->pGui->cleanupDone, 0, 0) != 0)
	{
		return callOriginalPresent();
	}

	if (InterlockedCompareExchange(&manager->pGui->unloadRequested, 0, 0) != 0)
		return finalizeUnloadPresent();

	const auto [observedWorld, observedLevel] = observeCurrentWorld();
	cleanupForWorldTransition(observedWorld, observedLevel);

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
			return callOriginalPresent();
	}

	if (InterlockedCompareExchange(&manager->pGui->unloadRequested, 0, 0) != 0)
		return finalizeUnloadPresent();

	if (!manager->pConfig->menu.injected)
	{
		manager->pGui->Cleanup();
		return callOriginalPresent();
	}

	manager->pConfig->Hotkeys();
	const bool shouldRenderOverlay = manager->pConfig->menu.enabled || manager->pEsp->NeedsOverlayRender();
	bool sdkReady = false;

	__try
	{
		sdkReady = manager->UpdateSDK();
		manager->pEsp->Tick();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		manager->ClearSDK();
		sdkReady = false;
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
		if (sdkReady)
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

	return callOriginalPresent();
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
	if (Vars::World && Vars::MyController && Vars::CharacterClass)
		manager->pEsp->RenderOverlay();
}

void Gui::Cleanup()
{
	if (InterlockedCompareExchange(&cleanupStarted, 1, 0) != 0)
		return;

	if (window && oWndProc)
	{
		SetLastError(ERROR_SUCCESS);
		const LONG_PTR restoreResult = SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));
		if (restoreResult != 0 || GetLastError() == ERROR_SUCCESS)
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

	trackedWorld = nullptr;
	trackedLevel = nullptr;
	InterlockedExchange(&cleanupDone, 1);
}
