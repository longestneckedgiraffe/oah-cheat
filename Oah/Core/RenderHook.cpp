#include "RenderHook.h"

#include "../Libs/MinHook/include/MinHook.h"

namespace
{
	constexpr UINT kPresentVtableIndex = 8;

	void* g_presentTarget = nullptr;
	bool g_hookCreated = false;
	bool g_hookInitialized = false;
	bool g_ownsMinHook = false;

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

	RenderHook::Status ResolvePresentTarget(void** target)
	{
		if (!target)
			return RenderHook::Status::InvalidArgument;

		*target = nullptr;

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

		*target = swapChainVtable[kPresentVtableIndex];

		context->Release();
		device->Release();
		swapChain->Release();
		DestroyWindow(window);
		return RenderHook::Status::Success;
	}
}

RenderHook::Status RenderHook::Initialize(Present hookFunction, Present* originalFunction)
{
	if (g_hookInitialized)
		return Status::AlreadyInitialized;

	if (!hookFunction || !originalFunction)
		return Status::InvalidArgument;

	*originalFunction = nullptr;

	Status targetStatus = ResolvePresentTarget(&g_presentTarget);
	if (targetStatus != Status::Success)
		return targetStatus;

	const MH_STATUS initStatus = MH_Initialize();
	if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED)
	{
		g_presentTarget = nullptr;
		return Status::MinHookInitializeFailed;
	}

	g_ownsMinHook = (initStatus == MH_OK);

	if (MH_CreateHook(g_presentTarget, reinterpret_cast<LPVOID>(hookFunction), reinterpret_cast<LPVOID*>(originalFunction)) != MH_OK)
	{
		if (g_ownsMinHook)
			MH_Uninitialize();

		g_ownsMinHook = false;
		g_presentTarget = nullptr;
		return Status::MinHookCreateFailed;
	}

	g_hookCreated = true;

	if (MH_EnableHook(g_presentTarget) != MH_OK)
	{
		MH_RemoveHook(g_presentTarget);
		g_hookCreated = false;

		if (g_ownsMinHook)
			MH_Uninitialize();

		g_ownsMinHook = false;
		g_presentTarget = nullptr;
		return Status::MinHookEnableFailed;
	}

	g_hookInitialized = true;
	return Status::Success;
}

void RenderHook::Shutdown()
{
	if (!g_hookInitialized && !g_hookCreated && !g_presentTarget && !g_ownsMinHook)
		return;

	if (g_hookCreated && g_presentTarget)
	{
		MH_DisableHook(g_presentTarget);
		MH_RemoveHook(g_presentTarget);
	}

	if (g_ownsMinHook)
		MH_Uninitialize();

	g_presentTarget = nullptr;
	g_hookCreated = false;
	g_hookInitialized = false;
	g_ownsMinHook = false;
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
	case Status::MinHookInitializeFailed:
		return "MinHookInitializeFailed";
	case Status::MinHookCreateFailed:
		return "MinHookCreateFailed";
	case Status::MinHookEnableFailed:
		return "MinHookEnableFailed";
	default:
		return "UnknownStatus";
	}
}
