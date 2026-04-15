#include "../Core/Manager.h"
#include "Esp.h"

#include <vector>

#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/BulletTrace_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/PlayerCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RatCharacter_classes.hpp"

namespace
{
	static constexpr int kGlowStencilValue = 0;

	void AppendTrackedActors(
		std::vector<Esp::CachedEspActor>& destination,
		const std::vector<SDK::AActor*>& actors,
		Esp::TrackedActorType type)
	{
		for (SDK::AActor* actor : actors)
			destination.push_back({ actor, type });
	}

	bool IsPoliceEspActive(const Config& config)
	{
		return config.esp.policeGlowEnabled || config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled;
	}

	bool IsPlayerEspActive(const Config& config)
	{
		return config.esp.playerGlowEnabled || config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled;
	}

	bool IsCameraEspActive(const Config& config)
	{
		return config.esp.cameraGlowEnabled || config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled;
	}

	bool IsRatEspActive(const Config& config)
	{
		return config.esp.ratGlowEnabled || config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled;
	}

	bool HasAnyBoxOverlayEnabled(const Config& config)
	{
		return
			(IsPoliceEspActive(config) && (config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled)) ||
			(IsPlayerEspActive(config) && (config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled)) ||
			(IsCameraEspActive(config) && (config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled)) ||
			(IsRatEspActive(config) && (config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled));
	}

	bool HasAnyGlowEnabled(const Config& config)
	{
		return
			(IsPoliceEspActive(config) && config.esp.policeGlowEnabled) ||
			(IsPlayerEspActive(config) && config.esp.playerGlowEnabled) ||
			(IsCameraEspActive(config) && config.esp.cameraGlowEnabled) ||
			(IsRatEspActive(config) && config.esp.ratGlowEnabled);
	}

	SDK::FLinearColor ToLinearColor(const float color[4])
	{
		return SDK::FLinearColor{ color[0], color[1], color[2], color[3] };
	}

	void GetGlowColors(const Config& config, SDK::FLinearColor& primaryColor, SDK::FLinearColor& secondaryColor)
	{
		primaryColor = ToLinearColor(config.esp.glowColor);
		secondaryColor = primaryColor;
	}

	bool AreSameColor(const SDK::FLinearColor& lhs, const SDK::FLinearColor& rhs)
	{
		return
			lhs.R == rhs.R &&
			lhs.G == rhs.G &&
			lhs.B == rhs.B &&
			lhs.A == rhs.A;
	}

	void ApplyGlowToPrimitive(SDK::UPrimitiveComponent* component, bool enabling, int stencilValue)
	{
		if (!component)
			return;

		__try
		{
			if (!SDK::UKismetSystemLibrary::IsValid(component))
				return;

			component->SetRenderCustomDepth(enabling);
			if (enabling)
				component->SetCustomDepthStencilValue(stencilValue);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void DisableGlowOnPrimitive(SDK::UPrimitiveComponent* component)
	{
		if (!component)
			return;

		__try
		{
			if (!SDK::UKismetSystemLibrary::IsValid(component))
				return;

			component->SetRenderCustomDepth(false);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	SDK::TArray<SDK::FWeightedBlendable>* GetBlendablesForOwner(Esp::GlowBlendableOwnerType ownerType, SDK::UObject* owner)
	{
		if (!owner)
			return nullptr;

		__try
		{
			switch (ownerType)
			{
			case Esp::GlowBlendableOwnerType::Camera:
				return &static_cast<SDK::UCameraComponent*>(owner)->PostProcessSettings.WeightedBlendables.Array;
			case Esp::GlowBlendableOwnerType::PostProcessVolume:
				return &static_cast<SDK::APostProcessVolume*>(owner)->Settings.WeightedBlendables.Array;
			default:
				return nullptr;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	bool IsTargetGlowBlendableMaterial(SDK::UMaterialInterface* material)
	{
		if (!material)
			return false;

		return material->GetFullName().find("HighlightMat_Inst") != std::string::npos;
	}

	void ApplyGlowColorToMaterial(
		SDK::UMaterialInstanceDynamic* material,
		const SDK::FLinearColor& primaryColor,
		const SDK::FLinearColor& secondaryColor)
	{
		if (!material)
			return;

		static SDK::FName colorName{};
		static SDK::FName color1Name{};

		material->SetVectorParameterValue(SDK::GetStaticName(L"Color", colorName), primaryColor);
		material->SetVectorParameterValue(SDK::GetStaticName(L"Color1", color1Name), secondaryColor);
	}

	SDK::UMaterialInstanceDynamic* CreateGlowOverrideMaterial(SDK::UMaterialInterface* sourceMaterial)
	{
		if (!Vars::World || !sourceMaterial)
			return nullptr;

		SDK::UMaterialInstanceDynamic* dynamicMaterial =
			SDK::UKismetMaterialLibrary::CreateDynamicMaterialInstance(
				Vars::World,
				sourceMaterial,
				SDK::FName{},
				SDK::EMIDCreationFlags::Transient);

		if (!dynamicMaterial)
			return nullptr;

		dynamicMaterial->K2_CopyMaterialInstanceParameters(sourceMaterial, false);
		return dynamicMaterial;
	}
}

void Esp::RefreshEspActorCache(bool forceRefresh, bool trackPolice, bool trackPlayers, bool trackCameras, bool trackRats)
{
	if (!trackPolice && !trackPlayers && !trackCameras && !trackRats)
	{
		cachedEspActors.clear();
		cachedActorRegistryRevision = 0;
		return;
	}

	if (!manager)
		return;

	const ActorRegistry& actorRegistry = manager->actorRegistry;
	const std::uint64_t registryRevision = actorRegistry.GetRevision();
	if (!forceRefresh && cachedActorRegistryRevision == registryRevision)
		return;

	cachedEspActors.clear();
	const size_t reserveCount =
		(trackPolice ? actorRegistry.GetGuards().size() + actorRegistry.GetPolice().size() : 0) +
		(trackPlayers ? actorRegistry.GetPlayers().size() : 0) +
		(trackCameras ? actorRegistry.GetCameras().size() : 0) +
		(trackRats ? actorRegistry.GetRats().size() : 0);
	cachedEspActors.reserve(reserveCount);

	if (trackPolice)
	{
		AppendTrackedActors(cachedEspActors, actorRegistry.GetGuards(), TrackedActorType::Guard);
		AppendTrackedActors(cachedEspActors, actorRegistry.GetPolice(), TrackedActorType::Police);
	}
	if (trackPlayers)
		AppendTrackedActors(cachedEspActors, actorRegistry.GetPlayers(), TrackedActorType::Player);
	if (trackCameras)
		AppendTrackedActors(cachedEspActors, actorRegistry.GetCameras(), TrackedActorType::Camera);
	if (trackRats)
		AppendTrackedActors(cachedEspActors, actorRegistry.GetRats(), TrackedActorType::Rat);

	cachedActorRegistryRevision = registryRevision;
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

void Esp::TrackGlowPrimitive(SDK::UPrimitiveComponent* component)
{
	if (!component)
		return;

	const std::int32_t componentKey = Fns::GetObjectKey(component);
	if (componentKey == Fns::InvalidObjectKey)
		return;

	trackedGlowPrimitives[componentKey] = component;
}

void Esp::Tick()
{
	if (!manager || !manager->pConfig)
		return;

	const Config& config = *manager->pConfig;

	bool policeNow = IsPoliceEspActive(config);
	bool policeGlowNow = manager->pConfig->esp.policeGlowEnabled;
	bool policeBox2DNow = manager->pConfig->esp.policeBox2DEnabled;
	bool policeBox3DNow = manager->pConfig->esp.policeBox3DEnabled;
	bool playerNow = IsPlayerEspActive(config);
	bool playerGlowNow = manager->pConfig->esp.playerGlowEnabled;
	bool playerBox2DNow = manager->pConfig->esp.playerBox2DEnabled;
	bool playerBox3DNow = manager->pConfig->esp.playerBox3DEnabled;
	bool cameraNow = IsCameraEspActive(config);
	bool cameraGlowNow = manager->pConfig->esp.cameraGlowEnabled;
	bool cameraBox2DNow = manager->pConfig->esp.cameraBox2DEnabled;
	bool cameraBox3DNow = manager->pConfig->esp.cameraBox3DEnabled;
	bool ratNow = IsRatEspActive(config);
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
	{
		if (stateChanged || cachedActorRegistryRevision == 0)
			manager->actorRegistry.Refresh(true);
		RefreshEspActorCache(stateChanged, trackPolice, trackPlayers, trackCameras, trackRats);
	}
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

	if (anyGlowEnabled)
		ApplyGlowColorOverride(shouldRun);
	else
		RestoreGlowColorOverride();

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

void Esp::ApplyGlow()
{
	if (!Vars::MyController)
		return;
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;
	if (!manager || !manager->pConfig)
		return;

	const Config& config = *manager->pConfig;
	const bool filterDormant = config.settings.filterDormant;

	for (const CachedEspActor& cachedActor : cachedEspActors)
	{
		__try
		{
			SDK::AActor* currActor = cachedActor.actor;
			if (!currActor || !currActor->RootComponent)
				continue;
			if (Fns::IsNullPointer(currActor) || !SDK::UKismetSystemLibrary::IsValid(currActor))
				continue;

			if (cachedActor.type == TrackedActorType::Guard)
			{
				auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
				bool enabling = IsPoliceEspActive(config) &&
					config.esp.policeGlowEnabled &&
					!(filterDormant && guard->Dead_);
				TrackGlowPrimitive(guard->Mesh);
				TrackGlowPrimitive(guard->Hat);
				ApplyGlowToPrimitive(guard->Mesh, enabling, kGlowStencilValue);
				ApplyGlowToPrimitive(guard->Hat, enabling, kGlowStencilValue);
			}
			else if (cachedActor.type == TrackedActorType::Police)
			{
				auto* police = static_cast<SDK::ANPC_Police_base_C*>(currActor);
				bool enabling = IsPoliceEspActive(config) &&
					config.esp.policeGlowEnabled &&
					!(filterDormant && police->Dead_);
				auto* character = static_cast<SDK::ACharacter*>(currActor);
				TrackGlowPrimitive(character ? character->Mesh : nullptr);
				ApplyGlowToPrimitive(
					character ? character->Mesh : nullptr,
					enabling,
					kGlowStencilValue);
			}
			else if (cachedActor.type == TrackedActorType::Camera)
			{
				auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
				bool enabling = IsCameraEspActive(config) &&
					config.esp.cameraGlowEnabled &&
					!(filterDormant && camera->Destroyed_);
				TrackGlowPrimitive(camera->CameraViewCollision);
				TrackGlowPrimitive(camera->CameraHead);
				TrackGlowPrimitive(camera->CameraArm);
				DisableGlowOnPrimitive(camera->CameraViewCollision);
				ApplyGlowToPrimitive(camera->CameraHead, enabling, kGlowStencilValue);
				ApplyGlowToPrimitive(camera->CameraArm, enabling, kGlowStencilValue);
			}
			else if (cachedActor.type == TrackedActorType::Player)
			{
				auto* character = static_cast<SDK::ACharacter*>(currActor);
				bool isLocalPlayer = currActor == Vars::CharacterClass || currActor->GetOwner() == Vars::MyController;
				bool enabling = IsPlayerEspActive(config) &&
					config.esp.playerGlowEnabled &&
					!isLocalPlayer;
				TrackGlowPrimitive(character ? character->Mesh : nullptr);
				ApplyGlowToPrimitive(
					character ? character->Mesh : nullptr,
					enabling,
					kGlowStencilValue);
			}
			else if (cachedActor.type == TrackedActorType::Rat)
			{
				auto* rat = static_cast<SDK::ARatCharacter_C*>(currActor);
				bool enabling = IsRatEspActive(config) &&
					config.esp.ratGlowEnabled &&
					!(filterDormant && rat->Dead_);
				TrackGlowPrimitive(rat ? rat->Mesh : nullptr);
				ApplyGlowToPrimitive(
					rat ? rat->Mesh : nullptr,
					enabling,
					kGlowStencilValue);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
}

void Esp::ApplyGlowColorOverride(bool shouldScan)
{
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;

	SDK::ULevel* currentLevel = Vars::World->Levels[0];
	if (!currentLevel)
		return;

	if (glowOverrideLevel && glowOverrideLevel != currentLevel)
		RestoreGlowColorOverride();

	SDK::FLinearColor primaryGlowColor{};
	SDK::FLinearColor secondaryGlowColor{};
	GetGlowColors(*manager->pConfig, primaryGlowColor, secondaryGlowColor);

	const bool colorChanged =
		!hasLastAppliedGlowColor ||
		!AreSameColor(lastAppliedGlowColor, primaryGlowColor) ||
		!AreSameColor(lastAppliedGlowSecondaryColor, secondaryGlowColor);
	if (colorChanged)
	{
		lastAppliedGlowColor = primaryGlowColor;
		lastAppliedGlowSecondaryColor = secondaryGlowColor;
		hasLastAppliedGlowColor = true;
	}

	std::vector<GlowBlendableOverride> activeOverrides{};
	activeOverrides.reserve(glowBlendableOverrides.size());

	for (const GlowBlendableOverride& overrideEntry : glowBlendableOverrides)
	{
		if (!overrideEntry.owner || !SDK::UKismetSystemLibrary::IsValid(static_cast<SDK::UObject*>(overrideEntry.owner)))
			continue;

		SDK::TArray<SDK::FWeightedBlendable>* blendables = GetBlendablesForOwner(overrideEntry.ownerType, overrideEntry.owner);
		if (!blendables)
			continue;
		if (overrideEntry.index < 0 || overrideEntry.index >= blendables->Num())
			continue;

		SDK::FWeightedBlendable& blendable = (*blendables)[overrideEntry.index];
		if (blendable.Object != overrideEntry.overrideObject)
			continue;
		if (!overrideEntry.overrideObject)
			continue;

		if (colorChanged)
			ApplyGlowColorToMaterial(overrideEntry.overrideObject, primaryGlowColor, secondaryGlowColor);
		activeOverrides.push_back(overrideEntry);
	}

	glowBlendableOverrides.swap(activeOverrides);
	if (!glowBlendableOverrides.empty())
		return;

	if (!shouldScan)
		return;

	for (int i = 0; i < currentLevel->Actors.Num(); i++)
	{
		SDK::AActor* actor = currentLevel->Actors[i];
		if (!actor || Fns::IsNullPointer(actor))
			continue;
		if (!actor->IsA(SDK::APostProcessVolume::StaticClass()))
			continue;

		auto* postProcessVolume = static_cast<SDK::APostProcessVolume*>(actor);
		SDK::TArray<SDK::FWeightedBlendable>& blendables = postProcessVolume->Settings.WeightedBlendables.Array;
		if (blendables.Num() == 0)
			continue;

		SDK::FWeightedBlendable& blendable = blendables[0];
		SDK::UObject* blendableObject = blendable.Object;
		if (!blendableObject || !blendableObject->IsA(SDK::UMaterialInterface::StaticClass()))
			continue;

		auto* sourceMaterial = static_cast<SDK::UMaterialInterface*>(blendableObject);
		if (!IsTargetGlowBlendableMaterial(sourceMaterial))
			continue;

		SDK::UMaterialInstanceDynamic* overrideMaterial = CreateGlowOverrideMaterial(sourceMaterial);
		if (!overrideMaterial)
			continue;

		ApplyGlowColorToMaterial(overrideMaterial, primaryGlowColor, secondaryGlowColor);
		glowBlendableOverrides.push_back({
			GlowBlendableOwnerType::PostProcessVolume,
			postProcessVolume,
			0,
			blendableObject,
			overrideMaterial
			});
		blendable.Object = overrideMaterial;
		glowOverrideLevel = currentLevel;
		return;
	}
}

void Esp::RestoreGlowColorOverride()
{
	if (glowBlendableOverrides.empty())
	{
		glowOverrideLevel = nullptr;
		hasLastAppliedGlowColor = false;
		return;
	}

	__try
	{
		for (const GlowBlendableOverride& overrideEntry : glowBlendableOverrides)
		{
			if (!overrideEntry.owner || !SDK::UKismetSystemLibrary::IsValid(static_cast<SDK::UObject*>(overrideEntry.owner)))
				continue;

			SDK::TArray<SDK::FWeightedBlendable>* blendables = GetBlendablesForOwner(overrideEntry.ownerType, overrideEntry.owner);
			if (!blendables)
				continue;
			if (overrideEntry.index < 0 || overrideEntry.index >= blendables->Num())
				continue;

			SDK::FWeightedBlendable& blendable = (*blendables)[overrideEntry.index];
			if (blendable.Object == overrideEntry.overrideObject)
				blendable.Object = overrideEntry.originalObject;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}

	glowBlendableOverrides.clear();
	glowOverrideLevel = nullptr;
	hasLastAppliedGlowColor = false;
}

void Esp::DisableAll()
{
	RestoreGlowColorOverride();
	for (const auto& trackedPrimitiveEntry : trackedGlowPrimitives)
		DisableGlowOnPrimitive(trackedPrimitiveEntry.second);

	auto disableCharacterMeshes = [](const std::vector<SDK::AActor*>& actors)
	{
		for (SDK::AActor* actor : actors)
		{
			if (!actor || !actor->RootComponent || Fns::IsNullPointer(actor))
				continue;

			auto* character = static_cast<SDK::ACharacter*>(actor);
			DisableGlowOnPrimitive(character ? character->Mesh : nullptr);
		}
	};

	for (SDK::AActor* actor : manager->actorRegistry.GetGuards())
	{
		if (!actor || !actor->RootComponent || Fns::IsNullPointer(actor))
			continue;

		auto* guard = static_cast<SDK::ANPC_Guard_C*>(actor);
		DisableGlowOnPrimitive(guard->Mesh);
		DisableGlowOnPrimitive(guard->Hat);
	}

	disableCharacterMeshes(manager->actorRegistry.GetPolice());
	disableCharacterMeshes(manager->actorRegistry.GetPlayers());
	disableCharacterMeshes(manager->actorRegistry.GetRats());

	for (SDK::AActor* actor : manager->actorRegistry.GetCameras())
	{
		if (!actor || !actor->RootComponent || Fns::IsNullPointer(actor))
			continue;

		auto* camera = static_cast<SDK::ACameraBP_C*>(actor);
		DisableGlowOnPrimitive(camera->CameraViewCollision);
		DisableGlowOnPrimitive(camera->CameraHead);
		DisableGlowOnPrimitive(camera->CameraArm);
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
	cachedActorRegistryRevision = 0;
	trackedGlowPrimitives.clear();
	liveBulletPositions.clear();
	bulletTracerSegments.clear();
	glowOverrideLevel = nullptr;
	hasLastAppliedGlowColor = false;
}

void Esp::OnWorldChanged()
{
	RestoreGlowColorOverride();
	prevPoliceEsp = false;
	prevPoliceGlow = false;
	prevPlayerEsp = false;
	prevPlayerGlow = false;
	prevCameraEsp = false;
	prevCameraGlow = false;
	prevRatEsp = false;
	prevRatGlow = false;
	prevFilterDormant = true;
	espFrameCounter = 0;
	cachedEspActors.clear();
	cachedActorRegistryRevision = 0;
	trackedGlowPrimitives.clear();
	liveBulletPositions.clear();
	bulletTracerSegments.clear();
}
