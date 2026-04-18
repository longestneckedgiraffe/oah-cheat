#include <cfloat>
#include <climits>
#include <string>

#include "../Utils/Color.h"
#include "../Core/Manager.h"
#include "../Core/Version.h"

#include "Gui.h"

namespace
{
	void ClampInt(int& value, int minValue, int maxValue)
	{
		if (value < minValue)
			value = minValue;
		else if (value > maxValue)
			value = maxValue;
	}

	void DrawEspToggleRow(const char* label, bool& glow, bool& box2D, bool& box3D, bool& name)
	{
		ImGui::PushID(label);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);

		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##glow", &glow);

		ImGui::TableSetColumnIndex(2);
		ImGui::Checkbox("##box2d", &box2D);

		ImGui::TableSetColumnIndex(3);
		ImGui::Checkbox("##box3d", &box3D);

		ImGui::TableSetColumnIndex(4);
		ImGui::Checkbox("##name", &name);
		ImGui::PopID();
	}

	const char* GetKeybindLabel(int key)
	{
		switch (key)
		{
		case VK_XBUTTON1:
			return "XBUTTON1";
		case VK_XBUTTON2:
			return "XBUTTON2";
		case VK_F1:
			return "F1";
		case VK_F2:
			return "F2";
		case VK_F3:
			return "F3";
		case VK_SPACE:
			return "SPACE";
		default:
			return nullptr;
		}
	}

	void DrawToggleKeybindText(const char* prefix, int key)
	{
		const char* keyLabel = GetKeybindLabel(key);
		if (!keyLabel)
			return;

		ImGui::TextDisabled("%s%s.", prefix, keyLabel);
	}
}

void Gui::RenderMainWindow()
{
	if (!manager->pConfig->menu.enabled)
		return;

	ImVec2 windowSize = ImVec2(760.0f, 680.0f);
	std::string windowName = APP_NAME;
	bool* windowOpen = &manager->pConfig->menu.enabled;

	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);

	if (ImGui::Begin(windowName.c_str(), windowOpen))
	{
		if (ImGui::BeginTabBar("tabs"))
		{
			if (ImGui::BeginTabItem("Combat"))
			{
				if (ImGui::BeginTable("combatLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupColumn("combatLeft", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("combatRight", ImGuiTableColumnFlags_WidthStretch, 1.0f);

					ImGui::TableNextColumn();
					ImGui::BeginChild("##combatLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Aimbot", &manager->pConfig->aimbot.enabled);
					if (manager->pConfig->aimbot.enabled)
					{
						ImGui::TextUnformatted("Field of View");
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##AimbotFov", &manager->pConfig->aimbot.fov, 5.0f, 60.0f, "%.0f deg");
						ImGui::Checkbox("Visible only", &manager->pConfig->aimbot.visibleOnly);
						ImGui::Checkbox("Draw FOV circle", &manager->pConfig->aimbot.showFov);
						DrawToggleKeybindText("Hold ", VK_XBUTTON1);
					}

					ImGui::Separator();
					ImGui::Checkbox("Invulnerable", &manager->pConfig->invulnerable.enabled);
					ImGui::Checkbox("Max health", &manager->pConfig->maxHealth.enabled);
					ImGui::Checkbox("Max armor", &manager->pConfig->maxArmor.enabled);
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##combatRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Unlimited ammo", &manager->pConfig->unlimitedAmmo.enabled);
					ImGui::Checkbox("Rapid fire", &manager->pConfig->rapidFire.enabled);
					ImGui::Checkbox("Instant reload", &manager->pConfig->instantReload.enabled);
					ImGui::Checkbox("Multishot", &manager->pConfig->multishot.enabled);
					ImGui::Separator();
					ImGui::Checkbox("Render tracers", &manager->pConfig->esp.bulletTracersEnabled);
					ImGui::EndChild();

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Visuals"))
			{
				if (ImGui::BeginTable("visualsLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupColumn("visualsLeft", ImGuiTableColumnFlags_WidthStretch, 1.1f);
					ImGui::TableSetupColumn("visualsRight", ImGuiTableColumnFlags_WidthStretch, 0.9f);

					ImGui::TableNextColumn();
					ImGui::BeginChild("##visualsLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					if (ImGui::BeginTable("espRows", 5, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter))
					{
						ImGui::TableSetupColumn("Actor");
						ImGui::TableSetupColumn("Glow");
						ImGui::TableSetupColumn("2D");
						ImGui::TableSetupColumn("3D");
						ImGui::TableSetupColumn("Name");
						ImGui::TableHeadersRow();

						DrawEspToggleRow(
							"Police and Guards",
							manager->pConfig->esp.policeGlowEnabled,
							manager->pConfig->esp.policeBox2DEnabled,
							manager->pConfig->esp.policeBox3DEnabled,
							manager->pConfig->esp.policeNameEnabled);
						DrawEspToggleRow(
							"Players",
							manager->pConfig->esp.playerGlowEnabled,
							manager->pConfig->esp.playerBox2DEnabled,
							manager->pConfig->esp.playerBox3DEnabled,
							manager->pConfig->esp.playerNameEnabled);
						DrawEspToggleRow(
							"Cameras",
							manager->pConfig->esp.cameraGlowEnabled,
							manager->pConfig->esp.cameraBox2DEnabled,
							manager->pConfig->esp.cameraBox3DEnabled,
							manager->pConfig->esp.cameraNameEnabled);
						DrawEspToggleRow(
							"Rats",
							manager->pConfig->esp.ratGlowEnabled,
							manager->pConfig->esp.ratBox2DEnabled,
							manager->pConfig->esp.ratBox3DEnabled,
							manager->pConfig->esp.ratNameEnabled);

						manager->pConfig->esp.policeEspEnabled =
							manager->pConfig->esp.policeGlowEnabled ||
							manager->pConfig->esp.policeBox2DEnabled ||
							manager->pConfig->esp.policeBox3DEnabled ||
							manager->pConfig->esp.policeNameEnabled;
						manager->pConfig->esp.playerEspEnabled =
							manager->pConfig->esp.playerGlowEnabled ||
							manager->pConfig->esp.playerBox2DEnabled ||
							manager->pConfig->esp.playerBox3DEnabled ||
							manager->pConfig->esp.playerNameEnabled;
						manager->pConfig->esp.cameraEspEnabled =
							manager->pConfig->esp.cameraGlowEnabled ||
							manager->pConfig->esp.cameraBox2DEnabled ||
							manager->pConfig->esp.cameraBox3DEnabled ||
							manager->pConfig->esp.cameraNameEnabled;
						manager->pConfig->esp.ratEspEnabled =
							manager->pConfig->esp.ratGlowEnabled ||
							manager->pConfig->esp.ratBox2DEnabled ||
							manager->pConfig->esp.ratBox3DEnabled ||
							manager->pConfig->esp.ratNameEnabled;

						ImGui::EndTable();
					}
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##visualsRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Visibility check", &manager->pConfig->esp.visibilityCheckEnabled);
					ImGui::ColorEdit4("Default Box", manager->pConfig->esp.defaultBoxColor);
					ImGui::ColorEdit4("Hidden Box", manager->pConfig->esp.hiddenBoxColor);
					ImGui::ColorEdit4("Visible Box", manager->pConfig->esp.visibleBoxColor);
					ImGui::Dummy(ImVec2(0.0f, 8.0f));
					ImGui::ColorEdit4("Glow Color", manager->pConfig->esp.glowColor);
					ImGui::Separator();
					ImGui::Checkbox("Filter dead", &manager->pConfig->settings.filterDormant);
					ImGui::EndChild();

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Movement"))
			{
				if (ImGui::BeginTable("movementLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupColumn("movementLeft", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("movementRight", ImGuiTableColumnFlags_WidthStretch, 1.0f);

					ImGui::TableNextColumn();
					ImGui::BeginChild("##movementLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Speed", &manager->pConfig->speed.enabled);
					if (manager->pConfig->speed.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("Walk Speed", &manager->pConfig->speed.speed, 300.0f, 2000.0f, "%.0f");
						DrawToggleKeybindText("Toggle with ", manager->pConfig->speed.keyEnable);
					}

					ImGui::Checkbox("Jump", &manager->pConfig->jumpHack.enabled);
					if (manager->pConfig->jumpHack.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderInt("Jump Strength", &manager->pConfig->jumpHack.value, 300, 800);
						DrawToggleKeybindText("Toggle with ", manager->pConfig->jumpHack.keyEnable);
					}

					ImGui::Checkbox("Bhop", &manager->pConfig->bhop.enabled);
					if (manager->pConfig->bhop.enabled)
						DrawToggleKeybindText("Hold ", VK_SPACE);
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##movementRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Fly", &manager->pConfig->flyHack.enabled);
					if (manager->pConfig->flyHack.enabled)
						DrawToggleKeybindText("Toggle with ", manager->pConfig->flyHack.keyEnable);

					ImGui::Checkbox("Noclip", &manager->pConfig->noclip.enabled);
					if (manager->pConfig->noclip.enabled)
						DrawToggleKeybindText("Toggle with ", VK_XBUTTON2);

					ImGui::Separator();
					ImGui::Checkbox("Third person", &manager->pConfig->thirdPerson.enabled);
					if (manager->pConfig->thirdPerson.enabled)
					{
						ImGui::TextUnformatted("Back");
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##ThirdPersonBack", &manager->pConfig->thirdPerson.back, 25.0f, 400.0f, "%.0f");

						ImGui::TextUnformatted("Right");
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##ThirdPersonRight", &manager->pConfig->thirdPerson.right, -150.0f, 150.0f, "%.0f");

						ImGui::TextUnformatted("Up");
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("##ThirdPersonUp", &manager->pConfig->thirdPerson.up, -50.0f, 200.0f, "%.0f");
					}
					ImGui::EndChild();

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("World"))
			{
				if (ImGui::BeginTable("worldLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupColumn("worldLeft", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("worldRight", ImGuiTableColumnFlags_WidthStretch, 1.15f);

					ImGui::TableNextColumn();
					ImGui::BeginChild("##worldLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Disable cameras", &manager->pConfig->disableCameras.enabled);
					ImGui::Checkbox("Instant lockpick", &manager->pConfig->instantLockpick.enabled);

					ImGui::Separator();
					if (ImGui::Button("Unlock Doors", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->unlockDoors.enabled = true;

					if (ImGui::Button("Disable Alarms", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->disableAlarms.enabled = true;

					if (ImGui::Button("Tie Up Civilians", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->tieUpCivilians.enabled = true;
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##worldRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					if (ImGui::BeginTable("cleanupButtons", 2, ImGuiTableFlags_SizingStretchSame))
					{
						ImGui::TableNextColumn();
						if (ImGui::Button("Delete Civilians", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.killCivilians = true;

						ImGui::TableNextColumn();
						if (ImGui::Button("Delete Rats", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.killRats = true;

						ImGui::TableNextColumn();
						if (ImGui::Button("Delete Police", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.killPolice = true;

						ImGui::TableNextColumn();
						if (ImGui::Button("Delete Cameras", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.killCameras = true;

						ImGui::TableNextColumn();
						if (ImGui::Button("Delete Doors", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.killDoors = true;
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Text("Only deletes doors locally.");
							ImGui::EndTooltip();
						}

						ImGui::TableNextColumn();
						if (ImGui::Button("Move Money To Truck", ImVec2(-FLT_MIN, 0.0f)))
							manager->pConfig->teleportExploits.moveMoneyToTruck = true;

						ImGui::EndTable();
					}
					ImGui::EndChild();

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Progression"))
			{
				if (ImGui::BeginTable("progressionLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
				{
					ImGui::TableSetupColumn("progressionLeft", ImGuiTableColumnFlags_WidthStretch, 1.0f);
					ImGui::TableSetupColumn("progressionRight", ImGuiTableColumnFlags_WidthStretch, 1.0f);

					ImGui::TableNextColumn();
					ImGui::BeginChild("##progressionLeft", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::TextUnformatted("Target Level");
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##TargetLevel", &manager->pConfig->levelHack.level, 0, 0);
					ClampInt(manager->pConfig->levelHack.level, 0, 9999);
					if (ImGui::Button("Apply Level", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->levelHack.setLevel = true;
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##progressionRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::TextUnformatted("Target Cash");
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("##TargetCash", &manager->pConfig->cashHack.cashValue, 0, 0);
					ClampInt(manager->pConfig->cashHack.cashValue, 0, INT_MAX);
					if (ImGui::Button("Apply Cash", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->cashHack.setCash = true;
					ImGui::EndChild();

					ImGui::EndTable();
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}

	ImGui::End();
}
