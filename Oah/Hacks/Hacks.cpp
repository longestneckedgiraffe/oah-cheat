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
			if (Fns::IsNullPointer(actor))
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
		std::int32_t objectKey{ Fns::InvalidObjectKey };
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

	struct CharacterMutationState
	{
		SDK::APlayerCharacter_C* character{};
		SDK::UCharacterMovementComponent* movement{};
		float maxWalkSpeed{};
		float maxAcceleration{};
		float jumpZVelocity{};
		float maxFlySpeed{};
		SDK::EMovementMode movementMode{};
		std::uint8_t customMovementMode{};
		bool actorEnableCollision{};
		int damageImmunity{};
		float drillImmunityTime{};
		bool initialized{ false };
	};

	struct CameraIgnoredState
	{
		std::int32_t objectKey{ Fns::InvalidObjectKey };
		SDK::ACameraBP_C* camera{};
		bool ignored{};
	};

	struct WeaponModState
	{
		std::int32_t objectKey{ Fns::InvalidObjectKey };
		std::int32_t characterObjectKey{ Fns::InvalidObjectKey };
		SDK::AGunBase_C* weapon{};
		SDK::APlayerCharacter_C* character{};
		float coolDownTime{};
		bool canShoot{};
		bool autoFire{};
		int bulletAmount{};
		bool reloading{};
	};

	static std::unordered_map<std::int32_t, TrackedWeaponAmmoState> g_trackedAmmoWeapons;
	static std::unordered_map<std::int32_t, CameraIgnoredState> g_trackedCameraIgnoredStates;
	static std::unordered_map<std::int32_t, WeaponModState> g_trackedWeaponModStates;
	static ThirdPersonState g_thirdPersonState;
	static CharacterMutationState g_characterMutationState;
	static bool g_disableCamerasState = false;
	static int g_disableCamerasFrameCounter = 0;
	static bool g_speedHackState = false;
	static bool g_bhopWasJumping = false;
	static bool g_flyHackState = false;
	static bool g_noclipState = false;
	static bool g_unlimitedAmmoState = false;
	static SDK::FName g_headBone = SDK::FName();
	static bool g_headBoneInitialized = false;
	static bool g_jumpHackState = false;
	static SDK::ALock_C* g_trackedLock = nullptr;
	static SDK::AActor* g_lastHoldingActor = nullptr;
	static bool g_invulnerableState = false;
	static int g_invulnerableFrameCounter = 0;
	static int g_maxHealthFrameCounter = 0;
	static int g_maxArmorFrameCounter = 0;

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

	bool CaptureCharacterMutationStateIfNeeded()
	{
		if (!Vars::CharacterClass || !Vars::CharacterClass->CharacterMovement)
			return false;

		if (g_characterMutationState.initialized &&
			g_characterMutationState.character == Vars::CharacterClass &&
			g_characterMutationState.movement == Vars::CharacterClass->CharacterMovement)
		{
			return true;
		}

		g_characterMutationState = {};

		__try
		{
			g_characterMutationState.character = Vars::CharacterClass;
			g_characterMutationState.movement = Vars::CharacterClass->CharacterMovement;
			g_characterMutationState.maxWalkSpeed = Vars::CharacterClass->CharacterMovement->MaxWalkSpeed;
			g_characterMutationState.maxAcceleration = Vars::CharacterClass->CharacterMovement->MaxAcceleration;
			g_characterMutationState.jumpZVelocity = Vars::CharacterClass->CharacterMovement->JumpZVelocity;
			g_characterMutationState.maxFlySpeed = Vars::CharacterClass->CharacterMovement->MaxFlySpeed;
			g_characterMutationState.movementMode = Vars::CharacterClass->CharacterMovement->MovementMode;
			g_characterMutationState.customMovementMode = Vars::CharacterClass->CharacterMovement->CustomMovementMode;
			g_characterMutationState.actorEnableCollision = Vars::CharacterClass->bActorEnableCollision;
			g_characterMutationState.damageImmunity = Vars::CharacterClass->DamageImmunity;
			g_characterMutationState.drillImmunityTime = Vars::CharacterClass->DrillImmunityTime;
			g_characterMutationState.initialized = true;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			g_characterMutationState = {};
			return false;
		}
	}

	void RestoreCharacterMutationState()
	{
		if (!g_characterMutationState.initialized)
			return;

		__try
		{
			if (g_characterMutationState.character)
			{
				g_characterMutationState.character->SetActorEnableCollision(g_characterMutationState.actorEnableCollision);
				g_characterMutationState.character->DamageImmunity = g_characterMutationState.damageImmunity;
				g_characterMutationState.character->DrillImmunityTime = g_characterMutationState.drillImmunityTime;
			}

			if (g_characterMutationState.movement)
			{
				g_characterMutationState.movement->MaxWalkSpeed = g_characterMutationState.maxWalkSpeed;
				g_characterMutationState.movement->MaxAcceleration = g_characterMutationState.maxAcceleration;
				g_characterMutationState.movement->JumpZVelocity = g_characterMutationState.jumpZVelocity;
				g_characterMutationState.movement->MaxFlySpeed = g_characterMutationState.maxFlySpeed;
				g_characterMutationState.movement->SetMovementMode(
					g_characterMutationState.movementMode,
					g_characterMutationState.customMovementMode);
			}
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
		}

		g_characterMutationState = {};
	}

	bool TryReadCameraIgnoredState(SDK::ACameraBP_C* camera, bool& ignored)
	{
		__try
		{
			if (!camera)
				return false;

			ignored = camera->Ignored_;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	void TrackCameraIgnoredStateIfNeeded(SDK::ACameraBP_C* camera)
	{
		if (!camera)
			return;

		const std::int32_t cameraKey = Fns::GetObjectKey(camera);
		if (cameraKey == Fns::InvalidObjectKey)
			return;

		if (g_trackedCameraIgnoredStates.contains(cameraKey))
			return;

		bool ignored = false;
		if (!TryReadCameraIgnoredState(camera, ignored))
			return;

		g_trackedCameraIgnoredStates.emplace(cameraKey, CameraIgnoredState{ cameraKey, camera, ignored });
	}

	void RestoreTrackedCameraIgnoredStates()
	{
		for (const auto& trackedCameraEntry : g_trackedCameraIgnoredStates)
		{
			__try
			{
				SDK::ACameraBP_C* camera = trackedCameraEntry.second.camera;
				if (!camera || !Fns::HasObjectKey(camera, trackedCameraEntry.second.objectKey))
					continue;

				camera->Ignored_ = trackedCameraEntry.second.ignored;
				if (camera->SpotPlayerComponent)
				{
					camera->SpotPlayerComponent->SpottedPlayer = nullptr;
					camera->SpotPlayerComponent->Spot_time = 0.0f;
				}
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
			{
			}
		}

		g_trackedCameraIgnoredStates.clear();
	}

	bool TryReadWeaponModState(SDK::AGunBase_C* weapon, SDK::APlayerCharacter_C* character, WeaponModState& state)
	{
		__try
		{
			if (!weapon || !character)
				return false;

			state.objectKey = Fns::GetObjectKey(weapon);
			state.characterObjectKey = Fns::GetObjectKey(character);
			state.weapon = weapon;
			state.character = character;
			state.coolDownTime = weapon->CoolDownTime;
			state.canShoot = weapon->CanShoot_;
			state.autoFire = weapon->Auto_;
			state.bulletAmount = weapon->BulletAmount;
			state.reloading = character->Reloading_;
			return true;
		}
		__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
		{
			return false;
		}
	}

	void TrackWeaponModStateIfNeeded(SDK::AGunBase_C* weapon, SDK::APlayerCharacter_C* character)
	{
		if (!weapon || !character)
			return;

		const std::int32_t weaponKey = Fns::GetObjectKey(weapon);
		if (weaponKey == Fns::InvalidObjectKey)
			return;

		if (g_trackedWeaponModStates.contains(weaponKey))
			return;

		WeaponModState state{};
		if (!TryReadWeaponModState(weapon, character, state))
			return;

		g_trackedWeaponModStates.emplace(weaponKey, state);
	}

	void RestoreTrackedWeaponModStates()
	{
		for (const auto& trackedWeaponEntry : g_trackedWeaponModStates)
		{
			__try
			{
				const WeaponModState& state = trackedWeaponEntry.second;
				if (state.weapon && Fns::HasObjectKey(state.weapon, state.objectKey))
				{
					state.weapon->CoolDownTime = state.coolDownTime;
					state.weapon->CanShoot_ = state.canShoot;
					state.weapon->Auto_ = state.autoFire;
					state.weapon->BulletAmount = state.bulletAmount;
				}

				if (state.character && Fns::HasObjectKey(state.character, state.characterObjectKey))
					state.character->Reloading_ = state.reloading;
			}
			__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
			{
			}
		}

		g_trackedWeaponModStates.clear();
	}

	SDK::AArmor_Light_C* GetEquippedArmorActor()
	{
		if (!Vars::CharacterClass || !Vars::CharacterClass->ArmorChildActor)
			return nullptr;
		SDK::AActor* childActor = Vars::CharacterClass->ArmorChildActor->ChildActor;
		if (!childActor || !childActor->IsA(SDK::AArmor_Light_C::StaticClass()))
			return nullptr;

		return static_cast<SDK::AArmor_Light_C*>(childActor);
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
			if (!weapon || !Fns::HasObjectKey(weapon, trackedWeapon.objectKey))
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

		const std::int32_t weaponKey = Fns::GetObjectKey(weapon);
		if (weaponKey == Fns::InvalidObjectKey)
			return;

		if (g_trackedAmmoWeapons.contains(weaponKey))
			return;

		WeaponAmmoState state{};
		if (!TryReadWeaponAmmoState(weapon, state))
			return;

		g_trackedAmmoWeapons.emplace(weaponKey, TrackedWeaponAmmoState{ weaponKey, weapon, state });
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

	bool IsLockpickInProgress(SDK::ALock_C* lock)
	{
		__try
		{
			return lock &&
				!lock->Unlocked_ &&
				lock->Tool &&
				lock->Tool->Picking_;
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

void Hacks::DisableAll()
{
	__try
	{
		RestoreThirdPersonState();

		for (const auto& trackedWeaponEntry : g_trackedAmmoWeapons)
			TryRestoreWeaponAmmoState(trackedWeaponEntry.second);
		g_trackedAmmoWeapons.clear();
		RestoreTrackedWeaponModStates();
		RestoreTrackedCameraIgnoredStates();

		if (Vars::CharacterClass && (Vars::CharacterClass->bPressedJump || IsKeyHeld(VK_SPACE)))
			Vars::CharacterClass->StopJumping();

		RestoreCharacterMutationState();
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	{
	}
}

void Hacks::OnWorldChanged()
{
	g_trackedAmmoWeapons.clear();
	g_trackedCameraIgnoredStates.clear();
	g_trackedWeaponModStates.clear();
	g_thirdPersonState = {};
	g_characterMutationState = {};
	g_disableCamerasState = false;
	g_disableCamerasFrameCounter = 0;
	g_speedHackState = false;
	g_bhopWasJumping = false;
	g_flyHackState = false;
	g_noclipState = false;
	g_unlimitedAmmoState = false;
	g_jumpHackState = false;
	g_trackedLock = nullptr;
	g_lastHoldingActor = nullptr;
	g_invulnerableState = false;
	g_invulnerableFrameCounter = 0;
	g_maxHealthFrameCounter = 0;
	g_maxArmorFrameCounter = 0;
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
	if (!g_headBoneInitialized)
	{
		GetStaticName(L"head", g_headBone);
		g_headBoneInitialized = true;
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

			SDK::FVector headLocation = character->Mesh->GetSocketLocation(g_headBone);
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
	if (!manager->pConfig->disableCameras.enabled)
	{
		if (g_disableCamerasState)
		{
			RestoreTrackedCameraIgnoredStates();
			g_disableCamerasState = false;
			g_disableCamerasFrameCounter = 0;
		}
		return;
	}

	g_disableCamerasFrameCounter++;
	if (g_disableCamerasState && g_disableCamerasFrameCounter < 60)
		return;
	g_disableCamerasFrameCounter = 0;
	g_disableCamerasState = true;
	manager->actorRegistry.Refresh(true);

	ForEachValidActor(manager->actorRegistry.GetCameras(), [](SDK::AActor* actor)
	{
		auto* camera = static_cast<SDK::ACameraBP_C*>(actor);
		TrackCameraIgnoredStateIfNeeded(camera);
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
	if (manager->pConfig->speed.enabled)
	{
		if (!CaptureCharacterMutationStateIfNeeded())
			return;

		Vars::CharacterClass->CharacterMovement->MaxWalkSpeed = manager->pConfig->speed.speed;
		Vars::CharacterClass->CharacterMovement->MaxAcceleration = manager->pConfig->speed.speed;
		g_speedHackState = true;
	}
	else if (g_speedHackState)
	{
		if (g_characterMutationState.initialized && g_characterMutationState.movement == Vars::CharacterClass->CharacterMovement)
		{
			Vars::CharacterClass->CharacterMovement->MaxWalkSpeed = g_characterMutationState.maxWalkSpeed;
			Vars::CharacterClass->CharacterMovement->MaxAcceleration = g_characterMutationState.maxAcceleration;
		}
		g_speedHackState = false;
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
	if (!Vars::CharacterClass || !Vars::CharacterClass->CharacterMovement)
		return;

	if (!manager->pConfig->bhop.enabled || !IsKeyHeld(VK_SPACE))
	{
		if (g_bhopWasJumping || Vars::CharacterClass->bPressedJump)
			Vars::CharacterClass->StopJumping();

		g_bhopWasJumping = false;
		return;
	}

	if (Vars::CharacterClass->Downed_ || Vars::CharacterClass->Vaulting_)
	{
		if (g_bhopWasJumping || Vars::CharacterClass->bPressedJump)
			Vars::CharacterClass->StopJumping();

		g_bhopWasJumping = false;
		return;
	}

	if (Vars::CharacterClass->CharacterMovement->IsMovingOnGround() && Vars::CharacterClass->CanJump())
	{
		Vars::CharacterClass->Jump();
		g_bhopWasJumping = true;
	}
	else if (g_bhopWasJumping || Vars::CharacterClass->bPressedJump)
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
	if (manager->pConfig->flyHack.enabled)
	{
		if (!CaptureCharacterMutationStateIfNeeded())
			return;

		Vars::CharacterClass->CharacterMovement->Velocity = SDK::FVector{0, 0, 0};
		Vars::CharacterClass->CharacterMovement->MaxFlySpeed = 600.0f;
		Vars::CharacterClass->CharacterMovement->SetMovementMode(SDK::EMovementMode::MOVE_Flying, 0);

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
		g_flyHackState = true;
	}
	else if(!manager->pConfig->flyHack.enabled && g_flyHackState)
	{
		if (!manager->pConfig->noclip.enabled &&
			g_characterMutationState.initialized &&
			g_characterMutationState.movement == Vars::CharacterClass->CharacterMovement)
		{
			Vars::CharacterClass->CharacterMovement->MaxFlySpeed = g_characterMutationState.maxFlySpeed;
			Vars::CharacterClass->CharacterMovement->SetMovementMode(
				g_characterMutationState.movementMode,
				g_characterMutationState.customMovementMode);
		}
		g_flyHackState = false;
	}
}

void Hacks::Noclip()
{
	if (manager->pConfig->noclip.enabled)
	{
		if (!CaptureCharacterMutationStateIfNeeded())
			return;

		Vars::CharacterClass->SetActorEnableCollision(false);

		if (!manager->pConfig->flyHack.enabled &&
			Vars::CharacterClass->CharacterMovement->MovementMode != SDK::EMovementMode::MOVE_Flying)
		{
			Vars::CharacterClass->CharacterMovement->SetMovementMode(SDK::EMovementMode::MOVE_Flying, 0);
		}

		g_noclipState = true;
	}
	else if (g_noclipState)
	{
		if (g_characterMutationState.initialized && g_characterMutationState.character == Vars::CharacterClass)
			Vars::CharacterClass->SetActorEnableCollision(g_characterMutationState.actorEnableCollision);

		if (!manager->pConfig->flyHack.enabled &&
			g_characterMutationState.initialized &&
			g_characterMutationState.movement == Vars::CharacterClass->CharacterMovement)
		{
			Vars::CharacterClass->CharacterMovement->SetMovementMode(
				g_characterMutationState.movementMode,
				g_characterMutationState.customMovementMode);
		}

		g_noclipState = false;
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
	if (!manager->pConfig->unlimitedAmmo.enabled)
	{
		if (g_unlimitedAmmoState)
		{
			for (const auto& trackedWeaponEntry : g_trackedAmmoWeapons)
			{
				TryRestoreWeaponAmmoState(trackedWeaponEntry.second);
			}

			g_trackedAmmoWeapons.clear();
			g_unlimitedAmmoState = false;
		}
		return;
	}

	g_unlimitedAmmoState = true;
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
	{
		RestoreTrackedWeaponModStates();
		return;
	}
	if (!Vars::CharacterClass->HoldingGun)
		return;

	__try
	{
		TrackWeaponModStateIfNeeded(Vars::CharacterClass->HoldingGun, Vars::CharacterClass);

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

	if (manager->pConfig->jumpHack.enabled)
	{
		if (!CaptureCharacterMutationStateIfNeeded())
			return;

		Vars::CharacterClass->CharacterMovement->JumpZVelocity = static_cast<float>(manager->pConfig->jumpHack.value);
		g_jumpHackState = true;
	}
	else if(g_jumpHackState)
	{
		if (g_characterMutationState.initialized && g_characterMutationState.movement == Vars::CharacterClass->CharacterMovement)
			Vars::CharacterClass->CharacterMovement->JumpZVelocity = g_characterMutationState.jumpZVelocity;
		g_jumpHackState = false;
	}
}

void Hacks::InstantLockpick()
{
	if (!manager->pConfig->instantLockpick.enabled)
	{
		g_trackedLock = nullptr;
		g_lastHoldingActor = nullptr;
		return;
	}
	if (!Vars::World || Vars::World->Levels.Num() == 0)
		return;

	SDK::AActor* holdingActor = Vars::CharacterClass ? Vars::CharacterClass->HoldingActor : nullptr;
	const bool hadHeldLockpick = g_lastHoldingActor && g_lastHoldingActor->IsA(SDK::ALock_pick_C::StaticClass());
	const bool hasHeldLockpick = holdingActor && holdingActor->IsA(SDK::ALock_pick_C::StaticClass());

	__try
	{
		if (hasHeldLockpick)
		{
			auto* heldLockpick = static_cast<SDK::ALock_pick_C*>(holdingActor);
			if (heldLockpick->Lock)
				g_trackedLock = heldLockpick->Lock;

			if (g_trackedLock && TryCompleteActiveLockpick(g_trackedLock))
			{
				g_lastHoldingActor = holdingActor;
				return;
			}
		}
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
	{
	}

	if (g_trackedLock)
	{
		if (TryCompleteActiveLockpick(g_trackedLock))
		{
			g_lastHoldingActor = holdingActor;
			return;
		}

		if (!IsLockpickInProgress(g_trackedLock))
			g_trackedLock = nullptr;
	}

	if (!g_trackedLock && hadHeldLockpick && !hasHeldLockpick)
	{
		ForEachValidActor(manager->actorRegistry.GetLocks(), [&](SDK::AActor* actor)
		{
			if (!g_trackedLock)
			{
				auto* lock = static_cast<SDK::ALock_C*>(actor);
				if (IsLockpickInProgress(lock))
					g_trackedLock = lock;
			}
		});

		if (g_trackedLock)
		{
			if (TryCompleteActiveLockpick(g_trackedLock))
				return;
		}
	}

	g_lastHoldingActor = holdingActor;
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
	if (!manager->pConfig->invulnerable.enabled)
	{
		if (g_invulnerableState)
		{
			if (g_characterMutationState.initialized && g_characterMutationState.character == Vars::CharacterClass)
			{
				Vars::CharacterClass->DamageImmunity = g_characterMutationState.damageImmunity;
				Vars::CharacterClass->DrillImmunityTime = g_characterMutationState.drillImmunityTime;
			}
			g_invulnerableState = false;
		}
		g_invulnerableFrameCounter = 0;
		return;
	}

	g_invulnerableFrameCounter++;
	if (g_invulnerableState && g_invulnerableFrameCounter < 300)
		return;
	g_invulnerableFrameCounter = 0;
	g_invulnerableState = true;

	CaptureCharacterMutationStateIfNeeded();
	Vars::CharacterClass->GiveImmunityLevel(99999.f);
}

void Hacks::MaxHealth()
{
	if (!manager->pConfig->maxHealth.enabled)
	{
		g_maxHealthFrameCounter = 0;
		return;
	}

	g_maxHealthFrameCounter++;
	if (g_maxHealthFrameCounter < 30)
		return;
	g_maxHealthFrameCounter = 0;

	const int maxHealth = Vars::CharacterClass->MaxHealth;
	if (maxHealth <= 0)
		return;

	if (Vars::CharacterClass->Health < maxHealth)
		Vars::CharacterClass->Health = maxHealth;
}

void Hacks::MaxArmor()
{
	if (!manager->pConfig->maxArmor.enabled)
	{
		g_maxArmorFrameCounter = 0;
		return;
	}

	g_maxArmorFrameCounter++;
	if (g_maxArmorFrameCounter < 30)
		return;
	g_maxArmorFrameCounter = 0;

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
