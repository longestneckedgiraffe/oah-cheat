#pragma once

#include <cstdint>

#include "../Libs/UEDump/SDK.hpp"

namespace Fns
{
	inline constexpr std::int32_t InvalidObjectKey = -1;

	template <typename T>
	inline bool IsNullPointer(T* ptr) noexcept
	{
		return ptr == nullptr;
	}

	template <typename TObject>
	inline std::int32_t GetObjectKey(const TObject* object) noexcept
	{
		const auto* uObject = static_cast<const SDK::UObject*>(object);
		return uObject ? uObject->Index : InvalidObjectKey;
	}

	template <typename TObject>
	inline bool HasObjectKey(const TObject* object, std::int32_t expectedKey) noexcept
	{
		return GetObjectKey(object) == expectedKey;
	}

}
