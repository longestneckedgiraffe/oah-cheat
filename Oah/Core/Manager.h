#pragma once

#include <memory>

#include "../Gui/Gui.h"
#include "Config.h"
#include "../Hacks/Hacks.h"
#include "../Esp/Esp.h"

#define ASSERT(x) { MessageBoxA(NULL, NULL, x, NULL); exit(0); }

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
};

extern std::unique_ptr<Manager> manager;
