#pragma once

#include <Windows.h>

class Config
{
public:
	void Hotkeys();

	struct Menu
	{
		bool enabled{ false };
		bool injected{ true };
		int keyEnable{ VK_INSERT };
		int keyUnload{ VK_DELETE };
	} menu;

	struct Speed
	{
		bool enabled{ false };
		float speed{ 2500.f };
		int keyEnable{ VK_F1 };
	} speed;

	struct JumpHack
	{
		bool enabled{ false };
		int value{ 300 };
		int keyEnable{ VK_F2 };
	} jumpHack;

	struct Bhop
	{
		bool enabled{ false };
	} bhop;

	struct FlyHack
	{
		bool enabled{ false };
		int keyEnable{ VK_F3 };
	} flyHack;

	struct NoclipHack
	{
		bool enabled{ false };
	} noclip;

	struct ThirdPerson
	{
		bool enabled{ false };
		float back{ 360.f };
		float right{ 0.f };
		float up{ 90.f };
	} thirdPerson;

	struct LevelHack
	{
		bool setLevel{ false };
		int level = 1;
	} levelHack;

	struct DisableCameras
	{
		bool enabled{ false };
	} disableCameras;

	struct CashHack
	{
		bool setCash{ false };
		int cashValue{ 12345 };
	} cashHack;



	struct UnlimitedAmmo
	{
		bool enabled{ false };
	} unlimitedAmmo;

	struct RapidFire
	{
		bool enabled{ false };
	} rapidFire;

	struct InstantReload
	{
		bool enabled{ false };
	} instantReload;

	struct Multishot
	{
		bool enabled{ false };
	} multishot;

	struct TieUpCivilians
	{
		bool enabled{ false };
	} tieUpCivilians;

	struct UnlockDoors
	{
		bool enabled{ false };
	} unlockDoors;

	struct DisableAlarms
	{
		bool enabled{ false };
	} disableAlarms;

	struct InstantLockpick
	{
		bool enabled{ false };
	} instantLockpick;

	struct TeleportExploits
	{
		bool killRats{ false };
		bool killCivilians{ false };
		bool killPolice{ false };
		bool killDoors{ false };
		bool killCameras{ false };
		bool moveMoneyToTruck{ false };
	} teleportExploits;


	struct Invulnerable
	{
		bool enabled{ false };
	} invulnerable;

	struct MaxHealth
	{
		bool enabled{ false };
	} maxHealth;

	struct MaxArmor
	{
		bool enabled{ false };
	} maxArmor;

	struct Settings
	{
		bool filterDormant{ true };
	} settings;

	struct Aimbot
	{
		bool enabled{ false };
		float fov{ 30.f };
		bool showFov{ true };
	} aimbot;

	struct DebugEsp
	{
		bool enabled{ false };
		float maxDistance{ 1000.0f };
	} debugEsp;

	struct Esp
	{
		bool visibilityCheckEnabled{ false };
		float defaultBoxColor[4]{ 1.0f, 1.0f, 1.0f, 210.0f / 255.0f };
		float hiddenBoxColor[4]{ 0.0f, 1.0f, 0.0f, 210.0f / 255.0f };
		float visibleBoxColor[4]{ 1.0f, 0.0f, 0.0f, 210.0f / 255.0f };
		float glowColor[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
		bool policeEspEnabled{ false };
		bool policeGlowEnabled{ true };
		bool policeBox2DEnabled{ false };
		bool policeBox3DEnabled{ false };
		bool cameraEspEnabled{ false };
		bool cameraGlowEnabled{ true };
		bool cameraBox2DEnabled{ false };
		bool cameraBox3DEnabled{ false };
		bool playerEspEnabled{ false };
		bool playerGlowEnabled{ true };
		bool playerBox2DEnabled{ false };
		bool playerBox3DEnabled{ false };
		bool ratEspEnabled{ false };
		bool ratGlowEnabled{ true };
		bool ratBox2DEnabled{ false };
		bool ratBox3DEnabled{ false };
		bool bulletTracersEnabled{ false };
	} esp;
};
