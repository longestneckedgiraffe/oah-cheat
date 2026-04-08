#define WIN32_LEAN_AND_MEAN
#define NO_MIN_MAX

#include <iostream>
#include <string>

#include "Version.h"
#include "Manager.h"

/*
	An efficient and simple .dll based injectable cheat for the game One Armed Robber
*/

std::unique_ptr<Manager> manager;

static const char* KieroStatusToString(kiero::Status::Enum status)
{
	switch (status)
	{
	case kiero::Status::Success:
		return "Success";
	case kiero::Status::UnknownError:
		return "UnknownError";
	case kiero::Status::NotSupportedError:
		return "NotSupportedError";
	case kiero::Status::ModuleNotFoundError:
		return "ModuleNotFoundError";
	case kiero::Status::AlreadyInitializedError:
		return "AlreadyInitializedError";
	case kiero::Status::NotInitializedError:
		return "NotInitializedError";
	default:
		return "UnmappedStatus";
	}
}

DWORD WINAPI MainThread(HMODULE hmodule)
{
	bool keepConsoleOpen = false;
	const bool consoleCreated = AllocConsole() != FALSE;
	FILE* consoleFile = nullptr;
	if (consoleCreated)
	{
		freopen_s(&consoleFile, "CONOUT$", "w", stdout);
		SetConsoleTitleA(APP_NAME);
	}

	HWND consoleWnd = GetConsoleWindow();
	if (consoleWnd)
	{
		HMENU hMenu = GetSystemMenu(consoleWnd, FALSE);
		if (hMenu)
			DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
	}

	auto logStep = [](const char* step, const char* result)
	{
		std::cout << "[" << step << "] " << result << std::endl;
	};
	auto logStatus = [](const char* step, kiero::Status::Enum status)
	{
		std::cout << "[" << step << "] " << KieroStatusToString(status)
			<< " (" << static_cast<int>(status) << ")" << std::endl;
	};
	auto closeConsoleIfNeeded = [&](bool forceClose)
	{
		if (!forceClose || keepConsoleOpen)
			return;

		if (consoleFile)
		{
			fclose(consoleFile);
			consoleFile = nullptr;
		}

		if (GetConsoleWindow())
			FreeConsole();
	};

	logStep("Console", consoleCreated ? "Allocated" : "Allocation failed");
	logStep("Loader", "Starting initialization");

	bool initHook = false;
	int initAttempts = 0;
	while (!initHook)
	{
		initAttempts++;
		const kiero::Status::Enum initStatus = kiero::init(kiero::RenderType::D3D11);
		if (initStatus == kiero::Status::Success)
		{
			logStatus("Kiero::init", initStatus);

			manager = std::make_unique<Manager>();
			if (!manager || !manager->pGui || !manager->pConfig || !manager->pHacks || !manager->pEsp)
			{
				keepConsoleOpen = true;
				logStep("Manager", "Failed to construct core objects");
				return TRUE;
			}
			logStep("Manager", "Constructed core objects");

			const kiero::Status::Enum bindStatus =
				kiero::bind(8, (void**)&manager->pGui->oPresent, manager->pGui->HkPresent);
			if (bindStatus != kiero::Status::Success)
			{
				keepConsoleOpen = true;
				logStatus("Kiero::bind", bindStatus);
				return TRUE;
			}
			logStatus("Kiero::bind", bindStatus);

			if (manager->UpdateSDK())
				logStep("SDK", "Ready");
			else
				logStep("SDK", "Not ready yet; hook will continue and refresh in Present");

			initHook = true;
		}
		else
		{
			if (initAttempts == 1 || initAttempts % 200 == 0)
				logStatus("Kiero::init", initStatus);
			if (initAttempts > 200)
				keepConsoleOpen = true;
			Sleep(10);
		}
	}

	logStep("Loader", keepConsoleOpen ? "Initialized with hitch; console will remain open" : "Initialization complete");
	if (!keepConsoleOpen)
		Sleep(1000);
	closeConsoleIfNeeded(true);

	while (manager->pConfig->menu.injected)
	{
		if (GetAsyncKeyState(manager->pConfig->menu.keyUnload) & 1)
		{
			manager->pConfig->speed.enabled = false;
			manager->pConfig->flyHack.enabled = false;
			manager->pConfig->noclip.enabled = false;
			manager->pConfig->jumpHack.enabled = false;
			manager->pConfig->disableCameras.enabled = false;
			manager->pConfig->guardPhoneDelay.enabled = false;
			manager->pConfig->invulnerable.enabled = false;
			manager->pConfig->unlimitedAmmo.enabled = false;
			manager->pConfig->rapidFire.enabled = false;
			manager->pConfig->instantReload.enabled = false;
			manager->pConfig->multishot.enabled = false;
			manager->pConfig->aimbot.enabled = false;

			manager->pConfig->esp.policeEspEnabled = false;
			manager->pConfig->esp.playerEspEnabled = false;
			manager->pConfig->esp.cameraEspEnabled = false;
			manager->pConfig->esp.ratEspEnabled = false;

			Sleep(200);

			manager->pEsp->DisableAll();

			manager->pConfig->menu.injected = false;

			for (int i = 0; i < 200 && !manager->pGui->cleanupDone; i++)
				Sleep(10);

			kiero::shutdown();
			Sleep(100);

			if (consoleFile)
			{
				fclose(consoleFile);
				consoleFile = nullptr;
			}
			if (GetConsoleWindow())
				FreeConsole();
			FreeLibraryAndExitThread(hmodule, 0);
		}
		Sleep(10);
	}

	return TRUE;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hMod);
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hMod, 0, nullptr);
		break;
	default:
		break;
	}
	return TRUE;
}
