#pragma once

#include "../Libs/UEDump/SDK.hpp"

namespace Vars {
	inline bool Debug =
#ifdef _DEBUG
		true;
#else
		false;
#endif
	inline bool ShowMenu = true;

	inline SDK::UEngine* Engine{};
	inline SDK::UWorld* World{};
	inline SDK::APlayerController* MyController{};
	inline SDK::APlayerCharacter_C* CharacterClass{};
	inline SDK::APawn* MyPawn{};
	inline SDK::TArray<class SDK::APlayerState*> PlayerArray{};
}
