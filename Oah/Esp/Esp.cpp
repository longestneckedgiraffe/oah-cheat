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

	bool IsGuardActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::ANPC_Guard_C::StaticName());
	}

	bool IsPoliceActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::ANPC_Police_base_C::StaticName());
	}

	bool IsPlayerActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::APlayerCharacter_C::StaticName());
	}

	bool IsCameraActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::ACameraBP_C::StaticName());
	}

	bool IsRatActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::ARatCharacter_C::StaticName());
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

	void DestroyCameraProxyPrimitive(SDK::UStaticMeshComponent*& component)
	{
		if (!component)
			return;

		__try
		{
			if (SDK::UKismetSystemLibrary::IsValid(component))
			{
				component->SetRenderCustomDepth(false);
				component->bRenderInDepthPass = false;
				component->SetRenderInMainPass(false);
				component->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);

				if (SDK::AActor* owner = component->GetOwner())
					owner->K2_DestroyComponent(component);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}

		component = nullptr;
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

SDK::UStaticMeshComponent* Esp::CreateCameraProxyMesh(SDK::ACameraBP_C* camera, SDK::UStaticMeshComponent* source, int stencilValue)
{
	if (!camera || !source)
		return nullptr;

	if (!SDK::UKismetSystemLibrary::IsValid(camera) || !SDK::UKismetSystemLibrary::IsValid(source))
		return nullptr;

	SDK::FTransform identity{};
	identity.Rotation.X = 0.f;
	identity.Rotation.Y = 0.f;
	identity.Rotation.Z = 0.f;
	identity.Rotation.W = 1.f;
	identity.Translation.X = 0.f;
	identity.Translation.Y = 0.f;
	identity.Translation.Z = 0.f;
	identity.Scale3D.X = 1.f;
	identity.Scale3D.Y = 1.f;
	identity.Scale3D.Z = 1.f;

	auto* proxy = static_cast<SDK::UStaticMeshComponent*>(
		camera->AddComponentByClass(
			SDK::UStaticMeshComponent::StaticClass(),
			true,
			identity,
			true
		)
	);

	if (!proxy)
		return nullptr;

	proxy->SetStaticMesh(source->StaticMesh);

	const int numMats = source->GetNumMaterials();
	for (int i = 0; i < numMats; i++)
	{
		SDK::UMaterialInterface* mat = source->GetMaterial(i);
		if (mat)
			proxy->SetMaterial(i, mat);
	}

	proxy->SetRenderInMainPass(false);
	proxy->bRenderInDepthPass = false;
	proxy->SetRenderCustomDepth(true);
	proxy->SetCustomDepthStencilValue(stencilValue);
	proxy->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);
	proxy->SetCastShadow(false);
	proxy->SetReceivesDecals(false);

	camera->FinishAddComponent(proxy, true, identity);

	static SDK::FName emptySocket{};
	proxy->K2_AttachToComponent(
		source,
		emptySocket,
		SDK::EAttachmentRule::SnapToTarget,
		SDK::EAttachmentRule::SnapToTarget,
		SDK::EAttachmentRule::SnapToTarget,
		false
	);

	return proxy;
}

void Esp::EnsureCameraProxies(SDK::ACameraBP_C* camera, bool enabling, int stencilValue)
{
	if (!camera)
		return;

	const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(camera);

	if (!enabling)
	{
		auto it = cameraProxies.find(key);
		if (it != cameraProxies.end())
		{
			DestroyCameraProxyPrimitive(it->second.head);
			DestroyCameraProxyPrimitive(it->second.arm);
			cameraProxies.erase(it);
		}
		return;
	}

	CameraProxyMeshes& proxies = cameraProxies[key];

	if (!proxies.head || !SDK::UKismetSystemLibrary::IsValid(proxies.head))
	{
		if (camera->CameraHead && SDK::UKismetSystemLibrary::IsValid(camera->CameraHead))
			proxies.head = CreateCameraProxyMesh(camera, camera->CameraHead, stencilValue);
	}

	if (!proxies.arm || !SDK::UKismetSystemLibrary::IsValid(proxies.arm))
	{
		if (camera->CameraArm && SDK::UKismetSystemLibrary::IsValid(camera->CameraArm))
			proxies.arm = CreateCameraProxyMesh(camera, camera->CameraArm, stencilValue);
	}

	if (proxies.head && SDK::UKismetSystemLibrary::IsValid(proxies.head))
		ApplyGlowToPrimitive(proxies.head, true, stencilValue);

	if (proxies.arm && SDK::UKismetSystemLibrary::IsValid(proxies.arm))
		ApplyGlowToPrimitive(proxies.arm, true, stencilValue);
}

void Esp::CleanupAllCameraProxies()
{
	for (auto& entry : cameraProxies)
	{
		DestroyCameraProxyPrimitive(entry.second.head);
		DestroyCameraProxyPrimitive(entry.second.arm);
	}
	cameraProxies.clear();
}

void Esp::RefreshEspActorCache(bool forceRefresh, bool trackPolice, bool trackPlayers, bool trackCameras, bool trackRats)
{
	if (!trackPolice && !trackPlayers && !trackCameras && !trackRats)
	{
		CleanupAllCameraProxies();
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

	if (!Vars::World || Vars::World->Levels.Num() == 0)
	{
		CleanupAllCameraProxies();
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
	{
		CleanupAllCameraProxies();
		cachedEspActors.clear();
		cachedEspLevel = nullptr;
		actorCacheFrameCounter = 0;
		return;
	}

	actorCacheFrameCounter++;
	const bool levelChanged = cachedEspLevel != currLevel;
	const bool refreshNow = forceRefresh || levelChanged || actorCacheFrameCounter >= 120;

	if (!refreshNow)
		return;

	if (levelChanged)
		CleanupAllCameraProxies();

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

		if (trackPolice && IsGuardActor(currActor))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Guard });
		}
		else if (trackPolice && IsPoliceActor(currActor))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Police });
		}
		else if (trackPlayers && IsPlayerActor(currActor))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Player });
		}
		else if (trackCameras && IsCameraActor(currActor))
		{
			cachedEspActors.push_back({ currActor, TrackedActorType::Camera });
		}
		else if (trackRats && IsRatActor(currActor))
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
		RefreshEspActorCache(stateChanged, trackPolice, trackPlayers, trackCameras, trackRats);
	else
		RefreshEspActorCache(false, false, false, false, false);

	if (anyGlowEnabled)
		ApplyGlowColorOverride();
	else
		RestoreGlowColorOverride();

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
			if (Fns::IsBadPoint(currActor) || !SDK::UKismetSystemLibrary::IsValid(currActor))
				continue;

			if (cachedActor.type == TrackedActorType::Guard)
			{
				auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
				bool enabling = IsPoliceEspActive(config) &&
					config.esp.policeGlowEnabled &&
					!(filterDormant && guard->Dead_);
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
				DisableGlowOnPrimitive(camera->CameraViewCollision);
				DisableGlowOnPrimitive(camera->CameraHead);
				DisableGlowOnPrimitive(camera->CameraArm);
				EnsureCameraProxies(camera, enabling, kGlowStencilValue);
			}
			else if (cachedActor.type == TrackedActorType::Player)
			{
				auto* character = static_cast<SDK::ACharacter*>(currActor);
				bool isLocalPlayer = currActor == Vars::CharacterClass || currActor->GetOwner() == Vars::MyController;
				bool enabling = IsPlayerEspActive(config) &&
					config.esp.playerGlowEnabled &&
					!isLocalPlayer;
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

void Esp::ApplyGlowColorOverride()
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

	for (int i = 0; i < currentLevel->Actors.Num(); i++)
	{
		SDK::AActor* actor = currentLevel->Actors[i];
		if (!actor || Fns::IsBadPoint(actor))
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
			DisableGlowOnPrimitive(guard->Mesh);
			DisableGlowOnPrimitive(guard->Hat);
		}
		else if (fullName.find("NPC_Police") != std::string::npos ||
			fullName.find("PlayerCharacter") != std::string::npos ||
			fullName.find("RatCharacter") != std::string::npos)
		{
			auto* character = static_cast<SDK::ACharacter*>(currActor);
			DisableGlowOnPrimitive(character->Mesh);
		}

		if (fullName.find("CameraBP") != std::string::npos)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			DisableGlowOnPrimitive(camera->CameraViewCollision);
			DisableGlowOnPrimitive(camera->CameraHead);
			DisableGlowOnPrimitive(camera->CameraArm);
		}
	}

	CleanupAllCameraProxies();

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
	glowOverrideLevel = nullptr;
	hasLastAppliedGlowColor = false;
}
