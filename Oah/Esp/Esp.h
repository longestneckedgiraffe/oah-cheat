#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/Config.h"
#include "../Libs/UEDump/SDK.hpp"
#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"

inline bool IsPoliceEspActive(const Config& config)
{
	return config.esp.policeGlowEnabled || config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled || config.esp.policeNameEnabled;
}

inline bool IsPlayerEspActive(const Config& config)
{
	return config.esp.playerGlowEnabled || config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled || config.esp.playerNameEnabled;
}

inline bool IsCameraEspActive(const Config& config)
{
	return config.esp.cameraGlowEnabled || config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled || config.esp.cameraNameEnabled;
}

inline bool IsRatEspActive(const Config& config)
{
	return config.esp.ratGlowEnabled || config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled || config.esp.ratNameEnabled;
}

inline bool HasAnyEspOverlayEnabled(const Config& config)
{
	return
		(IsPoliceEspActive(config) && (config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled || config.esp.policeNameEnabled)) ||
		(IsPlayerEspActive(config) && (config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled || config.esp.playerNameEnabled)) ||
		(IsCameraEspActive(config) && (config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled || config.esp.cameraNameEnabled)) ||
		(IsRatEspActive(config) && (config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled || config.esp.ratNameEnabled));
}

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
		std::string name{};
		float nameWidth{ 0.0f };
		float nameHeight{ 0.0f };
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
	void ApplyGlowColorOverride(bool shouldScan);
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
