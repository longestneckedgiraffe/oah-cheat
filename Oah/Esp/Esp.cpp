#include "../Core/Manager.h"
#include "Esp.h"

#include <cstdio>
#include <iostream>
#include <string>
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

	void GlowDebugLog(const std::string& message)
	{
		std::cout << "[Glow] " << message << std::endl;
	}

	std::string FormatLinearColor(const SDK::FLinearColor& color)
	{
		char buffer[96];
		std::snprintf(buffer, sizeof(buffer), "(R=%.3f G=%.3f B=%.3f A=%.3f)",
			color.R, color.G, color.B, color.A);
		return buffer;
	}

	bool SafeIsValid(SDK::UObject* object)
	{
		if (!object)
			return false;
		__try
		{
			return SDK::UKismetSystemLibrary::IsValid(object);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	std::string SafeObjectName(SDK::UObject* object)
	{
		if (!object)
			return "<null>";
		if (!SafeIsValid(object))
			return "<invalid>";
		return object->GetName();
	}

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
		{
			GlowDebugLog("ApplyGlowColorToMaterial: null material, skipping");
			return;
		}

		static SDK::FName colorName{};
		static SDK::FName color1Name{};

		material->SetVectorParameterValue(SDK::GetStaticName(L"Color", colorName), primaryColor);
		material->SetVectorParameterValue(SDK::GetStaticName(L"Color1", color1Name), secondaryColor);

		GlowDebugLog("ApplyGlowColorToMaterial: set Color=" + FormatLinearColor(primaryColor) +
			" Color1=" + FormatLinearColor(secondaryColor) +
			" on '" + SafeObjectName(material) + "'");
	}

	SDK::UMaterialInstanceDynamic* CreateGlowOverrideMaterial(SDK::UMaterialInterface* sourceMaterial)
	{
		if (!Vars::World || !sourceMaterial)
		{
			GlowDebugLog(std::string("CreateGlowOverrideMaterial: aborting (world=") +
				(Vars::World ? "ok" : "null") +
				" sourceMaterial=" + (sourceMaterial ? "ok" : "null") + ")");
			return nullptr;
		}

		SDK::UMaterialInstanceDynamic* dynamicMaterial =
			SDK::UKismetMaterialLibrary::CreateDynamicMaterialInstance(
				Vars::World,
				sourceMaterial,
				SDK::FName{},
				SDK::EMIDCreationFlags::Transient);

		if (!dynamicMaterial)
		{
			GlowDebugLog("CreateGlowOverrideMaterial: CreateDynamicMaterialInstance returned null for '" +
				SafeObjectName(sourceMaterial) + "'");
			return nullptr;
		}

		dynamicMaterial->K2_CopyMaterialInstanceParameters(sourceMaterial, false);
		GlowDebugLog("CreateGlowOverrideMaterial: created MID '" + SafeObjectName(dynamicMaterial) +
			"' from source '" + SafeObjectName(sourceMaterial) + "'");
		return dynamicMaterial;
	}

	bool RestoreGlowBlendablesSafely(
		const Esp::GlowBlendableOverride* entries,
		size_t entryCount,
		size_t& restoredCount,
		size_t& invalidOwnerCount,
		size_t& mismatchCount)
	{
		__try
		{
			for (size_t i = 0; i < entryCount; i++)
			{
				const Esp::GlowBlendableOverride& overrideEntry = entries[i];

				if (!overrideEntry.owner || !SDK::UKismetSystemLibrary::IsValid(static_cast<SDK::UObject*>(overrideEntry.owner)))
				{
					invalidOwnerCount++;
					continue;
				}

				SDK::TArray<SDK::FWeightedBlendable>* blendables = GetBlendablesForOwner(overrideEntry.ownerType, overrideEntry.owner);
				if (!blendables)
					continue;
				if (overrideEntry.index < 0 || overrideEntry.index >= blendables->Num())
					continue;

				SDK::FWeightedBlendable& blendable = (*blendables)[overrideEntry.index];
				if (blendable.Object == overrideEntry.overrideObject)
				{
					blendable.Object = overrideEntry.originalObject;
					restoredCount++;
				}
				else
				{
					mismatchCount++;
				}
			}
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
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
	{
		GlowDebugLog("ApplyGlowColorOverride: level changed, restoring previous overrides");
		RestoreGlowColorOverride();
	}

	SDK::FLinearColor primaryGlowColor{};
	SDK::FLinearColor secondaryGlowColor{};
	GetGlowColors(*manager->pConfig, primaryGlowColor, secondaryGlowColor);

	const bool colorChanged =
		!hasLastAppliedGlowColor ||
		!AreSameColor(lastAppliedGlowColor, primaryGlowColor) ||
		!AreSameColor(lastAppliedGlowSecondaryColor, secondaryGlowColor);
	if (colorChanged)
	{
		GlowDebugLog("ApplyGlowColorOverride: color changed -> " + FormatLinearColor(primaryGlowColor));
		lastAppliedGlowColor = primaryGlowColor;
		lastAppliedGlowSecondaryColor = secondaryGlowColor;
		hasLastAppliedGlowColor = true;
	}

	std::vector<GlowBlendableOverride> activeOverrides{};
	activeOverrides.reserve(glowBlendableOverrides.size());
	const size_t priorOverrideCount = glowBlendableOverrides.size();
	size_t droppedOwnerInvalid = 0;
	size_t droppedBlendablesNull = 0;
	size_t droppedIndexOutOfRange = 0;
	size_t droppedReplaced = 0;

	for (const GlowBlendableOverride& overrideEntry : glowBlendableOverrides)
	{
		if (!overrideEntry.owner || !SDK::UKismetSystemLibrary::IsValid(static_cast<SDK::UObject*>(overrideEntry.owner)))
		{
			droppedOwnerInvalid++;
			continue;
		}

		SDK::TArray<SDK::FWeightedBlendable>* blendables = GetBlendablesForOwner(overrideEntry.ownerType, overrideEntry.owner);
		if (!blendables)
		{
			droppedBlendablesNull++;
			continue;
		}
		if (overrideEntry.index < 0 || overrideEntry.index >= blendables->Num())
		{
			droppedIndexOutOfRange++;
			continue;
		}

		SDK::FWeightedBlendable& blendable = (*blendables)[overrideEntry.index];
		if (blendable.Object != overrideEntry.overrideObject)
		{
			droppedReplaced++;
			continue;
		}
		if (!overrideEntry.overrideObject)
			continue;

		if (colorChanged)
			ApplyGlowColorToMaterial(overrideEntry.overrideObject, primaryGlowColor, secondaryGlowColor);
		activeOverrides.push_back(overrideEntry);
	}

	const size_t afterCount = activeOverrides.size();
	if (priorOverrideCount != afterCount)
	{
		char buffer[256];
		std::snprintf(buffer, sizeof(buffer),
			"ApplyGlowColorOverride: pruned overrides %zu -> %zu (owner_invalid=%zu blendables_null=%zu index_oor=%zu replaced=%zu)",
			priorOverrideCount, afterCount,
			droppedOwnerInvalid, droppedBlendablesNull, droppedIndexOutOfRange, droppedReplaced);
		GlowDebugLog(buffer);
	}

	glowBlendableOverrides.swap(activeOverrides);
	if (!glowBlendableOverrides.empty())
		return;

	if (!shouldScan)
		return;

	GlowDebugLog("ApplyGlowColorOverride: scanning level for PostProcessVolume with HighlightMat_Inst blendable (actors=" +
		std::to_string(currentLevel->Actors.Num()) + ")");

	int postProcessVolumeCount = 0;
	int volumesWithBlendables = 0;
	int materialBlendables = 0;
	int targetMaterialMatches = 0;
	std::string firstNonMatchName;

	for (int i = 0; i < currentLevel->Actors.Num(); i++)
	{
		SDK::AActor* actor = currentLevel->Actors[i];
		if (!actor || Fns::IsNullPointer(actor))
			continue;
		if (!actor->IsA(SDK::APostProcessVolume::StaticClass()))
			continue;

		postProcessVolumeCount++;
		auto* postProcessVolume = static_cast<SDK::APostProcessVolume*>(actor);
		SDK::TArray<SDK::FWeightedBlendable>& blendables = postProcessVolume->Settings.WeightedBlendables.Array;
		if (blendables.Num() == 0)
			continue;

		volumesWithBlendables++;
		SDK::FWeightedBlendable& blendable = blendables[0];
		SDK::UObject* blendableObject = blendable.Object;
		if (!blendableObject || !blendableObject->IsA(SDK::UMaterialInterface::StaticClass()))
			continue;

		materialBlendables++;
		auto* sourceMaterial = static_cast<SDK::UMaterialInterface*>(blendableObject);
		if (!IsTargetGlowBlendableMaterial(sourceMaterial))
		{
			if (firstNonMatchName.empty())
				firstNonMatchName = SafeObjectName(sourceMaterial);
			continue;
		}

		targetMaterialMatches++;
		GlowDebugLog("ApplyGlowColorOverride: found target material '" + SafeObjectName(sourceMaterial) +
			"' on PostProcessVolume '" + SafeObjectName(postProcessVolume) + "'");

		SDK::UMaterialInstanceDynamic* overrideMaterial = CreateGlowOverrideMaterial(sourceMaterial);
		if (!overrideMaterial)
		{
			GlowDebugLog("ApplyGlowColorOverride: CreateGlowOverrideMaterial failed, skipping this volume");
			continue;
		}

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
		GlowDebugLog("ApplyGlowColorOverride: hook installed (blendables[0] swapped to override MID); scan summary ppv=" +
			std::to_string(postProcessVolumeCount) + " w/blendables=" + std::to_string(volumesWithBlendables) +
			" w/material=" + std::to_string(materialBlendables));
		return;
	}

	char summary[256];
	std::snprintf(summary, sizeof(summary),
		"ApplyGlowColorOverride: scan found no target (ppv=%d w/blendables=%d w/material=%d matches=%d first_nonmatch='%s')",
		postProcessVolumeCount, volumesWithBlendables, materialBlendables, targetMaterialMatches,
		firstNonMatchName.empty() ? "<none>" : firstNonMatchName.c_str());
	GlowDebugLog(summary);
}

void Esp::RestoreGlowColorOverride()
{
	if (glowBlendableOverrides.empty())
	{
		glowOverrideLevel = nullptr;
		hasLastAppliedGlowColor = false;
		return;
	}

	GlowDebugLog("RestoreGlowColorOverride: restoring " + std::to_string(glowBlendableOverrides.size()) + " override(s)");

	size_t restoredCount = 0;
	size_t invalidOwnerCount = 0;
	size_t mismatchCount = 0;

	const bool restoreCompleted = RestoreGlowBlendablesSafely(
		glowBlendableOverrides.data(),
		glowBlendableOverrides.size(),
		restoredCount,
		invalidOwnerCount,
		mismatchCount);

	if (!restoreCompleted)
		GlowDebugLog("RestoreGlowColorOverride: exception during restore loop");

	char summary[192];
	std::snprintf(summary, sizeof(summary),
		"RestoreGlowColorOverride: restored=%zu invalid_owner=%zu already_replaced=%zu",
		restoredCount, invalidOwnerCount, mismatchCount);
	GlowDebugLog(summary);

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
