#include "RenderHook.h"

namespace
{
	constexpr UINT kPresentVtableIndex = 8;

	void** g_presentSlot = nullptr;
	Present g_originalPresent = nullptr;
	Present g_hookPresent = nullptr;
	bool g_hookCreated = false;
	bool g_hookEnabled = false;
	bool g_hookInitialized = false;

	HWND CreateBootstrapWindow()
	{
		return CreateWindowExA(
			0,
			"STATIC",
			"OAHRenderHookWindow",
			WS_OVERLAPPEDWINDOW,
			0,
			0,
			100,
			100,
			nullptr,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr);
	}

	RenderHook::Status ResolvePresentSlot(void*** slot, Present* originalFunction)
	{
		if (!slot || !originalFunction)
			return RenderHook::Status::InvalidArgument;

		*slot = nullptr;
		*originalFunction = nullptr;

		HWND window = CreateBootstrapWindow();
		if (!window)
			return RenderHook::Status::CreateWindowFailed;

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = 1;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.OutputWindow = window;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Windowed = TRUE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		IDXGISwapChain* swapChain = nullptr;
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

		const HRESULT hardwareResult = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			&swapChainDesc,
			&swapChain,
			&device,
			&featureLevel,
			&context);

		HRESULT createResult = hardwareResult;
		if (FAILED(createResult))
		{
			createResult = D3D11CreateDeviceAndSwapChain(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				0,
				nullptr,
				0,
				D3D11_SDK_VERSION,
				&swapChainDesc,
				&swapChain,
				&device,
				&featureLevel,
				&context);
		}

		if (FAILED(createResult) || !swapChain)
		{
			if (context)
				context->Release();
			if (device)
				device->Release();
			DestroyWindow(window);
			return RenderHook::Status::CreateSwapChainFailed;
		}

		void** swapChainVtable = *reinterpret_cast<void***>(swapChain);
		if (!swapChainVtable || !swapChainVtable[kPresentVtableIndex])
		{
			context->Release();
			device->Release();
			swapChain->Release();
			DestroyWindow(window);
			return RenderHook::Status::PresentNotFound;
		}

		*slot = &swapChainVtable[kPresentVtableIndex];
		*originalFunction = reinterpret_cast<Present>(swapChainVtable[kPresentVtableIndex]);

		context->Release();
		device->Release();
		swapChain->Release();
		DestroyWindow(window);
		return RenderHook::Status::Success;
	}

	bool WritePresentSlot(Present function)
	{
		if (!g_presentSlot || !function)
			return false;

		DWORD oldProtect = 0;
		if (!VirtualProtect(g_presentSlot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
			return false;

		InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(g_presentSlot), reinterpret_cast<PVOID>(function));
		FlushInstructionCache(GetCurrentProcess(), g_presentSlot, sizeof(void*));

		DWORD restoreProtect = 0;
		VirtualProtect(g_presentSlot, sizeof(void*), oldProtect, &restoreProtect);
		return true;
	}
}

RenderHook::Status RenderHook::Initialize(Present hookFunction, Present* originalFunction)
{
	if (g_hookInitialized)
		return Status::AlreadyInitialized;

	if (!hookFunction || !originalFunction)
		return Status::InvalidArgument;

	*originalFunction = nullptr;

	Status resolveStatus = ResolvePresentSlot(&g_presentSlot, &g_originalPresent);
	if (resolveStatus != Status::Success)
		return resolveStatus;

	g_hookPresent = hookFunction;
	*originalFunction = g_originalPresent;

	if (!WritePresentSlot(g_hookPresent))
	{
		g_presentSlot = nullptr;
		g_originalPresent = nullptr;
		g_hookPresent = nullptr;
		return Status::VtablePatchFailed;
	}

	g_hookCreated = true;
	g_hookEnabled = true;
	g_hookInitialized = true;
	return Status::Success;
}

void RenderHook::Disable()
{
	if (!g_hookCreated || !g_hookEnabled || !g_originalPresent)
		return;

	if (WritePresentSlot(g_originalPresent))
		g_hookEnabled = false;
}

void RenderHook::Shutdown()
{
	if (!g_hookInitialized && !g_hookCreated && !g_hookEnabled && !g_presentSlot)
		return;

	if (g_hookEnabled)
		Disable();

	g_presentSlot = nullptr;
	g_originalPresent = nullptr;
	g_hookPresent = nullptr;
	g_hookCreated = false;
	g_hookEnabled = false;
	g_hookInitialized = false;
}

const char* RenderHook::StatusToString(Status status)
{
	switch (status)
	{
	case Status::Success:
		return "Success";
	case Status::AlreadyInitialized:
		return "AlreadyInitialized";
	case Status::NotInitialized:
		return "NotInitialized";
	case Status::InvalidArgument:
		return "InvalidArgument";
	case Status::CreateWindowFailed:
		return "CreateWindowFailed";
	case Status::CreateSwapChainFailed:
		return "CreateSwapChainFailed";
	case Status::PresentNotFound:
		return "PresentNotFound";
	case Status::VtablePatchFailed:
		return "VtablePatchFailed";
	case Status::VtableRestoreFailed:
		return "VtableRestoreFailed";
	default:
		return "UnknownStatus";
	}
}
