#include "Manager.h"

#include <cstring>
#include <iostream>

Manager::Manager() noexcept :
	pGui(std::make_unique<Gui>()),
	pConfig(std::make_unique<Config>()),
	pHacks(std::make_unique<Hacks>()),
	pEsp(std::make_unique<Esp>())
{
}

void Manager::ClearSDK()
{
	actorRegistry.Clear();
	Vars::World = nullptr;
	Vars::Engine = nullptr;
	Vars::MyController = nullptr;
	Vars::MyPawn = nullptr;
	Vars::CharacterClass = nullptr;
}

bool Manager::UpdateSDK()
{
	auto reportStatus = [this](const char* status)
	{
		if (!lastSdkStatus || std::strcmp(lastSdkStatus, status) != 0)
		{
			lastSdkStatus = status;
			std::cout << "[SDK] " << status << std::endl;
		}
	};

	Vars::World = SDK::UWorld::GetWorld();
	if (Fns::IsNullPointer(Vars::World))
	{
		reportStatus("Not ready: World null");
		ClearSDK();
		return false;
	}

	Vars::Engine = SDK::UEngine::GetEngine();

	{
		if (Fns::IsNullPointer(Vars::World->OwningGameInstance))
		{
			reportStatus("Not ready: OwningGameInstance null");
			ClearSDK();
			return false;
		}
		if (Vars::World->OwningGameInstance->LocalPlayers.Num() == 0 ||
			Fns::IsNullPointer(Vars::World->OwningGameInstance->LocalPlayers[0]))
		{
			reportStatus("Not ready: LocalPlayers empty or null");
			ClearSDK();
			return false;
		}
		Vars::MyController = Vars::World->OwningGameInstance->LocalPlayers[0]->PlayerController;
		if (Fns::IsNullPointer(Vars::MyController))
		{
			reportStatus("Not ready: PlayerController null");
			ClearSDK();
			return false;
		}
	}

	{
		if (Fns::IsNullPointer(Vars::World->GameState))
		{
			reportStatus("Not ready: GameState null");
			ClearSDK();
			return false;
		}
		Vars::PlayerArray = Vars::World->GameState->PlayerArray;
	}

	{
		Vars::MyPawn = Vars::MyController->AcknowledgedPawn;
		if (Vars::MyPawn == nullptr)
		{
			reportStatus("Not ready: AcknowledgedPawn null");
			ClearSDK();
			return false;
		}

		if (!Vars::MyPawn->IsA(SDK::APlayerCharacter_C::StaticClass()))
		{
			reportStatus("Not ready: Pawn is not APlayerCharacter_C");
			ClearSDK();
			return false;
		}

		Vars::CharacterClass = static_cast<SDK::APlayerCharacter_C*>(Vars::MyPawn);
	}

	reportStatus("Ready");
	actorRegistry.Refresh();
	return true;
}

void Manager::DumpUObjects()
{
	std::cout << Vars::Engine->ConsoleClass->GetFullName() << std::endl;

	for (int i = 0; i < SDK::UObject::GObjects->Num(); i++)
	{
		const SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);

		if (!Obj)
			continue;

		if (!Obj->IsDefaultObject())
			continue;

		if (Obj->IsA(SDK::APawn::StaticClass()) || Obj->HasTypeFlag(SDK::EClassCastFlags::Pawn))
		{
			std::cout << Obj->GetFullName() << "\n";
		}
	}
}
