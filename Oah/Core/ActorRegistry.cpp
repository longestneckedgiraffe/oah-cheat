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

	std::vector<SDK::ULevel*> levels{};
	levels.reserve(Vars::World->Levels.Num());

	std::vector<std::uintptr_t> levelKeys{};
	levelKeys.reserve(Vars::World->Levels.Num());

	for (int i = 0; i < Vars::World->Levels.Num(); i++)
	{
		SDK::ULevel* level = Vars::World->Levels[i];
		if (!level)
			continue;

		levels.push_back(level);
		levelKeys.push_back(reinterpret_cast<std::uintptr_t>(level));
	}

	if (levels.empty())
	{
		Clear();
		return;
	}

	const bool worldChanged = cachedWorld != Vars::World || cachedLevelKeys != levelKeys;
	if (!force && !worldChanged)
		return;

	Rebuild(levels);
	cachedWorld = Vars::World;
	cachedLevelKeys = std::move(levelKeys);
	++revision;
}

void ActorRegistry::Clear()
{
	const bool hadState =
		cachedWorld != nullptr ||
		!cachedLevelKeys.empty() ||
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

	cachedWorld = nullptr;
	cachedLevelKeys.clear();

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

	std::unordered_set<std::uintptr_t> seenActors{};
	size_t estimatedActors = 0;
	for (SDK::ULevel* level : levels)
		estimatedActors += static_cast<size_t>(level->Actors.Num());
	seenActors.reserve(estimatedActors);

	for (SDK::ULevel* level : levels)
	{
		for (int actorIndex = 0; actorIndex < level->Actors.Num(); actorIndex++)
		{
			SDK::AActor* actor = level->Actors[actorIndex];
			if (!actor || !actor->RootComponent)
				continue;
			if (Fns::IsBadPoint(actor))
				continue;

			const std::uintptr_t actorKey = reinterpret_cast<std::uintptr_t>(actor);
			if (!seenActors.insert(actorKey).second)
				continue;

			if (actor->IsA(SDK::ANPC_Guard_C::StaticClass()))
				guards.push_back(actor);
			else if (actor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
				police.push_back(actor);
			else if (actor->IsA(SDK::APlayerCharacter_C::StaticClass()))
				players.push_back(actor);
			else if (actor->IsA(SDK::ACameraBP_C::StaticClass()))
				cameras.push_back(actor);
			else if (actor->IsA(SDK::ARatCharacter_C::StaticClass()))
				rats.push_back(actor);
			else if (actor->IsA(SDK::ADoorBP_C::StaticClass()))
				doors.push_back(actor);
			else if (actor->IsA(SDK::AAlarmBP_C::StaticClass()))
				alarms.push_back(actor);
			else if (actor->IsA(SDK::ALock_C::StaticClass()))
				locks.push_back(actor);
			else if (actor->IsA(SDK::ADuffelbag_C::StaticClass()))
				duffelbags.push_back(actor);
			else if (actor->IsA(SDK::ARobberTruck_C::StaticClass()))
				robberTrucks.push_back(actor);
			else if (actor->IsA(SDK::AMoney_base_C::StaticClass()))
				money.push_back(actor);
			else if (IsCivilianActor(actor))
				civilians.push_back(actor);
		}
	}
}
