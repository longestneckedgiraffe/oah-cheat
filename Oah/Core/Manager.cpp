#include "Manager.h"

Manager::Manager() noexcept :
	pGui(std::make_unique<Gui>()),
	pConfig(std::make_unique<Config>()),
	pHacks(std::make_unique<Hacks>()),
	pEsp(std::make_unique<Esp>())
{
}

void Manager::ClearSDK()
{
	Vars::World = nullptr;
	Vars::Engine = nullptr;
	Vars::MyController = nullptr;
	Vars::MyPawn = nullptr;
	Vars::CharacterClass = nullptr;
}

bool Manager::UpdateSDK()
{
	Vars::World = SDK::UWorld::GetWorld();
	if (Fns::IsBadPoint(Vars::World))
	{
		ClearSDK();
		return false;
	}

	Vars::Engine = SDK::UEngine::GetEngine();

	{
		if (Fns::IsBadPoint(Vars::World->OwningGameInstance))
		{
			ClearSDK();
			return false;
		}
		if (Vars::World->OwningGameInstance->LocalPlayers.Num() == 0 ||
			Fns::IsBadPoint(Vars::World->OwningGameInstance->LocalPlayers[0]))
		{
			ClearSDK();
			return false;
		}
		Vars::MyController = Vars::World->OwningGameInstance->LocalPlayers[0]->PlayerController;
		if (Fns::IsBadPoint(Vars::MyController))
		{
			ClearSDK();
			return false;
		}
	}

	{
		if (Fns::IsBadPoint(Vars::World->GameState))
		{
			ClearSDK();
			return false;
		}
		Vars::PlayerArray = Vars::World->GameState->PlayerArray;
	}

	{
		Vars::MyPawn = Vars::MyController->AcknowledgedPawn;
		if (Vars::MyPawn == nullptr)
		{
			ClearSDK();
			return false;
		}

		if (!Vars::MyPawn->IsA(SDK::APlayerCharacter_C::StaticClass()))
		{
			ClearSDK();
			return false;
		}

		Vars::CharacterClass = static_cast<SDK::APlayerCharacter_C*>(Vars::MyPawn);
	}
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
