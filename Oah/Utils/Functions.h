#pragma once
#include "../Libs/UEDump/SDK.hpp"

namespace Fns
{
	template <typename T>
	inline bool IsBadPoint(T* ptr)
	{
		return ptr == nullptr;
	}
}
