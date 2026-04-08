#include "../Core/Manager.h"
#include "Esp.h"

#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/BulletTrace_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/PlayerCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RatCharacter_classes.hpp"

void Esp::RefreshEspActorCache(bool forceRefresh)
{
	if (!Vars::World || Vars::World->Levels.Num() == 0)
	{
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
	{
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

	actorCacheFrameCounter++;
	const bool refreshNow = forceRefresh ||
		cachedEspLevel != currLevel ||
		actorCacheFrameCounter >= 30 ||
		cachedEspActors.empty();

	if (!refreshNow)
		return;

	cachedEspActors.clear();
	cachedEspActors.reserve(256);
	cachedEspLevel = currLevel;
	actorCacheFrameCounter = 0;

	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		if (currActor->IsA(SDK::ANPC_Guard_C::StaticClass()) || currActor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Police });
		}
		else if (currActor->IsA(SDK::APlayerCharacter_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Player });
		}
		else if (currActor->IsA(SDK::ACameraBP_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Camera });
		}
		else if (currActor->IsA(SDK::ARatCharacter_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Rat });
		}
	}
}

void Esp::RenderESP()
{
	bool policeNow = manager->pConfig->esp.policeEspEnabled;
	bool policeGlowNow = manager->pConfig->esp.policeGlowEnabled;
	bool policeBox2DNow = manager->pConfig->esp.policeBox2DEnabled;
	bool policeBox3DNow = manager->pConfig->esp.policeBox3DEnabled;
	bool playerNow = manager->pConfig->esp.playerEspEnabled;
	bool playerGlowNow = manager->pConfig->esp.playerGlowEnabled;
	bool playerBox2DNow = manager->pConfig->esp.playerBox2DEnabled;
	bool playerBox3DNow = manager->pConfig->esp.playerBox3DEnabled;
	bool cameraNow = manager->pConfig->esp.cameraEspEnabled;
	bool cameraGlowNow = manager->pConfig->esp.cameraGlowEnabled;
	bool cameraBox2DNow = manager->pConfig->esp.cameraBox2DEnabled;
	bool cameraBox3DNow = manager->pConfig->esp.cameraBox3DEnabled;
	bool ratNow = manager->pConfig->esp.ratEspEnabled;
	bool ratGlowNow = manager->pConfig->esp.ratGlowEnabled;
	bool ratBox2DNow = manager->pConfig->esp.ratBox2DEnabled;
	bool ratBox3DNow = manager->pConfig->esp.ratBox3DEnabled;

	bool anyGlowEnabled =
		(policeNow && policeGlowNow) ||
		(playerNow && playerGlowNow) ||
		(cameraNow && cameraGlowNow) ||
		(ratNow && ratGlowNow);

	bool anyBoxEnabled =
		(policeNow && (policeBox2DNow || policeBox3DNow)) ||
		(playerNow && (playerBox2DNow || playerBox3DNow)) ||
		(cameraNow && (cameraBox2DNow || cameraBox3DNow)) ||
		(ratNow && (ratBox2DNow || ratBox3DNow));

	bool prevAnyGlowEnabled =
		(prevPoliceEsp && prevPoliceGlow) ||
		(prevPlayerEsp && prevPlayerGlow) ||
		(prevCameraEsp && prevCameraGlow) ||
		(prevRatEsp && prevRatGlow);

	bool anyTrackedVisuals = anyGlowEnabled || anyBoxEnabled || prevAnyGlowEnabled;
	const bool filterDormantNow = manager->pConfig->settings.filterDormant;

	bool stateChanged =
		policeNow != prevPoliceEsp ||
		policeGlowNow != prevPoliceGlow ||
		playerNow != prevPlayerEsp ||
		playerGlowNow != prevPlayerGlow ||
		cameraNow != prevCameraEsp ||
		cameraGlowNow != prevCameraGlow ||
		ratNow != prevRatEsp ||
		ratGlowNow != prevRatGlow ||
		filterDormantNow != prevFilterDormant;

	RenderFovCircle();
	UpdateBulletTracers();
	RenderBulletTracers();

	if (anyTrackedVisuals)
		RefreshEspActorCache(stateChanged);

	RenderEntityBoxes();
	RenderDebugESP();

	bool shouldRun = false;
	if (stateChanged)
	{
		shouldRun = true;
		espFrameCounter = 0;
	}
	else if (anyGlowEnabled || prevAnyGlowEnabled)
	{
		espFrameCounter++;
		if (espFrameCounter >= 30)
		{
			shouldRun = true;
			espFrameCounter = 0;
		}
	}

	if (!shouldRun)
		return;

	if (anyGlowEnabled || prevAnyGlowEnabled)
		ApplyGlow();

	prevPoliceEsp = policeNow;
	prevPoliceGlow = policeGlowNow;
	prevPlayerEsp = playerNow;
	prevPlayerGlow = playerGlowNow;
	prevCameraEsp = cameraNow;
	prevCameraGlow = cameraGlowNow;
	prevRatEsp = ratNow;
	prevRatGlow = ratGlowNow;
	prevFilterDormant = filterDormantNow;
}

void Esp::ApplyGlow()
{
	if (!Vars::MyController)
		return;
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;

	const bool filterDormant = manager->pConfig->settings.filterDormant;

	for (const CachedEspActor& cachedActor : cachedEspActors)
	{
		SDK::AActor* currActor = cachedActor.actor;
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		if (cachedActor.type == TrackedActorType::Police && currActor->IsA(SDK::ANPC_Guard_C::StaticClass()))
		{
			auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
			bool enabling = manager->pConfig->esp.policeEspEnabled &&
				manager->pConfig->esp.policeGlowEnabled &&
				!(filterDormant && guard->Dead_);
			if (guard->Mesh)
				guard->Mesh->SetRenderCustomDepth(enabling);
			if (guard->Hat)
			{
				guard->Hat->SetRenderCustomDepth(enabling);
				if (enabling)
					guard->Hat->SetCustomDepthStencilValue(0);
			}
		}
		else if (cachedActor.type == TrackedActorType::Police && currActor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
		{
			auto* police = static_cast<SDK::ANPC_Police_base_C*>(currActor);
			bool enabling = manager->pConfig->esp.policeEspEnabled &&
				manager->pConfig->esp.policeGlowEnabled &&
				!(filterDormant && police->Dead_);
			auto* character = static_cast<SDK::ACharacter*>(currActor);
			if (character->Mesh)
				character->Mesh->SetRenderCustomDepth(enabling);
		}
		else if (cachedActor.type == TrackedActorType::Camera)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			bool enabling = manager->pConfig->esp.cameraEspEnabled &&
				manager->pConfig->esp.cameraGlowEnabled &&
				!(filterDormant && camera->Destroyed_);
			if (camera->CameraHead)
				camera->CameraHead->SetRenderCustomDepth(enabling);
			if (camera->CameraArm)
				camera->CameraArm->SetRenderCustomDepth(enabling);
		}
		else if (cachedActor.type == TrackedActorType::Player)
		{
			auto* character = static_cast<SDK::ACharacter*>(currActor);
			bool isLocalPlayer = currActor == Vars::CharacterClass || currActor->GetOwner() == Vars::MyController;
			bool enabling = manager->pConfig->esp.playerEspEnabled &&
				manager->pConfig->esp.playerGlowEnabled &&
				!isLocalPlayer;
			if (character->Mesh)
			{
				character->Mesh->SetRenderCustomDepth(enabling);
				if (enabling)
					character->Mesh->SetCustomDepthStencilValue(1);
			}
		}
		else if (cachedActor.type == TrackedActorType::Rat)
		{
			auto* rat = static_cast<SDK::ARatCharacter_C*>(currActor);
			bool enabling = manager->pConfig->esp.ratEspEnabled &&
				manager->pConfig->esp.ratGlowEnabled &&
				!(filterDormant && rat->Dead_);
			if (rat->Mesh)
				rat->Mesh->SetRenderCustomDepth(enabling);
		}
	}
}

void Esp::DisableAll()
{
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		std::string fullName = currActor->GetFullName();

		if (fullName.find("NPC_Guard") != std::string::npos)
		{
			auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
			if (guard->Mesh)
				guard->Mesh->SetRenderCustomDepth(false);
			if (guard->Hat)
				guard->Hat->SetRenderCustomDepth(false);
		}
		else if (fullName.find("NPC_Police") != std::string::npos ||
			fullName.find("PlayerCharacter") != std::string::npos ||
			fullName.find("RatCharacter") != std::string::npos)
		{
			auto* character = static_cast<SDK::ACharacter*>(currActor);
			if (character->Mesh)
				character->Mesh->SetRenderCustomDepth(false);
		}

		if (fullName.find("CameraBP") != std::string::npos)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			if (camera->CameraHead)
				camera->CameraHead->SetRenderCustomDepth(false);
			if (camera->CameraArm)
				camera->CameraArm->SetRenderCustomDepth(false);
		}
	}

	prevPoliceEsp = false;
	prevPoliceGlow = false;
	prevPlayerEsp = false;
	prevPlayerGlow = false;
	prevCameraEsp = false;
	prevCameraGlow = false;
	prevRatEsp = false;
	prevRatGlow = false;
	prevFilterDormant = true;
	cachedEspActors.clear();
	cachedEspLevel = nullptr;
	actorCacheFrameCounter = 0;
	liveBulletPositions.clear();
	bulletTracerSegments.clear();
}
