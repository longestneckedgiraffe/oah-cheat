#include <string>

#include "../Utils/Color.h"
#include "Gui.h"
#include "../Core/Manager.h"
#include "../Libs/ImGui/imgui_internal.h"

void Gui::SetupImGuiFonts()
{
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();
	tahomaFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 13.0f);
}

void Gui::SetupImGuiStyle()
{
	ImGui::StyleColorsDark();
}

void Gui::MultiCombo(const char* label, const std::vector<const char*>& titles, const std::vector<bool*>& options, float width)
{
    if (titles.size() != options.size())
    {
        return;
    }

    if (width == 0.f)
        width = ImGui::GetContentRegionAvail().x * 0.5f;
    const float comboDistance = ImGui::GetContentRegionAvail().x - (2 * 2.f + width + ImGui::CalcTextSize(label).x);

    std::string strId = "##";
    strId += label;

    std::string preview = "disabled##";
    for (size_t i = 0; i < options.size(); i++)
    {
        if (*options[i])
        {
            if (preview == "disabled##")
                preview = "";

            preview += titles[i];
            preview.append(", ");
        }
    }

    preview.pop_back();
    preview.pop_back();

    ImGui::Text(label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + comboDistance);
    ImGui::PushItemWidth(width);
    if (ImGui::BeginCombo(strId.c_str(), preview.c_str()))
    {
        for (size_t i = 0; i < titles.size(); i++)
        {
            std::string pTitle = "+ ";
            pTitle += titles[i];
            ImGui::Selectable(*options[i] ? pTitle.c_str() : titles[i], options[i], ImGuiSelectableFlags_DontClosePopups);
        }

        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    const float comboOffset = abs(2.f - IM_TRUNC(ImGui::GetStyle().ItemSpacing.y * 0.5f));
    ImGui::ItemSize(ImVec2(0.f, comboOffset));
}

void Gui::MultiCombo(const char* label, const std::vector<const char*>& titles, const std::vector<int>& values, int* flag)
{
    if (titles.size() != values.size())
    {
        return;
    }

    std::string preview = "none";
    if (*flag)
    {
        preview = "";
        for (size_t i = 0; i < values.size(); i++)
        {
            if (*flag & values[i])
            {
                if (!preview.empty())
                    preview.append(", ");
                preview += titles[i];
            }
        }
    }

    if (ImGui::BeginCombo(label, preview.c_str()))
    {
        for (size_t i = 0; i < titles.size(); i++)
        {
            const bool hasFlag = *flag & values[i];
            if (ImGui::Selectable(titles[i], hasFlag, ImGuiSelectableFlags_DontClosePopups))
            {
                if (hasFlag)
                    *flag &= ~values[i];
                else
                    *flag |= values[i];
            }
        }

        ImGui::EndCombo();
    }
}
