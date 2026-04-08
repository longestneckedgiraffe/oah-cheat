#pragma once

#include "../Libs/Includes.h"

namespace RenderHook
{
	enum class Status
	{
		Success,
		AlreadyInitialized,
		NotInitialized,
		InvalidArgument,
		CreateWindowFailed,
		CreateSwapChainFailed,
		PresentNotFound,
		MinHookInitializeFailed,
		MinHookCreateFailed,
		MinHookEnableFailed
	};

	Status Initialize(Present hookFunction, Present* originalFunction);
	void Shutdown();
	const char* StatusToString(Status status);
}
