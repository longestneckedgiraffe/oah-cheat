#include <Windows.h>
#include "Config.h"

void Config::Hotkeys()
{
	if (GetAsyncKeyState(menu.keyEnable) & 1)
	{
		menu.enabled = !menu.enabled;
	}
	else if (GetAsyncKeyState(speed.keyEnable) & 1)
	{
		speed.enabled = !speed.enabled;
	}
	else if (GetAsyncKeyState(jumpHack.keyEnable) & 1)
	{
		jumpHack.enabled = !jumpHack.enabled;
	}
	else if (GetAsyncKeyState(flyHack.keyEnable) & 1)
	{
		flyHack.enabled = !flyHack.enabled;
	}
	else if (GetAsyncKeyState(VK_XBUTTON2) & 1)
	{
		noclip.enabled = !noclip.enabled;
	}
}
