#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../Libs/UEDump/SDK.hpp"
#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"

class Esp
{
public:
	void Tick();
	void RenderOverlay();
	bool NeedsOverlayRender() const;
	void DisableAll();
	void OnWorldChanged();

	enum class TrackedActorType
	{
		Guard,
		Police,
		Player,
		Camera,
		Rat
	};

	struct BulletTracerSegment
	{
		SDK::FVector start{};
		SDK::FVector end{};
		float createdAt{ 0.0f };
	};

	struct CachedEspActor
	{
		SDK::AActor* actor{};
		TrackedActorType type{};
	};

	enum class GlowBlendableOwnerType
	{
		Camera,
		PostProcessVolume
	};

	struct GlowBlendableOverride
	{
		GlowBlendableOwnerType ownerType{};
		SDK::UObject* owner{};
		int index{};
		SDK::UObject* originalObject{};
		SDK::UMaterialInstanceDynamic* overrideObject{};
	};

private:
	void RefreshEspActorCache(bool forceRefresh, bool trackPolice, bool trackPlayers, bool trackCameras, bool trackRats);
	void ApplyGlow();
	void ApplyGlowColorOverride();
	void RestoreGlowColorOverride();
	void TrackGlowPrimitive(SDK::UPrimitiveComponent* component);
	void UpdateBulletTracers();
	void RenderBulletTracers();
	void RenderEntityBoxes();

	void RenderFovCircle();

	bool prevPoliceEsp{ false };
	bool prevPoliceGlow{ false };
	bool prevPlayerEsp{ false };
	bool prevPlayerGlow{ false };
	bool prevCameraEsp{ false };
	bool prevCameraGlow{ false };
	bool prevRatEsp{ false };
	bool prevRatGlow{ false };
	bool prevFilterDormant{ true };
	int espFrameCounter{ 0 };
	std::uint64_t cachedActorRegistryRevision{ 0 };
	std::vector<CachedEspActor> cachedEspActors{};
	std::unordered_map<std::int32_t, SDK::UPrimitiveComponent*> trackedGlowPrimitives{};
	std::unordered_map<std::int32_t, SDK::FVector> liveBulletPositions{};
	std::vector<BulletTracerSegment> bulletTracerSegments{};
	std::vector<GlowBlendableOverride> glowBlendableOverrides{};
	SDK::ULevel* glowOverrideLevel{ nullptr };
	bool hasLastAppliedGlowColor{ false };
	SDK::FLinearColor lastAppliedGlowColor{};
	SDK::FLinearColor lastAppliedGlowSecondaryColor{};
};
