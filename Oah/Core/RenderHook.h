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
		VtablePatchFailed,
		VtableRestoreFailed
	};

	Status Initialize(Present hookFunction, Present* originalFunction);
	void Disable();
	void Shutdown();
	const char* StatusToString(Status status);
}
