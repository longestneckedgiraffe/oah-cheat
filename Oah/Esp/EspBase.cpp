#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <unordered_set>

#include "../Core/Manager.h"
#include "Esp.h"

#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/PlayerCharacter_classes.hpp"
#include "../Libs/UEDump/SDK/RatCharacter_classes.hpp"

static bool TryGetSceneComponentBounds(SDK::USceneComponent* component, SDK::FVector& origin, SDK::FVector& extent);

namespace
{
	static constexpr SDK::ETraceTypeQuery kVisibilityTraceChannel = SDK::ETraceTypeQuery::TraceTypeQuery1;

	bool IsBulletTraceActor(SDK::AActor* actor)
	{
		return actor && actor->IsA(SDK::ABulletTrace_C::StaticClass());
	}

	bool IsPoliceEspActive(const Config& config)
	{
		return config.esp.policeGlowEnabled || config.esp.policeBox2DEnabled || config.esp.policeBox3DEnabled || config.esp.policeNameEnabled;
	}

	bool IsPlayerEspActive(const Config& config)
	{
		return config.esp.playerGlowEnabled || config.esp.playerBox2DEnabled || config.esp.playerBox3DEnabled || config.esp.playerNameEnabled;
	}

	bool IsCameraEspActive(const Config& config)
	{
		return config.esp.cameraGlowEnabled || config.esp.cameraBox2DEnabled || config.esp.cameraBox3DEnabled || config.esp.cameraNameEnabled;
	}

	bool IsRatEspActive(const Config& config)
	{
		return config.esp.ratGlowEnabled || config.esp.ratBox2DEnabled || config.esp.ratBox3DEnabled || config.esp.ratNameEnabled;
	}

	ImU32 ToImU32(const float color[4])
	{
		return ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
	}

	SDK::FVector GetCurrentViewLocation()
	{
		if (Vars::MyController && Vars::MyController->PlayerCameraManager)
			return Vars::MyController->PlayerCameraManager->GetCameraLocation();
		if (Vars::MyPawn)
			return Vars::MyPawn->K2_GetActorLocation();
		return {};
	}

	bool IsProjectedOnScreen(const SDK::FVector& world)
	{
		if (!Vars::MyController)
			return false;

		SDK::FVector2D screenPos{};
		if (!Vars::MyController->ProjectWorldLocationToScreen(world, &screenPos, false))
			return false;

		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		return
			screenPos.X >= 0.0f &&
			screenPos.Y >= 0.0f &&
			screenPos.X <= displaySize.x &&
			screenPos.Y <= displaySize.y;
	}

	bool AppendCameraVisibilitySamplePoints(
		SDK::USceneComponent* component,
		std::array<SDK::FVector, 18>& samplePoints,
		size_t& sampleCount)
	{
		SDK::FVector origin{};
		SDK::FVector extent{};
		if (!TryGetSceneComponentBounds(component, origin, extent))
			return false;

		if (sampleCount < samplePoints.size())
			samplePoints[sampleCount++] = origin;
		const SDK::FVector offsets[8] = {
			{ extent.X,  extent.Y,  extent.Z },
			{ extent.X,  extent.Y, -extent.Z },
			{ extent.X, -extent.Y,  extent.Z },
			{ extent.X, -extent.Y, -extent.Z },
			{-extent.X,  extent.Y,  extent.Z },
			{-extent.X,  extent.Y, -extent.Z },
			{-extent.X, -extent.Y,  extent.Z },
			{-extent.X, -extent.Y, -extent.Z },
		};

		for (const SDK::FVector& offset : offsets)
		{
			if (sampleCount >= samplePoints.size())
				break;
			if (offset.X == 0.0f && offset.Y == 0.0f && offset.Z == 0.0f)
				continue;
			samplePoints[sampleCount++] = origin + offset;
		}

		return true;
	}

	bool IsCameraTraceHit(SDK::ACameraBP_C* camera, const SDK::FHitResult& hit)
	{
		if (!camera)
			return false;

		return
			hit.Actor.Get() == camera ||
			hit.Component.Get() == camera->CameraHead ||
			hit.Component.Get() == camera->CameraArm;
	}

	bool IsCameraPointVisible(SDK::ACameraBP_C* camera, const SDK::FVector& targetPoint)
	{
		if (!Vars::World || !Vars::MyController)
			return false;

		const SDK::FVector viewLocation = GetCurrentViewLocation();
		const SDK::FVector delta = targetPoint - viewLocation;
		const float distanceSq = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
		if (distanceSq <= 1.0f)
			return true;

		SDK::AActor* ignoredActorsBuffer[2]{};
		SDK::TArray<SDK::AActor*> ignoredActors(ignoredActorsBuffer, 0, 2);
		if (Vars::MyPawn)
			ignoredActors.Add(Vars::MyPawn);
		if (SDK::AActor* viewTarget = Vars::MyController->GetViewTarget(); viewTarget && viewTarget != Vars::MyPawn)
			ignoredActors.Add(viewTarget);

		SDK::FHitResult hit{};
		const bool hasBlockingHit = SDK::UKismetSystemLibrary::LineTraceSingle(
			Vars::World,
			viewLocation,
			targetPoint,
			kVisibilityTraceChannel,
			true,
			ignoredActors,
			SDK::EDrawDebugTrace::None,
			&hit,
			false,
			SDK::FLinearColor{},
			SDK::FLinearColor{},
			0.0f);

		return !hasBlockingHit || IsCameraTraceHit(camera, hit);
	}

	bool IsCameraProjectedOnScreen(SDK::ACameraBP_C* camera)
	{
		if (!camera)
			return false;

		std::array<SDK::FVector, 18> samplePoints{};
		size_t sampleCount = 0;
		bool foundComponent = false;

		foundComponent |= AppendCameraVisibilitySamplePoints(camera->CameraHead, samplePoints, sampleCount);
		foundComponent |= AppendCameraVisibilitySamplePoints(camera->CameraArm, samplePoints, sampleCount);

		for (size_t i = 0; i < sampleCount; ++i)
		{
			if (IsProjectedOnScreen(samplePoints[i]))
				return true;
		}

		if (!foundComponent)
			return IsProjectedOnScreen(camera->K2_GetActorLocation());

		return false;
	}

	bool IsCameraVisibleForBoxes(const Config&, SDK::ACameraBP_C* camera)
	{
		if (!camera || !Vars::MyController)
			return false;

		std::array<SDK::FVector, 18> samplePoints{};
		size_t sampleCount = 0;
		bool foundComponent = false;

		foundComponent |= AppendCameraVisibilitySamplePoints(camera->CameraHead, samplePoints, sampleCount);
		foundComponent |= AppendCameraVisibilitySamplePoints(camera->CameraArm, samplePoints, sampleCount);

		if (!foundComponent)
			return IsProjectedOnScreen(camera->K2_GetActorLocation()) && IsCameraPointVisible(camera, camera->K2_GetActorLocation());

		for (size_t i = 0; i < sampleCount; ++i)
		{
			if (!IsProjectedOnScreen(samplePoints[i]))
				continue;
			if (IsCameraPointVisible(camera, samplePoints[i]))
				return true;
		}

		return false;
	}

	ImU32 GetBoxColor(const Config& config, SDK::APlayerController* controller, SDK::AActor* actor, const SDK::FVector& viewPoint)
	{
		if (!config.esp.visibilityCheckEnabled || !controller || !actor)
			return ToImU32(config.esp.defaultBoxColor);

		const bool isVisible =
			actor->IsA(SDK::ACameraBP_C::StaticName())
			? IsCameraVisibleForBoxes(config, static_cast<SDK::ACameraBP_C*>(actor))
			: controller->LineOfSightTo(actor, viewPoint, false);

		return isVisible
			? ToImU32(config.esp.visibleBoxColor)
			: ToImU32(config.esp.hiddenBoxColor);
	}
}

static bool WorldToScreen(const SDK::FVector& world, ImVec2& screen)
{
	if (!Vars::MyController)
		return false;

	SDK::FVector2D screenPos;
	bool result = Vars::MyController->ProjectWorldLocationToScreen(world, &screenPos, false);
	if (!result)
		return false;

	screen.x = screenPos.X;
	screen.y = screenPos.Y;
	return screen.x > 0 && screen.y > 0 &&
		screen.x < ImGui::GetIO().DisplaySize.x &&
		screen.y < ImGui::GetIO().DisplaySize.y;
}

static bool GetProjectedBounds(
	const SDK::FVector& origin,
	const SDK::FVector& extent,
	std::array<ImVec2, 8>& screenCorners,
	ImVec2& minPoint,
	ImVec2& maxPoint)
{
	if ((extent.X == 0.0f && extent.Y == 0.0f && extent.Z == 0.0f) ||
		(origin.X == 0.0f && origin.Y == 0.0f && origin.Z == 0.0f))
	{
		return false;
	}

	const SDK::FVector corners[8] = {
		{ origin.X - extent.X, origin.Y - extent.Y, origin.Z - extent.Z },
		{ origin.X + extent.X, origin.Y - extent.Y, origin.Z - extent.Z },
		{ origin.X + extent.X, origin.Y + extent.Y, origin.Z - extent.Z },
		{ origin.X - extent.X, origin.Y + extent.Y, origin.Z - extent.Z },
		{ origin.X - extent.X, origin.Y - extent.Y, origin.Z + extent.Z },
		{ origin.X + extent.X, origin.Y - extent.Y, origin.Z + extent.Z },
		{ origin.X + extent.X, origin.Y + extent.Y, origin.Z + extent.Z },
		{ origin.X - extent.X, origin.Y + extent.Y, origin.Z + extent.Z },
	};

	float minX = FLT_MAX;
	float minY = FLT_MAX;
	float maxX = -FLT_MAX;
	float maxY = -FLT_MAX;

	for (size_t i = 0; i < 8; ++i)
	{
		if (!WorldToScreen(corners[i], screenCorners[i]))
			return false;

		minX = (screenCorners[i].x < minX) ? screenCorners[i].x : minX;
		minY = (screenCorners[i].y < minY) ? screenCorners[i].y : minY;
		maxX = (screenCorners[i].x > maxX) ? screenCorners[i].x : maxX;
		maxY = (screenCorners[i].y > maxY) ? screenCorners[i].y : maxY;
	}

	minPoint = { minX, minY };
	maxPoint = { maxX, maxY };
	return true;
}

static bool TryGetSceneComponentBounds(SDK::USceneComponent* component, SDK::FVector& origin, SDK::FVector& extent)
{
	if (!component)
		return false;

	float sphereRadius = 0.0f;
	SDK::UKismetSystemLibrary::GetComponentBounds(component, &origin, &extent, &sphereRadius);
	return extent.X > 0.0f || extent.Y > 0.0f || extent.Z > 0.0f;
}

static bool GetCharacterCapsuleBounds(SDK::ACharacter* character, SDK::FVector& origin, SDK::FVector& extent)
{
	if (!character || !character->CapsuleComponent)
		return false;

	const float radius = character->CapsuleComponent->GetScaledCapsuleRadius();
	const float halfHeight = character->CapsuleComponent->GetScaledCapsuleHalfHeight();
	if (radius <= 0.0f || halfHeight <= 0.0f)
		return false;

	origin = character->CapsuleComponent->K2_GetComponentLocation();
	extent = { radius, radius, halfHeight };
	return true;
}

static bool GetCharacter2DBox(SDK::ACharacter* character, ImVec2& minPoint, ImVec2& maxPoint)
{
	SDK::FVector origin{};
	SDK::FVector extent{};
	if (!GetCharacterCapsuleBounds(character, origin, extent))
		return false;

	ImVec2 topScreen{};
	ImVec2 bottomScreen{};
	if (!WorldToScreen(origin + SDK::FVector{ 0.0f, 0.0f, extent.Z }, topScreen))
		return false;
	if (!WorldToScreen(origin - SDK::FVector{ 0.0f, 0.0f, extent.Z }, bottomScreen))
		return false;

	const float boxHeight = std::fabs(bottomScreen.y - topScreen.y);
	if (boxHeight < 2.0f)
		return false;

	const float boxWidth = boxHeight * 0.45f;
	const float centerX = (topScreen.x + bottomScreen.x) * 0.5f;
	minPoint = { centerX - boxWidth * 0.5f, (topScreen.y < bottomScreen.y) ? topScreen.y : bottomScreen.y };
	maxPoint = { centerX + boxWidth * 0.5f, (topScreen.y > bottomScreen.y) ? topScreen.y : bottomScreen.y };
	return true;
}

static void ExpandBounds(const SDK::FVector& otherOrigin, const SDK::FVector& otherExtent, SDK::FVector& origin, SDK::FVector& extent)
{
	const SDK::FVector minA = { origin.X - extent.X, origin.Y - extent.Y, origin.Z - extent.Z };
	const SDK::FVector maxA = { origin.X + extent.X, origin.Y + extent.Y, origin.Z + extent.Z };
	const SDK::FVector minB = { otherOrigin.X - otherExtent.X, otherOrigin.Y - otherExtent.Y, otherOrigin.Z - otherExtent.Z };
	const SDK::FVector maxB = { otherOrigin.X + otherExtent.X, otherOrigin.Y + otherExtent.Y, otherOrigin.Z + otherExtent.Z };

	const SDK::FVector mergedMin = {
		(minA.X < minB.X) ? minA.X : minB.X,
		(minA.Y < minB.Y) ? minA.Y : minB.Y,
		(minA.Z < minB.Z) ? minA.Z : minB.Z
	};
	const SDK::FVector mergedMax = {
		(maxA.X > maxB.X) ? maxA.X : maxB.X,
		(maxA.Y > maxB.Y) ? maxA.Y : maxB.Y,
		(maxA.Z > maxB.Z) ? maxA.Z : maxB.Z
	};

	origin = (mergedMin + mergedMax) * 0.5f;
	extent = (mergedMax - mergedMin) * 0.5f;
}

static bool GetVisualBounds(const Esp::CachedEspActor& cachedActor, SDK::FVector& origin, SDK::FVector& extent)
{
	SDK::AActor* actor = cachedActor.actor;
	if (!actor)
		return false;

	if (cachedActor.type == Esp::TrackedActorType::Guard ||
		cachedActor.type == Esp::TrackedActorType::Police ||
		cachedActor.type == Esp::TrackedActorType::Player ||
		cachedActor.type == Esp::TrackedActorType::Rat)
	{
		auto* character = static_cast<SDK::ACharacter*>(actor);
		if (GetCharacterCapsuleBounds(character, origin, extent))
			return true;
		if (character->Mesh && TryGetSceneComponentBounds(character->Mesh, origin, extent))
			return true;
	}
	else if (cachedActor.type == Esp::TrackedActorType::Camera)
	{
		auto* camera = static_cast<SDK::ACameraBP_C*>(actor);
		SDK::FVector headOrigin{};
		SDK::FVector headExtent{};
		SDK::FVector armOrigin{};
		SDK::FVector armExtent{};

		const bool hasHead = camera->CameraHead && TryGetSceneComponentBounds(camera->CameraHead, headOrigin, headExtent);
		const bool hasArm = camera->CameraArm && TryGetSceneComponentBounds(camera->CameraArm, armOrigin, armExtent);

		if (hasHead && hasArm)
		{
			origin = headOrigin;
			extent = headExtent;
			ExpandBounds(armOrigin, armExtent, origin, extent);
			return true;
		}
		if (hasHead)
		{
			origin = headOrigin;
			extent = headExtent;
			return true;
		}
		if (hasArm)
		{
			origin = armOrigin;
			extent = armExtent;
			return true;
		}
	}

	actor->GetActorBounds(false, &origin, &extent, false);
	return extent.X > 0.0f || extent.Y > 0.0f || extent.Z > 0.0f;
}

static constexpr float kOutlineThickness = 1.0f;

static ImU32 GetOutlineColor(ImU32 fillColor)
{
	const float alpha = static_cast<float>((fillColor >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
	return IM_COL32(0, 0, 0, static_cast<int>(alpha * 200.0f));
}

static void Draw3DBox(const std::array<ImVec2, 8>& screenCorners, ImDrawList* drawList, ImU32 color)
{
	static constexpr int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7},
	};

	const ImU32 outlineColor = GetOutlineColor(color);
	for (const auto& edge : edges)
		drawList->AddLine(screenCorners[edge[0]], screenCorners[edge[1]], outlineColor, 1.0f + kOutlineThickness);
	for (const auto& edge : edges)
		drawList->AddLine(screenCorners[edge[0]], screenCorners[edge[1]], color, 1.0f);
}

static constexpr float kBulletTracerLifetime = 1.2f;

void Esp::UpdateBulletTracers()
{
	if (!manager->pConfig->esp.bulletTracersEnabled)
	{
		liveBulletPositions.clear();
		bulletTracerSegments.clear();
		return;
	}
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;

	const float now = static_cast<float>(ImGui::GetTime());
	const float lifetime = kBulletTracerLifetime;

	bulletTracerSegments.erase(
		std::remove_if(
			bulletTracerSegments.begin(),
			bulletTracerSegments.end(),
			[now, lifetime](const BulletTracerSegment& segment)
			{
				return now - segment.createdAt >= lifetime;
			}),
		bulletTracerSegments.end());

	std::unordered_set<std::int32_t> activeBullets;
	activeBullets.reserve(liveBulletPositions.size() + 16);

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsNullPointer(currActor))
			continue;
		if (!IsBulletTraceActor(currActor))
			continue;

		const std::int32_t actorKey = Fns::GetObjectKey(currActor);
		if (actorKey == Fns::InvalidObjectKey)
			continue;

		activeBullets.insert(actorKey);

		const SDK::FVector currentPosition = currActor->K2_GetActorLocation();
		auto found = liveBulletPositions.find(actorKey);
		if (found != liveBulletPositions.end())
		{
			const SDK::FVector delta = currentPosition - found->second;
			const float deltaSq = delta.X * delta.X + delta.Y * delta.Y + delta.Z * delta.Z;
			if (deltaSq > 1.0f)
			{
				bulletTracerSegments.push_back({ found->second, currentPosition, now });
				found->second = currentPosition;
			}
		}
		else
		{
			liveBulletPositions.emplace(actorKey, currentPosition);
		}
	}

	for (auto it = liveBulletPositions.begin(); it != liveBulletPositions.end(); )
	{
		if (!activeBullets.contains(it->first))
			it = liveBulletPositions.erase(it);
		else
			++it;
	}
}

void Esp::RenderBulletTracers()
{
	if (!manager->pConfig->esp.bulletTracersEnabled)
		return;
	if (bulletTracerSegments.empty())
		return;

	const float now = static_cast<float>(ImGui::GetTime());
	const float lifetime = kBulletTracerLifetime;
	auto* drawList = ImGui::GetBackgroundDrawList();

	static constexpr float kTracerThickness = 2.0f;

	for (const BulletTracerSegment& segment : bulletTracerSegments)
	{
		float age = now - segment.createdAt;
		if (age < 0.0f || age >= lifetime)
			continue;

		ImVec2 startScreen{};
		ImVec2 endScreen{};
		if (!WorldToScreen(segment.start, startScreen) || !WorldToScreen(segment.end, endScreen))
			continue;

		float alpha = 1.0f - (age / lifetime);
		ImU32 color = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0f));
		ImU32 outlineColor = IM_COL32(0, 0, 0, static_cast<int>(alpha * 200.0f));
		drawList->AddLine(startScreen, endScreen, outlineColor, kTracerThickness + kOutlineThickness);
		drawList->AddLine(startScreen, endScreen, color, kTracerThickness);
	}
}

void Esp::RenderEntityBoxes()
{
	const bool anyBoxesEnabled =
		(IsPoliceEspActive(*manager->pConfig) && (manager->pConfig->esp.policeBox2DEnabled || manager->pConfig->esp.policeBox3DEnabled || manager->pConfig->esp.policeNameEnabled)) ||
		(IsPlayerEspActive(*manager->pConfig) && (manager->pConfig->esp.playerBox2DEnabled || manager->pConfig->esp.playerBox3DEnabled || manager->pConfig->esp.playerNameEnabled)) ||
		(IsCameraEspActive(*manager->pConfig) && (manager->pConfig->esp.cameraBox2DEnabled || manager->pConfig->esp.cameraBox3DEnabled || manager->pConfig->esp.cameraNameEnabled)) ||
		(IsRatEspActive(*manager->pConfig) && (manager->pConfig->esp.ratBox2DEnabled || manager->pConfig->esp.ratBox3DEnabled || manager->pConfig->esp.ratNameEnabled));

	if (!anyBoxesEnabled)
		return;
	if (!Vars::MyController || !Vars::MyController->PlayerCameraManager)
		return;
	if (cachedEspActors.empty())
		return;

	const Config& config = *manager->pConfig;
	const bool filterDormant = manager->pConfig->settings.filterDormant;
	auto* drawList = ImGui::GetBackgroundDrawList();
	const SDK::FVector cameraLocation = Vars::MyController->PlayerCameraManager->GetCameraLocation();
	static constexpr float kMaxEspDistance = 20000.0f;
	const float maxDistSq = kMaxEspDistance * kMaxEspDistance;

	for (const CachedEspActor& cachedActor : cachedEspActors)
	{
		SDK::AActor* currActor = cachedActor.actor;
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsNullPointer(currActor))
			continue;

		bool shouldRender = false;
		if (cachedActor.type == TrackedActorType::Guard)
		{
			auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
			shouldRender = IsPoliceEspActive(*manager->pConfig) && !(filterDormant && guard->Dead_);
		}
		else if (cachedActor.type == TrackedActorType::Police)
		{
			auto* police = static_cast<SDK::ANPC_Police_base_C*>(currActor);
			shouldRender = IsPoliceEspActive(*manager->pConfig) && !(filterDormant && police->Dead_);
		}
		else if (cachedActor.type == TrackedActorType::Player)
		{
			bool isLocalPlayer = currActor == Vars::CharacterClass || currActor->GetOwner() == Vars::MyController;
			shouldRender = IsPlayerEspActive(*manager->pConfig) && !isLocalPlayer;
		}
		else if (cachedActor.type == TrackedActorType::Camera)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			shouldRender = IsCameraEspActive(*manager->pConfig) && !(filterDormant && camera->Destroyed_);
		}
		else if (cachedActor.type == TrackedActorType::Rat)
		{
			auto* rat = static_cast<SDK::ARatCharacter_C*>(currActor);
			shouldRender = IsRatEspActive(*manager->pConfig) && !(filterDormant && rat->Dead_);
		}

		if (!shouldRender)
			continue;

		bool draw2D = false;
		bool draw3D = false;
		bool drawName = false;
		switch (cachedActor.type)
		{
		case TrackedActorType::Guard:
		case TrackedActorType::Police:
			draw2D = manager->pConfig->esp.policeBox2DEnabled;
			draw3D = manager->pConfig->esp.policeBox3DEnabled;
			drawName = manager->pConfig->esp.policeNameEnabled;
			break;
		case TrackedActorType::Player:
			draw2D = manager->pConfig->esp.playerBox2DEnabled;
			draw3D = manager->pConfig->esp.playerBox3DEnabled;
			drawName = manager->pConfig->esp.playerNameEnabled;
			break;
		case TrackedActorType::Camera:
			draw2D = manager->pConfig->esp.cameraBox2DEnabled;
			draw3D = manager->pConfig->esp.cameraBox3DEnabled;
			drawName = manager->pConfig->esp.cameraNameEnabled;
			break;
		case TrackedActorType::Rat:
			draw2D = manager->pConfig->esp.ratBox2DEnabled;
			draw3D = manager->pConfig->esp.ratBox3DEnabled;
			drawName = manager->pConfig->esp.ratNameEnabled;
			break;
		}

		if (!draw2D && !draw3D && !drawName)
			continue;

		const SDK::FVector actorLocation = currActor->K2_GetActorLocation();
		const float dx = actorLocation.X - cameraLocation.X;
		const float dy = actorLocation.Y - cameraLocation.Y;
		const float dz = actorLocation.Z - cameraLocation.Z;
		if (dx * dx + dy * dy + dz * dz > maxDistSq)
			continue;

		const bool isCharacterActor =
			cachedActor.type == TrackedActorType::Guard ||
			cachedActor.type == TrackedActorType::Police ||
			cachedActor.type == TrackedActorType::Player ||
			cachedActor.type == TrackedActorType::Rat;

		ImVec2 minPoint{};
		ImVec2 maxPoint{};
		bool has2DRect = false;

		if ((draw2D || drawName) && isCharacterActor)
			has2DRect = GetCharacter2DBox(static_cast<SDK::ACharacter*>(currActor), minPoint, maxPoint);

		std::array<ImVec2, 8> screenCorners{};
		bool hasProjectedBounds = false;
		if (draw3D || ((draw2D || drawName) && !has2DRect))
		{
			SDK::FVector origin{};
			SDK::FVector extent{};
			if (!GetVisualBounds(cachedActor, origin, extent))
				continue;
			hasProjectedBounds = GetProjectedBounds(origin, extent, screenCorners, minPoint, maxPoint);
		}

		const ImU32 boxColor = GetBoxColor(config, Vars::MyController, currActor, cameraLocation);
		if (draw2D && (has2DRect || hasProjectedBounds))
		{
			drawList->AddRect(minPoint, maxPoint, GetOutlineColor(boxColor), 0.0f, 0, 1.0f + kOutlineThickness);
			drawList->AddRect(minPoint, maxPoint, boxColor, 0.0f, 0, 1.0f);
		}
		if (draw3D && hasProjectedBounds)
			Draw3DBox(screenCorners, drawList, boxColor);

		if (drawName && (has2DRect || hasProjectedBounds))
		{
			const std::string actorName = currActor->GetName();
			if (!actorName.empty())
			{
				static constexpr float kNameFontSize = 13.0f;
				ImFont* nameFont = (manager->pGui && manager->pGui->tahomaFont) ? manager->pGui->tahomaFont : ImGui::GetFont();
				const ImVec2 textSize = nameFont
					? nameFont->CalcTextSizeA(kNameFontSize, FLT_MAX, 0.0f, actorName.c_str())
					: ImGui::CalcTextSize(actorName.c_str());
				const float centerX = (minPoint.x + maxPoint.x) * 0.5f;
				const ImVec2 textPos = { centerX - textSize.x * 0.5f, minPoint.y - textSize.y - 2.0f };
				const ImVec2 shadowPos = { textPos.x + 1.0f, textPos.y + 1.0f };
				const ImU32 shadowColor = IM_COL32(0, 0, 0, 200);
				drawList->AddText(nameFont, kNameFontSize, shadowPos, shadowColor, actorName.c_str());
				drawList->AddText(nameFont, kNameFontSize, textPos, boxColor, actorName.c_str());
			}
		}
	}
}

void Esp::RenderFovCircle()
{
	if (!manager->pConfig->aimbot.enabled || !manager->pConfig->aimbot.showFov)
		return;

	ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	ImVec2 center = { displaySize.x * 0.5f, displaySize.y * 0.5f };

	float gameFov = 90.f;
	if (Vars::MyController && Vars::MyController->PlayerCameraManager)
		gameFov = Vars::MyController->PlayerCameraManager->GetFOVAngle();

	float radius = tanf(manager->pConfig->aimbot.fov * (3.14159265f / 180.0f))
		/ tanf(gameFov * 0.5f * (3.14159265f / 180.0f))
		* (displaySize.y * 0.5f);

	ImGui::GetBackgroundDrawList()->AddCircle(center, radius, IM_COL32(0, 0, 0, 120), 64, 1.0f + kOutlineThickness);
	ImGui::GetBackgroundDrawList()->AddCircle(center, radius, IM_COL32(255, 255, 255, 150), 64, 1.0f);
}
