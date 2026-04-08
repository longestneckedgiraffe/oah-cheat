#pragma once

#include <unordered_map>
#include <vector>

#include "../Libs/UEDump/SDK.hpp"

class Esp
{
public:
	void RenderESP();
	void DisableAll();

	enum class TrackedActorType
	{
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

private:
	void RefreshEspActorCache(bool forceRefresh = false);
	void ApplyGlow();
	void UpdateBulletTracers();
	void RenderBulletTracers();
	void RenderEntityBoxes();

	void RenderFovCircle();
	void RenderDebugESP();

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
	int actorCacheFrameCounter{ 0 };
	SDK::ULevel* cachedEspLevel{ nullptr };
	std::vector<CachedEspActor> cachedEspActors{};
	std::unordered_map<std::uintptr_t, SDK::FVector> liveBulletPositions{};
	std::vector<BulletTracerSegment> bulletTracerSegments{};
};
