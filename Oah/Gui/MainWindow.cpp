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

	void DrawEspToggleRow(const char* label, bool& enabled, bool& glow, bool& box2D, bool& box3D)
	{
		ImGui::PushID(label);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);

		ImGui::TableSetColumnIndex(1);
		ImGui::Checkbox("##enabled", &enabled);

		ImGui::TableSetColumnIndex(2);
		ImGui::Checkbox("##glow", &glow);

		ImGui::TableSetColumnIndex(3);
		ImGui::Checkbox("##box2d", &box2D);

		ImGui::TableSetColumnIndex(4);
		ImGui::Checkbox("##box3d", &box3D);
		ImGui::PopID();
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
					ImGui::Checkbox("Enable Aimbot", &manager->pConfig->aimbot.enabled);
					if (manager->pConfig->aimbot.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("Field of View", &manager->pConfig->aimbot.fov, 5.0f, 60.0f, "%.0f deg");
						ImGui::Checkbox("Draw FOV Circle", &manager->pConfig->aimbot.showFov);
						ImGui::Text("Hold XBUTTON1 while aiming.");
					}

					ImGui::Separator();
					ImGui::Checkbox("Invulnerable", &manager->pConfig->invulnerable.enabled);
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##combatRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Unlimited Ammo", &manager->pConfig->unlimitedAmmo.enabled);
					ImGui::Checkbox("Rapid Fire", &manager->pConfig->rapidFire.enabled);
					ImGui::Checkbox("Instant Reload", &manager->pConfig->instantReload.enabled);
					ImGui::Checkbox("Multishot", &manager->pConfig->multishot.enabled);
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
						ImGui::TableSetupColumn("Show");
						ImGui::TableSetupColumn("Glow");
						ImGui::TableSetupColumn("2D");
						ImGui::TableSetupColumn("3D");
						ImGui::TableHeadersRow();

						DrawEspToggleRow(
							"Police and Guards",
							manager->pConfig->esp.policeEspEnabled,
							manager->pConfig->esp.policeGlowEnabled,
							manager->pConfig->esp.policeBox2DEnabled,
							manager->pConfig->esp.policeBox3DEnabled);
						DrawEspToggleRow(
							"Players",
							manager->pConfig->esp.playerEspEnabled,
							manager->pConfig->esp.playerGlowEnabled,
							manager->pConfig->esp.playerBox2DEnabled,
							manager->pConfig->esp.playerBox3DEnabled);
						DrawEspToggleRow(
							"Cameras",
							manager->pConfig->esp.cameraEspEnabled,
							manager->pConfig->esp.cameraGlowEnabled,
							manager->pConfig->esp.cameraBox2DEnabled,
							manager->pConfig->esp.cameraBox3DEnabled);
						DrawEspToggleRow(
							"Rats",
							manager->pConfig->esp.ratEspEnabled,
							manager->pConfig->esp.ratGlowEnabled,
							manager->pConfig->esp.ratBox2DEnabled,
							manager->pConfig->esp.ratBox3DEnabled);

						ImGui::EndTable();
					}
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##visualsRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Bullet Tracers", &manager->pConfig->esp.bulletTracersEnabled);
					ImGui::Separator();
					ImGui::Checkbox("Dormant", &manager->pConfig->settings.filterDormant);

#ifdef _DEBUG
					ImGui::Separator();
					ImGui::Checkbox("Object Viewer", &manager->pConfig->debugEsp.enabled);
					if (manager->pConfig->debugEsp.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("Render Distance", &manager->pConfig->debugEsp.maxDistance, 500.0f, 10000.0f, "%.0f");
					}
#endif
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
					ImGui::Checkbox("Speed Hack", &manager->pConfig->speed.enabled);
					if (manager->pConfig->speed.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderFloat("Walk Speed", &manager->pConfig->speed.speed, 300.0f, 2000.0f, "%.0f");
					}

					ImGui::Checkbox("Jump Hack", &manager->pConfig->jumpHack.enabled);
					if (manager->pConfig->jumpHack.enabled)
					{
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::SliderInt("Jump Strength", &manager->pConfig->jumpHack.value, 300, 800);
					}
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##movementRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::Checkbox("Fly Hack", &manager->pConfig->flyHack.enabled);
					ImGui::Checkbox("Noclip", &manager->pConfig->noclip.enabled);
					ImGui::Text("Toggle with XBUTTON2.");
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
					ImGui::Checkbox("Disable Cameras", &manager->pConfig->disableCameras.enabled);
					ImGui::Checkbox("Disable Guard Check-In", &manager->pConfig->guardPhoneDelay.enabled);

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
						ImGui::Dummy(ImVec2(0.0f, 0.0f));

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
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("Target Level", &manager->pConfig->levelHack.level, 1, 10);
					ClampInt(manager->pConfig->levelHack.level, 0, 9999);
					if (ImGui::Button("Apply Level", ImVec2(-FLT_MIN, 0.0f)))
						manager->pConfig->levelHack.setLevel = true;
					ImGui::EndChild();

					ImGui::TableNextColumn();
					ImGui::BeginChild("##progressionRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::InputInt("Target Cash", &manager->pConfig->cashHack.cashValue, 1000, 100000);
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
