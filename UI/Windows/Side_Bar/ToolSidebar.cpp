#include "ToolSidebar.h"

namespace {
constexpr float kButtonSize = 52.0f;
constexpr float kButtonGap = 7.0f;

void draw_tool_icon(ImDrawList* draw_list, int icon, const ImVec2& center, ImU32 foreground, const ImVec4& active_color) {
    if (icon == 0) {
        draw_list->AddCircle(center, 17.0f, foreground, 0, 2.0f);
        draw_list->AddCircleFilled(ImVec2(center.x - 4.0f, center.y - 3.0f), 4.0f, IM_COL32(245, 105, 116, 255));
        draw_list->AddCircleFilled(ImVec2(center.x + 6.0f, center.y - 7.0f), 3.5f, IM_COL32(245, 190, 88, 255));
        draw_list->AddCircleFilled(ImVec2(center.x + 8.0f, center.y + 4.0f), 3.5f, IM_COL32(93, 205, 164, 255));
        draw_list->AddCircleFilled(ImVec2(center.x - 3.0f, center.y + 7.0f), 3.5f, IM_COL32(104, 157, 246, 255));
        draw_list->AddCircleFilled(ImVec2(center.x - 13.0f, center.y - 12.0f), 4.0f,
            IM_COL32(static_cast<int>(active_color.x * 255.0f), static_cast<int>(active_color.y * 255.0f), static_cast<int>(active_color.z * 255.0f), 255));
        return;
    }

    if (icon == 1) {
        draw_list->AddLine(ImVec2(center.x - 9.0f, center.y + 10.0f), ImVec2(center.x + 8.0f, center.y - 8.0f), foreground, 7.0f);
        draw_list->AddLine(ImVec2(center.x - 13.0f, center.y + 14.0f), ImVec2(center.x - 7.0f, center.y + 8.0f), IM_COL32(235, 181, 129, 255), 5.0f);
        draw_list->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f), ImVec2(center.x + 11.0f, center.y - 11.0f), IM_COL32(140, 148, 166, 255), 2.0f);
        return;
    }

    if (icon == 2) {
        const ImVec2 a(center.x - 13.0f, center.y - 2.0f);
        const ImVec2 b(center.x - 2.0f, center.y - 13.0f);
        const ImVec2 c(center.x + 13.0f, center.y + 2.0f);
        const ImVec2 d(center.x + 2.0f, center.y + 13.0f);
        draw_list->AddQuadFilled(a, b, c, d, IM_COL32(224, 112, 151, 255));
        draw_list->AddQuadFilled(a, ImVec2(center.x - 7.0f, center.y + 4.0f), ImVec2(center.x + 2.0f, center.y + 13.0f), d, IM_COL32(248, 200, 216, 255));
        draw_list->AddLine(ImVec2(center.x - 7.0f, center.y + 4.0f), ImVec2(center.x + 2.0f, center.y + 13.0f), IM_COL32(255, 235, 241, 255), 1.5f);
        return;
    }

    draw_list->AddLine(ImVec2(center.x - 12.0f, center.y - 8.0f), ImVec2(center.x + 12.0f, center.y - 8.0f), foreground, 2.0f);
    draw_list->AddLine(ImVec2(center.x - 8.0f, center.y), ImVec2(center.x + 8.0f, center.y), foreground, 4.0f);
    draw_list->AddLine(ImVec2(center.x - 4.0f, center.y + 8.0f), ImVec2(center.x + 4.0f, center.y + 8.0f), foreground, 7.0f);
}
}

void ToolSidebar::render(const ImVec2& viewport_pos, const ImVec2& viewport_size) {
    const float window_width = 78.0f;
    const float window_height = 4.0f * kButtonSize + 3.0f * kButtonGap + 26.0f;
    const ImVec2 sidebar_pos(viewport_pos.x + 18.0f, viewport_pos.y + (viewport_size.y - window_height) * 0.5f);

    ImGui::SetNextWindowPos(sidebar_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, kButtonGap));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.085f, 0.11f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.23f, 0.26f, 0.32f, 0.9f));

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNavFocus;
    ImGui::Begin("##FloatingToolSidebar", nullptr, flags);

    const auto button = [this](const char* id, int icon, bool selected, const ImVec4& icon_color) {
        const bool clicked = ImGui::InvisibleButton(id, ImVec2(kButtonSize, kButtonSize));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        if (selected || hovered) {
            const ImU32 fill = selected ? IM_COL32(118, 143, 220, 70) : IM_COL32(255, 255, 255, 20);
            draw_list->AddRectFilled(min, max, fill, 15.0f);
        }
        const ImU32 foreground = selected ? IM_COL32(244, 246, 255, 255) : IM_COL32(193, 201, 216, 255);
        draw_tool_icon(draw_list, icon, center, foreground, icon_color);
        return clicked;
    };

    if (button("##ColorTool", 0, false, selected_color_)) {
        ImGui::OpenPopup("##ColorPickerPopup");
    }
    if (button("##BrushTool", 1, selected_pen_type_ == 0, selected_color_)) {
        selected_pen_type_ = 0;
    }
    if (button("##EraserTool", 2, selected_pen_type_ == 1, selected_color_)) {
        selected_pen_type_ = 1;
    }
    if (button("##BrushSizeTool", 3, false, selected_color_)) {
        ImGui::OpenPopup("##BrushSizePopup");
    }

    ImGui::SetNextWindowPos(ImVec2(sidebar_pos.x + window_width + 12.0f, sidebar_pos.y + 12.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(286.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##ColorPickerPopup", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::ColorPicker4("##ColorSquare", &selected_color_.x,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoTooltip |
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueBar);
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImVec2(sidebar_pos.x + window_width + 12.0f, sidebar_pos.y + window_height - 212.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(78.0f, 220.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("##BrushSizePopup", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::VSliderInt("##BrushSizeSlider", ImVec2(32.0f, 188.0f), &brush_size_, 1, 256, "", ImGuiSliderFlags_NoInput);
        ImGui::EndPopup();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}
