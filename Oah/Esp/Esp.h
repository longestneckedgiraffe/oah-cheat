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

	struct CameraProxyMeshes
	{
		SDK::UStaticMeshComponent* head{};
		SDK::UStaticMeshComponent* arm{};
	};

private:
	void RefreshEspActorCache(bool forceRefresh, bool trackPolice, bool trackPlayers, bool trackCameras, bool trackRats);
	void ApplyGlow();
	void ApplyGlowColorOverride();
	void RestoreGlowColorOverride();
	void UpdateBulletTracers();
	void RenderBulletTracers();
	void RenderEntityBoxes();

	void RenderFovCircle();

	SDK::UStaticMeshComponent* CreateCameraProxyMesh(SDK::ACameraBP_C* camera, SDK::UStaticMeshComponent* source, int stencilValue);
	void EnsureCameraProxies(SDK::ACameraBP_C* camera, bool enabling, int stencilValue);
	void CleanupAllCameraProxies();

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
	std::unordered_map<std::uintptr_t, SDK::FVector> liveBulletPositions{};
	std::vector<BulletTracerSegment> bulletTracerSegments{};
	std::vector<GlowBlendableOverride> glowBlendableOverrides{};
	SDK::ULevel* glowOverrideLevel{ nullptr };
	bool hasLastAppliedGlowColor{ false };
	SDK::FLinearColor lastAppliedGlowColor{};
	SDK::FLinearColor lastAppliedGlowSecondaryColor{};

	std::unordered_map<std::uintptr_t, CameraProxyMeshes> cameraProxies{};
};
