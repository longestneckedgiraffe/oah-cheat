#include "../Core/Manager.h"
#include "Esp.h"

#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/BulletTrace_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/PlayerCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RatCharacter_classes.hpp"

namespace
{
	bool HasAnyBoxOverlayEnabled(const Config& config)
	{
		return
			(config.esp.policeEspEnabled && (config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled)) ||
			(config.esp.playerEspEnabled && (config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled)) ||
			(config.esp.cameraEspEnabled && (config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled)) ||
			(config.esp.ratEspEnabled && (config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled));
	}

	bool HasAnyGlowEnabled(const Config& config)
	{
		return
			(config.esp.policeEspEnabled && config.esp.policeGlowEnabled) ||
			(config.esp.playerEspEnabled && config.esp.playerGlowEnabled) ||
			(config.esp.cameraEspEnabled && config.esp.cameraGlowEnabled) ||
			(config.esp.ratEspEnabled && config.esp.ratGlowEnabled);
	}
}

void Esp::RefreshEspActorCache(bool forceRefresh, bool trackPolice, bool trackPlayers, bool trackCameras, bool trackRats)
{
	if (!trackPolice && !trackPlayers && !trackCameras && !trackRats)
	{
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

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
		actorCacheFrameCounter >= 120 ||
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

		if (trackPolice && currActor->IsA(SDK::ANPC_Guard_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Guard });
		}
		else if (trackPolice && currActor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Police });
		}
		else if (trackPlayers && currActor->IsA(SDK::APlayerCharacter_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Player });
		}
		else if (trackCameras && currActor->IsA(SDK::ACameraBP_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Camera });
		}
		else if (trackRats && currActor->IsA(SDK::ARatCharacter_C::StaticClass()))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Rat });
		}
	}
}

bool Esp::NeedsOverlayRender() const
{
	if (!manager || !manager->pConfig)
		return false;

	const Config& config = *manager->pConfig;
	return
		(config.aimbot.enabled && config.aimbot.showFov) ||
		config.esp.bulletTracersEnabled ||
		HasAnyBoxOverlayEnabled(config);
}

void Esp::Tick()
{
	if (!manager || !manager->pConfig)
		return;

	const Config& config = *manager->pConfig;

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

	bool anyGlowEnabled = HasAnyGlowEnabled(config);
	bool anyBoxEnabled = HasAnyBoxOverlayEnabled(config);
	bool prevAnyGlowEnabled =
		(prevPoliceEsp && prevPoliceGlow) ||
		(prevPlayerEsp && prevPlayerGlow) ||
		(prevCameraEsp && prevCameraGlow) ||
		(prevRatEsp && prevRatGlow);

	bool anyTrackedVisuals = anyGlowEnabled || anyBoxEnabled || prevAnyGlowEnabled;
	const bool filterDormantNow = manager->pConfig->settings.filterDormant;
	const bool trackPolice = policeNow || prevPoliceEsp;
	const bool trackPlayers = playerNow || prevPlayerEsp;
	const bool trackCameras = cameraNow || prevCameraEsp;
	const bool trackRats = ratNow || prevRatEsp;

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

	UpdateBulletTracers();

	if (anyTrackedVisuals)
		RefreshEspActorCache(stateChanged, trackPolice, trackPlayers, trackCameras, trackRats);
	else
		RefreshEspActorCache(false, false, false, false, false);

	bool shouldRun = false;
	if (stateChanged)
	{
		shouldRun = true;
		espFrameCounter = 0;
	}
	else if (anyGlowEnabled || prevAnyGlowEnabled)
	{
		espFrameCounter++;
		if (espFrameCounter >= 120)
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

void Esp::RenderOverlay()
{
	RenderFovCircle();
	RenderBulletTracers();
	RenderEntityBoxes();
}

void Esp::RenderESP()
{
	Tick();
	if (NeedsOverlayRender())
		RenderOverlay();
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

		if (cachedActor.type == TrackedActorType::Guard)
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
		else if (cachedActor.type == TrackedActorType::Police)
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
