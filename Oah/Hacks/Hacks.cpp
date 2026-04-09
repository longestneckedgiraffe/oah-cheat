#include <unordered_map>
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
#include "../Libs/UEDump/SDK/Armor_Light_classes.hpp"
#include "../Libs/UEDump/SDK/BP_CarValueOverlapper_classes.hpp"
#include "../Libs/UEDump/SDK/DoorBP_classes.hpp"
#include "../Libs/UEDump/SDK/Duffelbag_classes.hpp"
#include "../Libs/UEDump/SDK/AlarmBP_classes.hpp"
#include "../Libs/UEDump/SDK/BP_HackingPoint_classes.hpp"
#include "../Libs/UEDump/SDK/BP_Powerbox_classes.hpp"
#include "../Libs/UEDump/SDK/Lock_classes.hpp"
#include "../Libs/UEDump/SDK/Lock_pick_classes.hpp"
#include "../Libs/UEDump/SDK/Money_base_classes.hpp"
#include "../Libs/UEDump/SDK/RobberTruck_classes.hpp"

SDK::FVector GetVectorForward(const SDK::FVector& angles);
SDK::FVector GetVectorForward(const SDK::FRotator& angles);
#define IsKeyHeld(key) (GetAsyncKeyState(key) & 0x8000)

namespace
{
	bool ShouldRestrictAimbotToVisibleTargets(const Config& config)
	{
		return config.esp.visibilityCheckEnabled;
	}

	template <typename TFunc>
	void ForEachValidActor(const std::vector<SDK::AActor*>& actors, TFunc&& func)
	{
		for (SDK::AActor* actor : actors)
		{
			if (!actor || !actor->RootComponent)
				continue;
			if (Fns::IsBadPoint(actor))
				continue;

			func(actor);
		}
	}

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

	struct ThirdPersonState
	{
		SDK::APlayerCharacter_C* character{};
		SDK::UCameraComponent* camera{};
		SDK::USkeletalMeshComponent* mesh{};
		SDK::FVector baseRelativeLocation{};
		bool originalOwnerNoSee{};
		bool initialized{ false };
	};

	static std::unordered_map<std::uintptr_t, TrackedWeaponAmmoState> g_trackedAmmoWeapons;
	static ThirdPersonState g_thirdPersonState;

	void RestoreThirdPersonState()
	{
		if (!g_thirdPersonState.initialized)
			return;

		__try
		{
			if (g_thirdPersonState.camera)
				g_thirdPersonState.camera->K2_SetRelativeLocation(g_thirdPersonState.baseRelativeLocation, false, nullptr, true);

			if (g_thirdPersonState.mesh)
				g_thirdPersonState.mesh->SetOwnerNoSee(g_thirdPersonState.originalOwnerNoSee);
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
		}

		g_thirdPersonState = {};
	}

	SDK::AArmor_Light_C* GetEquippedArmorActor()
	{
		if (!Vars::CharacterClass || !Vars::CharacterClass->ArmorChildActor)
			return nullptr;
		if (!Vars::CharacterClass->ArmorChildActor->ChildActor)
			return nullptr;

		auto* armor = static_cast<SDK::AArmor_Light_C*>(Vars::CharacterClass->ArmorChildActor->ChildActor);
		return armor->IsA(SDK::AArmor_Light_C::StaticClass()) ? armor : nullptr;
	}

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

		const std::uintptr_t weaponKey = reinterpret_cast<std::uintptr_t>(weapon);
		if (g_trackedAmmoWeapons.contains(weaponKey))
			return;

		WeaponAmmoState state{};
		if (!TryReadWeaponAmmoState(weapon, state))
			return;

		g_trackedAmmoWeapons.emplace(weaponKey, TrackedWeaponAmmoState{ weapon, state });
	}

	bool TryCompleteActiveLockpick(SDK::ALock_C* lock)
	{
		__try
		{
			if (!lock || lock->Unlocked_)
				return false;
			if (!lock->Tool || !lock->Tool->Picking_)
				return false;

			SDK::ALock_pick_C* tool = lock->Tool;
			tool->PickProgress = tool->PickMaxChange > 0.0f ? tool->PickMaxChange : 100.0f;
			tool->JiggleChange = 0.0f;
			tool->Picking_ = false;

			lock->Unlocked_ = true;
			lock->Unlock();
			lock->UnlockMulti();
			tool->StopPickLock();
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	bool IsPickupHeld(SDK::APickupItem_base_C* pickup)
	{
		return pickup &&
			pickup->PickupItemComponent &&
			pickup->PickupItemComponent->Picked_up_;
	}

	SDK::ARobberTruck_C* FindRobberTruck(const ActorRegistry& actorRegistry)
	{
		SDK::ARobberTruck_C* truck = nullptr;
		ForEachValidActor(actorRegistry.GetRobberTrucks(), [&](SDK::AActor* actor)
		{
			if (!truck)
				truck = static_cast<SDK::ARobberTruck_C*>(actor);
		});
		return truck;
	}

	SDK::FVector GetTruckMoneyDropLocation(SDK::ARobberTruck_C* truck, int index)
	{
		const SDK::FVector baseLocation =
			(truck && truck->MoneyOverlapper)
			? truck->MoneyOverlapper->K2_GetComponentLocation()
			: truck->K2_GetActorLocation();

		const SDK::FRotator truckRotation = truck ? truck->K2_GetActorRotation() : SDK::FRotator{};
		const SDK::FVector right = GetVectorForward(SDK::FRotator{ 0.0f, truckRotation.Yaw + 90.0f, 0.0f });
		const SDK::FVector forward = GetVectorForward(SDK::FRotator{ 0.0f, truckRotation.Yaw, 0.0f });

		static constexpr int kColumns = 4;
		static constexpr int kRows = 3;
		static constexpr float kRightSpacing = 45.0f;
		static constexpr float kForwardSpacing = 55.0f;
		static constexpr float kLayerSpacing = 20.0f;

		const int slot = index % (kColumns * kRows);
		const int layer = index / (kColumns * kRows);
		const int column = slot % kColumns;
		const int row = slot / kColumns;

		const float rightOffset = (static_cast<float>(column) - 1.5f) * kRightSpacing;
		const float forwardOffset = (static_cast<float>(row) - 1.0f) * kForwardSpacing;
		const float upOffset = 10.0f + (static_cast<float>(layer) * kLayerSpacing);

		return
			baseLocation +
			(right * rightOffset) +
			(forward * forwardOffset) +
			SDK::FVector{ 0.0f, 0.0f, upOffset };
	}
}

void Hacks::RunHacks()
{
	if (!Vars::CharacterClass || !Vars::CharacterClass->HasLoaded_)
		return;

	Aimbot();
	DisableCameras();
	SpeedHack();
	LevelHack();
	CashHack();
	Bhop();
	FlyHack();
	Noclip();
	ThirdPerson();
	UnlimitedAmmo();
	GunMods();
	JumpHack();
	InstantLockpick();
	UnlockDoors();
	DisableAlarms();
	TeleportExploits();
	TieUpCivilians();
	Invulnerable();
	MaxHealth();
	MaxArmor();
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

	SDK::FVector cameraLocation = Vars::MyController->PlayerCameraManager->GetCameraLocation();
	SDK::FRotator cameraRotation = Vars::MyController->PlayerCameraManager->GetCameraRotation();
	SDK::FVector cameraForward = GetVectorForward(cameraRotation);
	const bool visibleOnly = ShouldRestrictAimbotToVisibleTargets(*manager->pConfig);
	if (manager->actorRegistry.GetGuards().empty() && manager->actorRegistry.GetPolice().empty())
		manager->actorRegistry.Refresh(true);
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
	auto evaluateTargets = [&](const std::vector<SDK::AActor*>& actors)
	{
		ForEachValidActor(actors, [&](SDK::AActor* currActor)
		{
			auto* npc = static_cast<SDK::ANPCBase_C*>(currActor);
			if (manager->pConfig->settings.filterDormant && npc->Dead_)
				return;

			auto* character = static_cast<SDK::ACharacter*>(currActor);
			if (!character->Mesh)
				return;

			if (visibleOnly && !Vars::MyController->LineOfSightTo(currActor, cameraLocation, false))
				return;

			SDK::FVector headLocation = character->Mesh->GetSocketLocation(headBone);
			headLocation.Z += 10.f;

			SDK::FVector direction = headLocation - cameraLocation;
			float dist = sqrtf(direction.X * direction.X + direction.Y * direction.Y + direction.Z * direction.Z);
			if (dist == 0.f)
				return;

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
		});
	};

	evaluateTargets(manager->actorRegistry.GetGuards());
	evaluateTargets(manager->actorRegistry.GetPolice());

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
			ForEachValidActor(manager->actorRegistry.GetCameras(), [](SDK::AActor* actor)
			{
				auto* camera = static_cast<SDK::ACameraBP_C*>(actor);
				camera->Ignored_ = false;
			});
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
	manager->actorRegistry.Refresh(true);

	ForEachValidActor(manager->actorRegistry.GetCameras(), [](SDK::AActor* actor)
	{
		auto* camera = static_cast<SDK::ACameraBP_C*>(actor);
		camera->Ignored_ = true;

		if (camera->SpotPlayerComponent)
		{
			camera->SpotPlayerComponent->SpottedPlayer = nullptr;
			camera->SpotPlayerComponent->Spot_time = 0.f;
		}
	});
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

	Vars::CharacterClass->PCController->Cash = static_cast<UC::int32>(manager->pConfig->cashHack.cashValue);
	Vars::CharacterClass->PCController->SaveCash();
	manager->pConfig->cashHack.setCash = false;
}

void Hacks::Bhop()
{
	static bool bhopWasJumping = false;

	if (!Vars::CharacterClass || !Vars::CharacterClass->CharacterMovement)
		return;

	if (!manager->pConfig->bhop.enabled || !IsKeyHeld(VK_SPACE))
	{
		if (bhopWasJumping || Vars::CharacterClass->bPressedJump)
			Vars::CharacterClass->StopJumping();

		bhopWasJumping = false;
		return;
	}

	if (Vars::CharacterClass->Downed_ || Vars::CharacterClass->Vaulting_)
	{
		if (bhopWasJumping || Vars::CharacterClass->bPressedJump)
			Vars::CharacterClass->StopJumping();

		bhopWasJumping = false;
		return;
	}

	if (Vars::CharacterClass->CharacterMovement->IsMovingOnGround() && Vars::CharacterClass->CanJump())
	{
		Vars::CharacterClass->Jump();
		bhopWasJumping = true;
	}
	else if (bhopWasJumping || Vars::CharacterClass->bPressedJump)
	{
		Vars::CharacterClass->StopJumping();
	}
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

void Hacks::ThirdPerson()
{
	if (!manager->pConfig->thirdPerson.enabled)
	{
		RestoreThirdPersonState();
		return;
	}

	if (!Vars::CharacterClass)
	{
		RestoreThirdPersonState();
		return;
	}

	SDK::UCameraComponent* camera = Vars::CharacterClass->Camera;
	SDK::USkeletalMeshComponent* mesh = Vars::CharacterClass->Mesh;
	if (!camera || !mesh)
	{
		RestoreThirdPersonState();
		return;
	}

	__try
	{
		if (!g_thirdPersonState.initialized ||
			g_thirdPersonState.character != Vars::CharacterClass ||
			g_thirdPersonState.camera != camera)
		{
			RestoreThirdPersonState();
			g_thirdPersonState.character = Vars::CharacterClass;
			g_thirdPersonState.camera = camera;
			g_thirdPersonState.mesh = mesh;
			g_thirdPersonState.baseRelativeLocation = camera->RelativeLocation;
			g_thirdPersonState.originalOwnerNoSee = mesh->bOwnerNoSee;
			g_thirdPersonState.initialized = true;
		}

		SDK::FVector anchorLocation = camera->K2_GetComponentLocation();
		if (Vars::CharacterClass->CamDefaultLocation)
			anchorLocation = Vars::CharacterClass->CamDefaultLocation->K2_GetComponentLocation();

		SDK::FRotator cameraRotation = Vars::CharacterClass->K2_GetActorRotation();
		if (Vars::MyController && Vars::MyController->PlayerCameraManager)
			cameraRotation = Vars::MyController->PlayerCameraManager->GetCameraRotation();

		const SDK::FRotator horizontalRotation{ 0.0f, cameraRotation.Yaw, 0.0f };
		const SDK::FVector forward = GetVectorForward(horizontalRotation);
		const SDK::FVector right = GetVectorForward(SDK::FRotator{ 0.0f, horizontalRotation.Yaw + 90.0f, 0.0f });

		const SDK::FVector targetLocation =
			anchorLocation -
			(forward * manager->pConfig->thirdPerson.back) +
			(right * manager->pConfig->thirdPerson.right) +
			SDK::FVector{ 0.0f, 0.0f, manager->pConfig->thirdPerson.up };

		camera->K2_SetWorldLocation(targetLocation, false, nullptr, true);
		mesh->SetOwnerNoSee(false);
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	{
		RestoreThirdPersonState();
	}
}

void Hacks::UnlimitedAmmo()
{
	static bool ammoState = false;

	if (!manager->pConfig->unlimitedAmmo.enabled)
	{
		if (ammoState)
		{
			for (const auto& trackedWeaponEntry : g_trackedAmmoWeapons)
			{
				TryRestoreWeaponAmmoState(trackedWeaponEntry.second);
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

void Hacks::InstantLockpick()
{
	if (!manager->pConfig->instantLockpick.enabled)
		return;
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;
	manager->actorRegistry.Refresh(true);

	__try
	{
		if (Vars::CharacterClass->HoldingActor &&
			Vars::CharacterClass->HoldingActor->IsA(SDK::ALock_pick_C::StaticClass()))
		{
			auto* heldLockpick = static_cast<SDK::ALock_pick_C*>(Vars::CharacterClass->HoldingActor);
			if (heldLockpick->Lock && TryCompleteActiveLockpick(heldLockpick->Lock))
				return;
		}
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	{
	}

	bool completed = false;
	ForEachValidActor(manager->actorRegistry.GetLocks(), [&](SDK::AActor* actor)
	{
		if (!completed)
			completed = TryCompleteActiveLockpick(static_cast<SDK::ALock_C*>(actor));
	});
}

void Hacks::UnlockDoors()
{
	if (!manager->pConfig->unlockDoors.enabled)
		return;
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;
	manager->actorRegistry.Refresh(true);
	ForEachValidActor(manager->actorRegistry.GetDoors(), [](SDK::AActor* actor)
	{
		auto* door = static_cast<SDK::ADoorBP_C*>(actor);
		door->Locked_ = false;
		door->PowerLocked_ = false;
	});

	manager->pConfig->unlockDoors.enabled = false;
}

void Hacks::DisableAlarms()
{
	if (!manager->pConfig->disableAlarms.enabled)
		return;
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;
	manager->actorRegistry.Refresh(true);
	ForEachValidActor(manager->actorRegistry.GetAlarms(), [](SDK::AActor* actor)
	{
		auto* alarm = static_cast<SDK::AAlarmBP_C*>(actor);
		alarm->AlarmEnabled_ = false;
		alarm->HasAlarmTriggered_ = false;
	});

	manager->pConfig->disableAlarms.enabled = false;
}

void Hacks::TeleportExploits()
{
	if (!manager->pConfig->teleportExploits.killCivilians && !manager->pConfig->teleportExploits.killRats &&
		!manager->pConfig->teleportExploits.killPolice && !manager->pConfig->teleportExploits.killDoors &&
		!manager->pConfig->teleportExploits.killCameras &&
		!manager->pConfig->teleportExploits.moveMoneyToTruck)
		return;
	manager->actorRegistry.Refresh(true);

	SDK::ARobberTruck_C* truck = nullptr;
	if (manager->pConfig->teleportExploits.moveMoneyToTruck)
		truck = FindRobberTruck(manager->actorRegistry);

	int truckMoneyIndex = 0;
	if (manager->pConfig->teleportExploits.moveMoneyToTruck && truck)
	{
		ForEachValidActor(manager->actorRegistry.GetMoney(), [&](SDK::AActor* actor)
		{
			auto* money = static_cast<SDK::AMoney_base_C*>(actor);
			if (money->Value > 0 && !IsPickupHeld(money))
			{
				actor->K2_TeleportTo(GetTruckMoneyDropLocation(truck, truckMoneyIndex++), actor->K2_GetActorRotation());
			}
		});

		ForEachValidActor(manager->actorRegistry.GetDuffelbags(), [&](SDK::AActor* actor)
		{
			auto* duffelbag = static_cast<SDK::ADuffelbag_C*>(actor);
			if (duffelbag->AttachedActors.Num() > 0 && !IsPickupHeld(duffelbag))
			{
				actor->K2_TeleportTo(GetTruckMoneyDropLocation(truck, truckMoneyIndex++), actor->K2_GetActorRotation());
			}
		});
	}

	const SDK::FVector teleLocation = { -9999.f, 9999.f, 9999.f };
	const SDK::FRotator teleRotation{ 0.0f, 0.0f, 0.0f };

	auto teleportActors = [&](const std::vector<SDK::AActor*>& actors)
	{
		ForEachValidActor(actors, [&](SDK::AActor* actor)
		{
			actor->K2_TeleportTo(teleLocation, teleRotation);
		});
	};

	if (manager->pConfig->teleportExploits.killCivilians)
		teleportActors(manager->actorRegistry.GetCivilians());
	if (manager->pConfig->teleportExploits.killRats)
		teleportActors(manager->actorRegistry.GetRats());
	if (manager->pConfig->teleportExploits.killPolice)
	{
		teleportActors(manager->actorRegistry.GetGuards());
		teleportActors(manager->actorRegistry.GetPolice());
	}
	if (manager->pConfig->teleportExploits.killDoors)
		teleportActors(manager->actorRegistry.GetDoors());
	if (manager->pConfig->teleportExploits.killCameras)
		teleportActors(manager->actorRegistry.GetCameras());

	manager->pConfig->teleportExploits.killCivilians = false;
	manager->pConfig->teleportExploits.killPolice = false;
	manager->pConfig->teleportExploits.killRats = false;
	manager->pConfig->teleportExploits.killDoors = false;
	manager->pConfig->teleportExploits.killCameras = false;
	manager->pConfig->teleportExploits.moveMoneyToTruck = false;
}

void Hacks::TieUpCivilians()
{
	if (!manager->pConfig->tieUpCivilians.enabled)
		return;

	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;
	manager->actorRegistry.Refresh(true);
	ForEachValidActor(manager->actorRegistry.GetCivilians(), [](SDK::AActor* actor)
	{
		auto* npc = static_cast<SDK::ANPCBase_C*>(actor);
		npc->TiedUp_ = true;
		if (npc->CharacterMovement)
			npc->CharacterMovement->DisableMovement();
	});

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

void Hacks::MaxHealth()
{
	static int maxHealthFrameCounter = 0;

	if (!manager->pConfig->maxHealth.enabled)
	{
		maxHealthFrameCounter = 0;
		return;
	}

	maxHealthFrameCounter++;
	if (maxHealthFrameCounter < 30)
		return;
	maxHealthFrameCounter = 0;

	const int maxHealth = Vars::CharacterClass->MaxHealth;
	if (maxHealth <= 0)
		return;

	if (Vars::CharacterClass->Health < maxHealth)
		Vars::CharacterClass->Health = maxHealth;
}

void Hacks::MaxArmor()
{
	static int maxArmorFrameCounter = 0;

	if (!manager->pConfig->maxArmor.enabled)
	{
		maxArmorFrameCounter = 0;
		return;
	}

	maxArmorFrameCounter++;
	if (maxArmorFrameCounter < 30)
		return;
	maxArmorFrameCounter = 0;

	SDK::AArmor_Light_C* armor = GetEquippedArmorActor();
	if (!armor)
		return;
	if (armor->Destroyed_)
		return;
	if (armor->ArmorMaxHealth <= 0)
		return;

	if (armor->ArmorHealth < armor->ArmorMaxHealth)
		armor->ArmorHealth = armor->ArmorMaxHealth;
}
