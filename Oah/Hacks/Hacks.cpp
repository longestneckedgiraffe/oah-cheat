#include <vector>

#include "../Core/Manager.h"
#include "../Core/Variables.h"
#include "Hacks.h"

#include "../Libs/UEDump/SDK/GunBase_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Guard_classes.hpp"
#include "../Libs/UEDump/SDK/NPC_Police_base_classes.hpp"
#include "../Libs/UEDump/SDK/AIModule_classes.hpp"
#include "../Libs/UEDump/SDK/NPCBase_classes.hpp"
#include "../Libs/UEDump/SDK/SpotPlayerComponent_classes.hpp"
#include "../Libs/UEDump/SDK/CameraBP_classes.hpp"
#include "../Libs/UEDump/SDK/AlertComponent_classes.hpp"
#include "../Libs/UEDump/SDK/DoorBP_classes.hpp"
#include "../Libs/UEDump/SDK/AlarmBP_classes.hpp"
#include "../Libs/UEDump/SDK/BP_HackingPoint_classes.hpp"
#include "../Libs/UEDump/SDK/BP_Powerbox_classes.hpp"

SDK::FVector GetVectorForward(const SDK::FVector& angles);
SDK::FVector GetVectorForward(const SDK::FRotator& angles);
#define IsKeyHeld(key) (GetAsyncKeyState(key) & 0x8000)

namespace
{
	struct WeaponAmmoState
	{
		int bulletsLeft{};
		float reloadTime{};
	};

	struct TrackedWeaponAmmoState
	{
		SDK::AGunBase_C* weapon{};
		WeaponAmmoState state{};
	};

	static std::vector<TrackedWeaponAmmoState> g_trackedAmmoWeapons;

	bool TryReadWeaponAmmoState(SDK::AGunBase_C* weapon, WeaponAmmoState& state)
	{
		__try
		{
			if (!weapon)
				return false;

			state.bulletsLeft = weapon->BulletsLeft;
			state.reloadTime = weapon->CalculatedReloadTime;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	bool TryApplyUnlimitedAmmo(SDK::AGunBase_C* weapon)
	{
		__try
		{
			if (!weapon)
				return false;

			weapon->CalculatedReloadTime = 0.0f;
			weapon->BulletsLeft = 1337;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	bool TryRestoreWeaponAmmoState(const TrackedWeaponAmmoState& trackedWeapon)
	{
		__try
		{
			SDK::AGunBase_C* weapon = trackedWeapon.weapon;
			if (!weapon)
				return false;

			int bulletsToRestore = trackedWeapon.state.bulletsLeft;
			if (bulletsToRestore < 0)
				bulletsToRestore = 0;
			if (bulletsToRestore > weapon->MagSize)
				bulletsToRestore = weapon->MagSize;

			weapon->BulletsLeft = bulletsToRestore;
			weapon->CalculatedReloadTime = trackedWeapon.state.reloadTime;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	void TrackWeaponAmmoStateIfNeeded(SDK::AGunBase_C* weapon)
	{
		if (!weapon)
			return;

		for (const TrackedWeaponAmmoState& trackedWeapon : g_trackedAmmoWeapons)
		{
			if (trackedWeapon.weapon == weapon)
				return;
		}

		WeaponAmmoState state{};
		if (!TryReadWeaponAmmoState(weapon, state))
			return;

		g_trackedAmmoWeapons.push_back({ weapon, state });
	}
}

void Hacks::RunHacks()
{
	if (!Vars::CharacterClass || !Vars::CharacterClass->HasLoaded_)
		return;

	Aimbot();
	DisableCameras();
	GuardPhoneDelay();
	SpeedHack();
	LevelHack();
	CashHack();
	FlyHack();
	Noclip();
	UnlimitedAmmo();
	GunMods();
	JumpHack();
	UnlockDoors();
	DisableAlarms();
	TeleportExploits();
	TieUpCivilians();
	Invulnerable();
}

void Hacks::Aimbot()
{
	if (!manager->pConfig->aimbot.enabled)
		return;
	if (!(GetAsyncKeyState(VK_XBUTTON1) & 0x8000))
		return;
	if (!Vars::MyController)
		return;
	if (!Vars::MyController->PlayerCameraManager)
		return;
	if (Vars::World->Levels.Num() == 0)
		return;

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	SDK::FVector cameraLocation = Vars::MyController->PlayerCameraManager->GetCameraLocation();
	SDK::FRotator cameraRotation = Vars::MyController->PlayerCameraManager->GetCameraRotation();
	SDK::FVector cameraForward = GetVectorForward(cameraRotation);
	static SDK::FName headBone = SDK::FName();
	static bool headBoneInitialized = false;
	if (!headBoneInitialized)
	{
		GetStaticName(L"head", headBone);
		headBoneInitialized = true;
	}

	SDK::AActor* bestTarget = nullptr;
	float bestAngle = manager->pConfig->aimbot.fov;
	SDK::FVector bestHeadLocation{};
	static std::vector<SDK::AActor*> cachedTargets;
	static SDK::ULevel* cachedLevel = nullptr;
	static int targetCacheFrames = 0;

	targetCacheFrames++;
	if (cachedLevel != currLevel || targetCacheFrames >= 30)
	{
		cachedTargets.clear();
		cachedTargets.reserve(128);
		cachedLevel = currLevel;
		targetCacheFrames = 0;

		for (int j = 0; j < currLevel->Actors.Num(); j++)
		{
			SDK::AActor* currActor = currLevel->Actors[j];
			if (!currActor || !currActor->RootComponent)
				continue;
			if (Fns::IsBadPoint(currActor))
				continue;
			if (!currActor->IsA(SDK::ANPC_Guard_C::StaticClass()) &&
				!currActor->IsA(SDK::ANPC_Police_base_C::StaticClass()))
				continue;

			cachedTargets.push_back(currActor);
		}
	}

	for (SDK::AActor* currActor : cachedTargets)
	{
		if (!currActor || !currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		auto* npc = static_cast<SDK::ANPCBase_C*>(currActor);
		if (manager->pConfig->settings.filterDormant && npc->Dead_)
			continue;

		const auto location = currActor->K2_GetActorLocation();
		if (location.X == 0.f || location.Y == 0.f || location.Z == 0.f)
			continue;

		auto* character = static_cast<SDK::ACharacter*>(currActor);
		if (!character->Mesh)
			continue;

		SDK::FVector headLocation = character->Mesh->GetSocketLocation(headBone);
		headLocation.Z += 10.f;

		SDK::FVector direction = headLocation - cameraLocation;
		float dist = sqrtf(direction.X * direction.X + direction.Y * direction.Y + direction.Z * direction.Z);
		if (dist == 0.f)
			continue;

		SDK::FVector dirNorm = { direction.X / dist, direction.Y / dist, direction.Z / dist };
		float dot = cameraForward.X * dirNorm.X + cameraForward.Y * dirNorm.Y + cameraForward.Z * dirNorm.Z;

		if (dot > 1.f) dot = 1.f;
		if (dot < -1.f) dot = -1.f;

		float angle = acosf(dot) * (180.0f / 3.14159265f);

		if (angle < bestAngle)
		{
			bestAngle = angle;
			bestTarget = currActor;
			bestHeadLocation = headLocation;
		}
	}

	if (!bestTarget)
		return;

	SDK::FVector direction = bestHeadLocation - cameraLocation;
	float horizontalDist = sqrtf(direction.X * direction.X + direction.Y * direction.Y);
	float yaw = atan2f(direction.Y, direction.X) * (180.0f / 3.14159265f);
	float pitch = atan2f(direction.Z, horizontalDist) * (180.0f / 3.14159265f);

	SDK::FRotator currentRotation = Vars::MyController->GetControlRotation();
	SDK::FRotator newRotation;
	newRotation.Pitch = pitch;
	newRotation.Yaw = yaw;
	newRotation.Roll = currentRotation.Roll;

	Vars::MyController->SetControlRotation(newRotation);
}

void Hacks::DisableCameras()
{
	static bool cameraState = false;
	static int cameraFrameCounter = 0;

	if (!manager->pConfig->disableCameras.enabled)
	{
		if (cameraState)
		{
			if (Vars::World && Vars::World->Levels.Num() > 0)
			{
				SDK::ULevel* currLevel = Vars::World->Levels[0];
				if (currLevel)
				{
					for (int j = 0; j < currLevel->Actors.Num(); j++)
					{
						SDK::AActor* currActor = currLevel->Actors[j];
						if (!currActor || !currActor->RootComponent)
							continue;
						if (Fns::IsBadPoint(currActor))
							continue;
						if (currActor->GetFullName().find("CameraBP") != std::string::npos)
						{
							auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
							camera->Ignored_ = false;
						}
					}
				}
			}
			cameraState = false;
			cameraFrameCounter = 0;
		}
		return;
	}

	cameraFrameCounter++;
	if (cameraState && cameraFrameCounter < 60)
		return;
	cameraFrameCounter = 0;
	cameraState = true;

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
		if (currActor->GetFullName().find("CameraBP") != std::string::npos)
		{
			auto* camera = static_cast<SDK::ACameraBP_C*>(currActor);
			camera->Ignored_ = true;

			if (camera->SpotPlayerComponent)
			{
				camera->SpotPlayerComponent->SpottedPlayer = nullptr;
				camera->SpotPlayerComponent->Spot_time = 0.f;
			}
		}
	}
}

void Hacks::GuardPhoneDelay()
{
	static bool phoneState = false;
	if (!manager->pConfig->guardPhoneDelay.enabled)
	{
		if (phoneState)
		{
			Vars::CharacterClass->AddedGuardPhoneTime = 0;
			phoneState = false;
		}
		return;
	}

	phoneState = true;
	Vars::CharacterClass->AddedGuardPhoneTime = 99999;
}

void Hacks::SpeedHack()
{
	static bool speedState = false;
	if (manager->pConfig->speed.enabled)
	{
		Vars::CharacterClass->CharacterMovement->MaxWalkSpeed = manager->pConfig->speed.speed;
		Vars::CharacterClass->CharacterMovement->MaxAcceleration = manager->pConfig->speed.speed;
		speedState = true;
	}
	else if (speedState)
	{
		Vars::CharacterClass->CharacterMovement->MaxWalkSpeed = 600.f;
		Vars::CharacterClass->CharacterMovement->MaxAcceleration = 2048.f;
		speedState = false;
	}
}

void Hacks::LevelHack()
{
	if (!manager->pConfig->levelHack.setLevel)
		return;
	if (!Vars::CharacterClass->PCController)
		return;
	if (!Vars::CharacterClass->PCController->Level)
		return;

	Vars::CharacterClass->PCController->Level = static_cast<UC::int32>(manager->pConfig->levelHack.level);
	Vars::CharacterClass->PCController->SaveLevel();
	manager->pConfig->levelHack.setLevel = false;
}

void Hacks::CashHack()
{
	if (!manager->pConfig->cashHack.setCash)
		return;
	if (!Vars::CharacterClass->PCController)
		return;
	if (!Vars::CharacterClass->PCController->Cash)
		return;

	Vars::CharacterClass->PCController->Cash = static_cast<UC::int32>(manager->pConfig->cashHack.cashValue);
	Vars::CharacterClass->PCController->SaveCash();
	manager->pConfig->cashHack.setCash = false;
}

SDK::FVector GetVectorForward(const SDK::FVector& angles)
{
	float sp, sy, cp, cy;
	float angle;

	angle = angles.Y * (3.14159265 / 180.0f);
	sy = sinf(angle);
	cy = cosf(angle);
	angle = -angles.X * (3.14159265 / 180.0f);
	sp = sinf(angle);
	cp = cosf(angle);

	return { cp * cy, cp * sy, -sp };
}
SDK::FVector GetVectorForward(const SDK::FRotator& angles)
{
	float sp, sy, cp, cy;
	float angle;

	angle = angles.Yaw * (3.14159265 / 180.0f);
	sy = sinf(angle);
	cy = cosf(angle);
	angle = -angles.Pitch * (3.14159265 / 180.0f);
	sp = sinf(angle);
	cp = cosf(angle);

	return { cp * cy, cp * sy, -sp };
}

void Hacks::FlyHack()
{
	if (!Vars::MyController->PlayerCameraManager)
		return;
	static bool flyHackState = false;
	if (manager->pConfig->flyHack.enabled)
	{
		Vars::CharacterClass->CharacterMovement->Velocity = SDK::FVector{0, 0, 0};
		Vars::CharacterClass->CharacterMovement->MaxFlySpeed = 600.f;
		Vars::CharacterClass->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Flying;

		SDK::FVector pos = { 0.f, 0.f, 0.f };
		if (GetAsyncKeyState(VK_SPACE))
			pos = { 0.f, 0.f, 3.f };
		else if (GetAsyncKeyState(VK_LCONTROL))
			pos = { 0.f, 0.f, -3.f };

		SDK::FVector sum = { };
		SDK::FVector newRot = { };
		float flySpeed = Vars::CharacterClass->CharacterMovement->MaxFlySpeed / 90.f;

		if (IsKeyHeld(VK_SHIFT))
			flySpeed *= 2.f;

		SDK::FRotator camRot = Vars::MyController->PlayerCameraManager->GetCameraRotation();

		if (IsKeyHeld('W'))
			sum += GetVectorForward(camRot) * flySpeed;
		else if (IsKeyHeld('S'))
		{
			newRot = { -camRot.Pitch, camRot.Yaw + 180.f, 0.f };
			sum += GetVectorForward(newRot) * flySpeed;
		}

		if (IsKeyHeld('D'))
		{
			newRot = { 0.f, camRot.Yaw + 90.f, 0.f };
			sum += GetVectorForward(newRot) * flySpeed;
		}
		else if (IsKeyHeld('A'))
		{
			newRot = { 0.f, camRot.Yaw + 270.f, 0.f };
			sum += GetVectorForward(newRot) * flySpeed;
		}

		Vars::CharacterClass->K2_TeleportTo(Vars::CharacterClass->K2_GetActorLocation() + pos + sum, Vars::CharacterClass->K2_GetActorRotation());
		flyHackState = true;
	}
	else if(!manager->pConfig->flyHack.enabled && flyHackState)
	{
		Vars::CharacterClass->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Falling;
		flyHackState = false;
	}
}

void Hacks::Noclip()
{
	static bool noclipState = false;
	if (manager->pConfig->noclip.enabled)
	{
		Vars::CharacterClass->bActorEnableCollision = false;

		if (!manager->pConfig->flyHack.enabled &&
			Vars::CharacterClass->CharacterMovement->MovementMode != SDK::EMovementMode::MOVE_Flying)
		{
			Vars::CharacterClass->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Flying;
		}

		noclipState = true;
	}
	else if (noclipState)
	{
		Vars::CharacterClass->bActorEnableCollision = true;

		if (!manager->pConfig->flyHack.enabled)
			Vars::CharacterClass->CharacterMovement->MovementMode = SDK::EMovementMode::MOVE_Falling;

		noclipState = false;
	}
}

void Hacks::UnlimitedAmmo()
{
	static bool ammoState = false;

	if (!manager->pConfig->unlimitedAmmo.enabled)
	{
		if (ammoState)
		{
			for (const TrackedWeaponAmmoState& trackedWeapon : g_trackedAmmoWeapons)
			{
				TryRestoreWeaponAmmoState(trackedWeapon);
			}

			g_trackedAmmoWeapons.clear();
			ammoState = false;
		}
		return;
	}

	ammoState = true;
	if (!Vars::CharacterClass->HoldingGun)
		return;

	SDK::AGunBase_C* holdingGun = Vars::CharacterClass->HoldingGun;
	TrackWeaponAmmoStateIfNeeded(holdingGun);
	TryApplyUnlimitedAmmo(holdingGun);
}

void Hacks::GunMods()
{
	bool rapidFire = manager->pConfig->rapidFire.enabled;
	bool instantReload = manager->pConfig->instantReload.enabled && Vars::CharacterClass->Reloading_;
	bool multishot = manager->pConfig->multishot.enabled;

	if (!rapidFire && !instantReload && !multishot)
		return;
	if (!Vars::CharacterClass->HoldingGun)
		return;

	__try
	{
		if (rapidFire)
		{
			Vars::CharacterClass->HoldingGun->CoolDownTime = 0.01f;
			Vars::CharacterClass->HoldingGun->Auto_ = true;
		}
		if (instantReload)
		{
			Vars::CharacterClass->HoldingGun->BulletsLeft = Vars::CharacterClass->HoldingGun->MagSize;
			Vars::CharacterClass->Reloading_ = false;
			Vars::CharacterClass->HoldingGun->CanShoot_ = true;
		}
		if (multishot)
		{
			Vars::CharacterClass->HoldingGun->BulletAmount = 10;
		}
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	{
		;
	}
}

void Hacks::JumpHack()
{
	if (Vars::CharacterClass->CharacterMovement == nullptr)
		return;

	static bool jumpHackState = false;
	if (manager->pConfig->jumpHack.enabled)
	{
		Vars::CharacterClass->CharacterMovement->JumpZVelocity = static_cast<float>(manager->pConfig->jumpHack.value);
		jumpHackState = true;
	}
	else if(jumpHackState)
	{
		Vars::CharacterClass->CharacterMovement->JumpZVelocity = 300.f;
		jumpHackState = false;
	}
}

void Hacks::UnlockDoors()
{
	if (!manager->pConfig->unlockDoors.enabled)
		return;
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
		if (currActor->GetFullName().find("DoorBP") != std::string::npos)
		{
			auto* door = static_cast<SDK::ADoorBP_C*>(currActor);
			door->Locked_ = false;
			door->PowerLocked_ = false;
		}
	}

	manager->pConfig->unlockDoors.enabled = false;
}

void Hacks::DisableAlarms()
{
	if (!manager->pConfig->disableAlarms.enabled)
		return;
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
		if (currActor->GetFullName().find("AlarmBP") != std::string::npos)
		{
			auto* alarm = static_cast<SDK::AAlarmBP_C*>(currActor);
			alarm->AlarmEnabled_ = false;
			alarm->HasAlarmTriggered_ = false;
		}
	}

	manager->pConfig->disableAlarms.enabled = false;
}

void Hacks::TeleportExploits()
{
	if (!manager->pConfig->teleportExploits.killCivilians && !manager->pConfig->teleportExploits.killRats &&
		!manager->pConfig->teleportExploits.killPolice && !manager->pConfig->teleportExploits.killDoors &&
		!manager->pConfig->teleportExploits.killCameras)
		return;
	if (!Vars::MyController)
		return;
	if (!Vars::MyController->PlayerCameraManager)
		return;
	if (Vars::World->Levels.Num() == 0)
		return;

	SDK::ULevel* currLevel = Vars::World->Levels[0];
	if (!currLevel)
		return;

	for (int j = 0; j < currLevel->Actors.Num(); j++)
	{
		SDK::AActor* currActor = currLevel->Actors[j];

		if (!currActor)
			continue;
		if (!currActor->RootComponent)
			continue;
		if (Fns::IsBadPoint(currActor))
			continue;

		const auto location = currActor->K2_GetActorLocation();
		if (location.X == 0.f || location.Y == 0.f || location.Z == 0.f)
			continue;

		SDK::FVector teleLocation = { -9999.f, 9999.f, 9999.f };
		std::string fullName = currActor->GetFullName();
		if (manager->pConfig->teleportExploits.killCivilians && fullName.find("Civilian_NPC") != std::string::npos)
			currActor->K2_TeleportTo(teleLocation, SDK::FRotator{ 0, 0, 0 });
		else if (manager->pConfig->teleportExploits.killRats && fullName.find("RatCharacter") != std::string::npos)
			currActor->K2_TeleportTo(teleLocation, SDK::FRotator{ 0, 0, 0 });
		else if (manager->pConfig->teleportExploits.killPolice && (fullName.find("NPC_Police") != std::string::npos || fullName.find("NPC_Guard") != std::string::npos))
			currActor->K2_TeleportTo(teleLocation, SDK::FRotator{ 0, 0, 0 });
		else if (manager->pConfig->teleportExploits.killDoors && fullName.find("DoorBP") != std::string::npos)
			currActor->K2_TeleportTo(teleLocation, SDK::FRotator{ 0, 0, 0 });
		else if (manager->pConfig->teleportExploits.killCameras && fullName.find("CameraBP") != std::string::npos)
			currActor->K2_TeleportTo(teleLocation, SDK::FRotator{ 0, 0, 0 });
	}

	manager->pConfig->teleportExploits.killCivilians = false;
	manager->pConfig->teleportExploits.killPolice = false;
	manager->pConfig->teleportExploits.killRats = false;
	manager->pConfig->teleportExploits.killDoors = false;
	manager->pConfig->teleportExploits.killCameras = false;
}

void Hacks::TieUpCivilians()
{
	if (!manager->pConfig->tieUpCivilians.enabled)
		return;

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

		if (currActor->GetFullName().find("Civilian_NPC") != std::string::npos)
		{
			auto* npc = static_cast<SDK::ANPCBase_C*>(currActor);
			npc->TiedUp_ = true;
			if (npc->CharacterMovement)
				npc->CharacterMovement->DisableMovement();
		}
	}

	manager->pConfig->tieUpCivilians.enabled = false;
}

void Hacks::Invulnerable()
{
	static bool invulnerableState = false;
	static int invulnerableFrameCounter = 0;

	if (!manager->pConfig->invulnerable.enabled)
	{
		if (invulnerableState)
		{
			Vars::CharacterClass->DamageImmunity = 0;
			invulnerableState = false;
		}
		invulnerableFrameCounter = 0;
		return;
	}

	invulnerableFrameCounter++;
	if (invulnerableState && invulnerableFrameCounter < 300)
		return;
	invulnerableFrameCounter = 0;
	invulnerableState = true;

	Vars::CharacterClass->GiveImmunityLevel(99999.f);
}
