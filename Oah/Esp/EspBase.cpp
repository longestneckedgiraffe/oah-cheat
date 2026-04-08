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

	if (cachedActor.type == Esp::TrackedActorType::Police ||
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

static void Draw3DBox(const std::array<ImVec2, 8>& screenCorners, ImDrawList* drawList, ImU32 color)
{
	static constexpr int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7},
	};

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

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	std::unordered_set<std::uintptr_t> activeBullets;
	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;
		if (!currActor->IsA(SDK::ABulletTrace_C::StaticClass()))
			continue;

		const std::uintptr_t actorKey = reinterpret_cast<std::uintptr_t>(currActor);
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
		drawList->AddLine(startScreen, endScreen, color, 1.0f);
	}
}

void Esp::RenderEntityBoxes()
{
	const bool anyBoxesEnabled =
		(manager->pConfig->esp.policeEspEnabled && (manager->pConfig->esp.policeBox2DEnabled || manager->pConfig->esp.policeBox3DEnabled)) ||
		(manager->pConfig->esp.playerEspEnabled && (manager->pConfig->esp.playerBox2DEnabled || manager->pConfig->esp.playerBox3DEnabled)) ||
		(manager->pConfig->esp.cameraEspEnabled && (manager->pConfig->esp.cameraBox2DEnabled || manager->pConfig->esp.cameraBox3DEnabled)) ||
		(manager->pConfig->esp.ratEspEnabled && (manager->pConfig->esp.ratBox2DEnabled || manager->pConfig->esp.ratBox3DEnabled));

	if (!anyBoxesEnabled)
		return;
	if (!Vars::MyController || !Vars::MyController->PlayerCameraManager)
		return;
	if (cachedEspActors.empty())
		return;

	const bool filterDormant = manager->pConfig->settings.filterDormant;
	auto* drawList = ImGui::GetBackgroundDrawList();
	const ImU32 boxColor = IM_COL32(255, 255, 255, 210);
	const SDK::FVector cameraLocation = Vars::MyController->PlayerCameraManager->GetCameraLocation();
	static constexpr float kMaxEspDistance = 20000.0f;
	const float maxDistSq = kMaxEspDistance * kMaxEspDistance;

	for (const CachedEspActor& cachedActor : cachedEspActors)
	{
		SDK::AActor* currActor = cachedActor.actor;
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		bool shouldRender = false;
		if (cachedActor.type == TrackedActorType::Police && currActor->IsA(SDK::ANPC_Guard_C::StaticClass()))
		{
			auto* guard = static_cast<SDK::ANPC_Guard_C*>(currActor);
			shouldRender = manager->pConfig->esp.policeEspEnabled && !(filterDormant && guard->Dead_);
		}
		else if (cachedActor.type == TrackedActorType::Police && currActor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
		{
			auto* police = static_cast<SDK::ANPC_Police_base_C*>(currActor);
			shouldRender = manager->pConfig->esp.policeEspEnabled && !(filterDormant && police->Dead_);
		}
		else if (cachedActor.type == TrackedActorType::Player)
		{
			bool isLocalPlayer = currActor == Vars::CharacterClass || currActor->GetOwner() == Vars::MyController;
			shouldRender = manager->pConfig->esp.playerEspEnabled && !isLocalPlayer;
		}
		else if (cachedActor.type == TrackedActorType::Camera)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			shouldRender = manager->pConfig->esp.cameraEspEnabled && !(filterDormant && camera->Destroyed_);
		}
		else if (cachedActor.type == TrackedActorType::Rat)
		{
			auto* rat = static_cast<SDK::ARatCharacter_C*>(currActor);
			shouldRender = manager->pConfig->esp.ratEspEnabled && !(filterDormant && rat->Dead_);
		}

		if (!shouldRender)
			continue;

		bool draw2D = false;
		bool draw3D = false;
		switch (cachedActor.type)
		{
		case TrackedActorType::Police:
			draw2D = manager->pConfig->esp.policeBox2DEnabled;
			draw3D = manager->pConfig->esp.policeBox3DEnabled;
			break;
		case TrackedActorType::Player:
			draw2D = manager->pConfig->esp.playerBox2DEnabled;
			draw3D = manager->pConfig->esp.playerBox3DEnabled;
			break;
		case TrackedActorType::Camera:
			draw2D = manager->pConfig->esp.cameraBox2DEnabled;
			draw3D = manager->pConfig->esp.cameraBox3DEnabled;
			break;
		case TrackedActorType::Rat:
			draw2D = manager->pConfig->esp.ratBox2DEnabled;
			draw3D = manager->pConfig->esp.ratBox3DEnabled;
			break;
		}

		if (!draw2D && !draw3D)
			continue;

		const SDK::FVector actorLocation = currActor->K2_GetActorLocation();
		const float dx = actorLocation.X - cameraLocation.X;
		const float dy = actorLocation.Y - cameraLocation.Y;
		const float dz = actorLocation.Z - cameraLocation.Z;
		if (dx * dx + dy * dy + dz * dz > maxDistSq)
			continue;

		const bool isCharacterActor =
			cachedActor.type == TrackedActorType::Police ||
			cachedActor.type == TrackedActorType::Player ||
			cachedActor.type == TrackedActorType::Rat;

		ImVec2 minPoint{};
		ImVec2 maxPoint{};
		bool has2DRect = false;

		if (draw2D && isCharacterActor)
			has2DRect = GetCharacter2DBox(static_cast<SDK::ACharacter*>(currActor), minPoint, maxPoint);

		std::array<ImVec2, 8> screenCorners{};
		bool hasProjectedBounds = false;
		if (draw3D || (draw2D && !has2DRect))
		{
			SDK::FVector origin{};
			SDK::FVector extent{};
			if (!GetVisualBounds(cachedActor, origin, extent))
				continue;
			hasProjectedBounds = GetProjectedBounds(origin, extent, screenCorners, minPoint, maxPoint);
		}

		if (draw2D && (has2DRect || hasProjectedBounds))
			drawList->AddRect(minPoint, maxPoint, boxColor, 0.0f, 0, 1.0f);
		if (draw3D && hasProjectedBounds)
			Draw3DBox(screenCorners, drawList, boxColor);
	}
}

void Esp::RenderDebugESP()
{
	if (!manager->pConfig->debugEsp.enabled)
		return;
	if (!Vars::MyController || !Vars::World || Vars::World->Levels.Num() == 0)
		return;

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	ImFont* font = manager->pGui->tahomaFont;
	if (!font)
		return;

	SDK::FVector cameraLocation = Vars::MyController->PlayerCameraManager->GetCameraLocation();
	float maxDist = manager->pConfig->debugEsp.maxDistance;
	float maxDistSq = maxDist * maxDist;

	auto* drawList = ImGui::GetBackgroundDrawList();
	ImU32 boxColor = IM_COL32(0, 255, 0, 120);
	ImU32 textColor = IM_COL32(255, 255, 255, 200);
	ImU32 textBg = IM_COL32(0, 0, 0, 150);

	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		SDK::FVector origin{};
		SDK::FVector extent{};
		currActor->GetActorBounds(true, &origin, &extent, false);

		if (origin.X == 0.f && origin.Y == 0.f && origin.Z == 0.f)
			continue;
		if (extent.X == 0.f && extent.Y == 0.f && extent.Z == 0.f)
			continue;

		float dx = origin.X - cameraLocation.X;
		float dy = origin.Y - cameraLocation.Y;
		float dz = origin.Z - cameraLocation.Z;
		if (dx * dx + dy * dy + dz * dz > maxDistSq)
			continue;

		ImVec2 screenCenter;
		if (!WorldToScreen(origin, screenCenter))
			continue;

		SDK::FVector corners[8] = {
			{ origin.X - extent.X, origin.Y - extent.Y, origin.Z - extent.Z },
			{ origin.X + extent.X, origin.Y - extent.Y, origin.Z - extent.Z },
			{ origin.X + extent.X, origin.Y + extent.Y, origin.Z - extent.Z },
			{ origin.X - extent.X, origin.Y + extent.Y, origin.Z - extent.Z },
			{ origin.X - extent.X, origin.Y - extent.Y, origin.Z + extent.Z },
			{ origin.X + extent.X, origin.Y - extent.Y, origin.Z + extent.Z },
			{ origin.X + extent.X, origin.Y + extent.Y, origin.Z + extent.Z },
			{ origin.X - extent.X, origin.Y + extent.Y, origin.Z + extent.Z },
		};

		ImVec2 screenCorners[8];
		bool allVisible = true;
		for (int k = 0; k < 8; k++)
		{
			if (!WorldToScreen(corners[k], screenCorners[k]))
			{
				allVisible = false;
				break;
			}
		}

		if (!allVisible)
			continue;

		int edges[12][2] = {
			{0,1},{1,2},{2,3},{3,0},
			{4,5},{5,6},{6,7},{7,4},
			{0,4},{1,5},{2,6},{3,7},
		};
		for (int k = 0; k < 12; k++)
			drawList->AddLine(screenCorners[edges[k][0]], screenCorners[edges[k][1]], boxColor, 1.0f);

		std::string name = currActor->GetName();
		float fontSize = font->LegacySize;
		ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, name.c_str());
		ImVec2 textPos = { screenCenter.x - textSize.x * 0.5f, screenCenter.y - textSize.y - 2.f };

		drawList->AddRectFilled(
			{ textPos.x - 2, textPos.y - 1 },
			{ textPos.x + textSize.x + 2, textPos.y + textSize.y + 1 },
			textBg
		);
		drawList->AddText(font, fontSize, textPos, textColor, name.c_str());
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

	ImGui::GetBackgroundDrawList()->AddCircle(center, radius, IM_COL32(255, 255, 255, 150), 64, 1.0f);
}
