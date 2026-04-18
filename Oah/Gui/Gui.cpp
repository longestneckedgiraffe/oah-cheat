#include <filesystem>
#include <iostream>
#include <utility>
#include "../Core/Manager.h"
#include "../Core/RenderHook.h"
#include "Gui.h"

namespace
{
	Present g_fallbackPresent = nullptr;

	bool UpdateSdkAndTickSafely(bool& sdkReady)
	{
		sdkReady = false;
		__try
		{
			sdkReady = manager->UpdateSDK();
			manager->pEsp->Tick();
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			manager->ClearSDK();
			sdkReady = false;
			return false;
		}
	}

	void RenderOverlaySafely(bool shouldRenderOverlay)
	{
		if (!shouldRenderOverlay)
			return;

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

		__try
		{
			ID3D11RenderTargetView* renderTargetView = manager->pGui->mainRenderTargetView.Get();
			manager->pGui->pContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void RunHacksSafely(bool sdkReady)
	{
		__try
		{
			if (sdkReady)
				manager->pHacks->RunHacks();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			manager->ClearSDK();
		}
	}
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
	auto finishPresent = [&](HRESULT result) -> HRESULT
	{
		InterlockedDecrement(&manager->pGui->activePresentCalls);
		return result;
	};

	auto callOriginalPresent = [&]() -> HRESULT
	{
		if (manager->pGui->oPresent)
			return manager->pGui->oPresent(pSwapChain, SyncInterval, Flags);

		return g_fallbackPresent ? g_fallbackPresent(pSwapChain, SyncInterval, Flags) : S_OK;
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
			if (observedWorld && !Fns::IsNullPointer(observedWorld) && observedWorld->Levels.Num() > 0)
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
		return finishPresent(callOriginalPresent());
	}

	if (InterlockedCompareExchange(&manager->pGui->unloadRequested, 0, 0) != 0)
		return finishPresent(finalizeUnloadPresent());

	const auto [observedWorld, observedLevel] = observeCurrentWorld();
	cleanupForWorldTransition(observedWorld, observedLevel);

	if (!manager->pGui->initDx)
	{
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		if (FAILED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(device.GetAddressOf()))))
			return finishPresent(callOriginalPresent());

		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		device->GetImmediateContext(context.GetAddressOf());
		if (!context)
			return finishPresent(callOriginalPresent());

		DXGI_SWAP_CHAIN_DESC sd{};
		if (FAILED(pSwapChain->GetDesc(&sd)))
			return finishPresent(callOriginalPresent());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
		if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()))))
			return finishPresent(callOriginalPresent());

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
		if (FAILED(device->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf())))
			return finishPresent(callOriginalPresent());

		HWND outputWindow = sd.OutputWindow;
		SetLastError(ERROR_SUCCESS);
		LONG_PTR previousWndProc = SetWindowLongPtr(outputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));
		if (previousWndProc == 0 && GetLastError() != ERROR_SUCCESS)
		{
			std::cout << "[WndProc] Install failed on HWND=0x" << std::hex << reinterpret_cast<std::uintptr_t>(outputWindow)
				<< " (GLE=" << std::dec << GetLastError() << ")" << std::endl;
			return finishPresent(callOriginalPresent());
		}

		std::cout << "[WndProc] Installed on HWND=0x" << std::hex << reinterpret_cast<std::uintptr_t>(outputWindow)
			<< " (original=0x" << static_cast<std::uintptr_t>(previousWndProc) << ")" << std::dec << std::endl;

		manager->pGui->window = outputWindow;
		manager->pGui->pDevice = std::move(device);
		manager->pGui->pContext = std::move(context);
		manager->pGui->mainRenderTargetView = std::move(renderTargetView);
		manager->pGui->oWndProc = reinterpret_cast<WNDPROC>(previousWndProc);

		manager->pGui->InitImGui();
		manager->pGui->initDx = true;
	}

	if (InterlockedCompareExchange(&manager->pGui->unloadRequested, 0, 0) != 0)
		return finishPresent(finalizeUnloadPresent());

	if (!manager->pConfig->menu.injected)
	{
		manager->pGui->Cleanup();
		return finishPresent(callOriginalPresent());
	}

	manager->pConfig->Hotkeys();
	const bool shouldRenderOverlay = manager->pConfig->menu.enabled || manager->pEsp->NeedsOverlayRender();
	bool sdkReady = false;

	UpdateSdkAndTickSafely(sdkReady);
	RenderOverlaySafely(shouldRenderOverlay);
	RunHacksSafely(sdkReady);

	return finishPresent(callOriginalPresent());
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
	ImGui_ImplDX11_Init(pDevice.Get(), pContext.Get());
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
		const LONG_PTR restoreResult = SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
		if (restoreResult != 0 || GetLastError() == ERROR_SUCCESS)
		{
			std::cout << "[WndProc] Restored on HWND=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window)
				<< std::dec << std::endl;
			oWndProc = nullptr;
		}
		else
		{
			std::cout << "[WndProc] Restore failed on HWND=0x" << std::hex << reinterpret_cast<std::uintptr_t>(window)
				<< " (GLE=" << std::dec << GetLastError() << ")" << std::endl;
		}
	}

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	if (mainRenderTargetView)
	{
		mainRenderTargetView.Reset();
	}

	if (pContext)
	{
		pContext.Reset();
	}
	if (pDevice)
	{
		pDevice.Reset();
	}

	window = nullptr;
	trackedWorld = nullptr;
	trackedLevel = nullptr;
	InterlockedExchange(&cleanupDone, 1);
}
