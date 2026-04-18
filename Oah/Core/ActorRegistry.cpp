#include "ActorRegistry.h"

#include <unordered_set>

#include "Variables.h"
#include "../Libs/UEDump/SDK/AlarmBP_classes.hpp"
#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/DoorBP_classes.hpp"
#include "../Libs/UEDump/SDK/Duffelbag_classes.hpp"
#include "../Libs/UEDump/SDK/Lock_classes.hpp"
#include "../Libs/UEDump/SDK/Money_base_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/PlayerCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RatCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RobberTruck_classes.hpp"
#include "../Utils/Functions.h"

namespace
{
	constexpr std::uint32_t kRefreshInterval = 30;

	bool IsCivilianActor(SDK::AActor* actor)
	{
		return actor && actor->GetFullName().find("Civilian_NPC") != std::string::npos;
	}
}

void ActorRegistry::Refresh(bool force)
{
	if (!Vars::World || Vars::World->Levels.Num() == 0)
	{
		Clear();
		return;
	}

	if (!force)
	{
		if (refreshTicks < kRefreshInterval)
		{
			++refreshTicks;
			return;
		}
		refreshTicks = 0;
	}

	std::vector<SDK::ULevel*> levels{};
	levels.reserve(Vars::World->Levels.Num());

	for (int i = 0; i < Vars::World->Levels.Num(); i++)
	{
		SDK::ULevel* level = Vars::World->Levels[i];
		if (level)
			levels.push_back(level);
	}

	if (levels.empty())
	{
		Clear();
		return;
	}

	Rebuild(levels);
	++revision;
}

void ActorRegistry::Clear()
{
	const bool hadState =
		!guards.empty() ||
		!police.empty() ||
		!players.empty() ||
		!cameras.empty() ||
		!rats.empty() ||
		!doors.empty() ||
		!alarms.empty() ||
		!locks.empty() ||
		!money.empty() ||
		!duffelbags.empty() ||
		!robberTrucks.empty() ||
		!civilians.empty();

	refreshTicks = 0;

	guards.clear();
	police.clear();
	players.clear();
	cameras.clear();
	rats.clear();
	doors.clear();
	alarms.clear();
	locks.clear();
	money.clear();
	duffelbags.clear();
	robberTrucks.clear();
	civilians.clear();

	if (hadState)
		++revision;
}

void ActorRegistry::Rebuild(const std::vector<SDK::ULevel*>& levels)
{
	guards.clear();
	police.clear();
	players.clear();
	cameras.clear();
	rats.clear();
	doors.clear();
	alarms.clear();
	locks.clear();
	money.clear();
	duffelbags.clear();
	robberTrucks.clear();
	civilians.clear();

	std::unordered_set<SDK::AActor*> seenActors{};
	size_t estimatedActors = 0;
	for (SDK::ULevel* level : levels)
		estimatedActors += static_cast<size_t>(level->Actors.Num());
	seenActors.reserve(estimatedActors);

	SDK::UClass* const guardClass = SDK::ANPC_Guard_C::StaticClass();
	SDK::UClass* const policeClass = SDK::ANPC_Police_base_C::StaticClass();
	SDK::UClass* const playerClass = SDK::APlayerCharacter_C::StaticClass();
	SDK::UClass* const cameraClass = SDK::ACameraBP_C::StaticClass();
	SDK::UClass* const ratClass = SDK::ARatCharacter_C::StaticClass();
	SDK::UClass* const doorClass = SDK::ADoorBP_C::StaticClass();
	SDK::UClass* const alarmClass = SDK::AAlarmBP_C::StaticClass();
	SDK::UClass* const lockClass = SDK::ALock_C::StaticClass();
	SDK::UClass* const duffelbagClass = SDK::ADuffelbag_C::StaticClass();
	SDK::UClass* const robberTruckClass = SDK::ARobberTruck_C::StaticClass();
	SDK::UClass* const moneyClass = SDK::AMoney_base_C::StaticClass();

	for (SDK::ULevel* level : levels)
	{
		for (int actorIndex = 0; actorIndex < level->Actors.Num(); actorIndex++)
		{
			SDK::AActor* actor = level->Actors[actorIndex];
			if (!actor || !actor->RootComponent)
				continue;
			if (Fns::IsNullPointer(actor))
				continue;

			if (!seenActors.insert(actor).second)
				continue;

			if (actor->IsA(guardClass))
				guards.push_back(actor);
			else if (actor->IsA(policeClass))
				police.push_back(actor);
			else if (actor->IsA(playerClass))
				players.push_back(actor);
			else if (actor->IsA(cameraClass))
				cameras.push_back(actor);
			else if (actor->IsA(ratClass))
				rats.push_back(actor);
			else if (actor->IsA(doorClass))
				doors.push_back(actor);
			else if (actor->IsA(alarmClass))
				alarms.push_back(actor);
			else if (actor->IsA(lockClass))
				locks.push_back(actor);
			else if (actor->IsA(duffelbagClass))
				duffelbags.push_back(actor);
			else if (actor->IsA(robberTruckClass))
				robberTrucks.push_back(actor);
			else if (actor->IsA(moneyClass))
				money.push_back(actor);
			else if (IsCivilianActor(actor))
				civilians.push_back(actor);
		}
	}
}
