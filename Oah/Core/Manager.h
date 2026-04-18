#pragma once

#include <cstdlib>
#include <memory>

#include "../Gui/Gui.h"
#include "ActorRegistry.h"
#include "Config.h"
#include "../Hacks/Hacks.h"
#include "../Esp/Esp.h"

[[noreturn]] inline void FailFast(const char* message)
{
	MessageBoxA(nullptr, message, "Assertion Failed", MB_OK | MB_ICONERROR);
	std::exit(EXIT_FAILURE);
}

class Manager
{
public:
	Manager() noexcept;

	bool UpdateSDK();
	void ClearSDK();
	void DumpUObjects();

	std::unique_ptr<Gui>    pGui;
	std::unique_ptr<Config> pConfig;
	std::unique_ptr<Hacks>  pHacks;
	std::unique_ptr<Esp>    pEsp;
	ActorRegistry actorRegistry{};

private:
	const char* lastSdkStatus = nullptr;
};

extern std::unique_ptr<Manager> manager;
