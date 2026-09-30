#pragma once

#include "imgui.h"
#include <cstdint>

class ToolSidebar {
public:
    void render(const ImVec2& viewport_pos, const ImVec2& viewport_size);

    const ImVec4& selected_color() const { return selected_color_; }
    float brush_size() const { return static_cast<float>(brush_size_); }
    uint32_t selected_pen_type() const { return selected_pen_type_; }

private:
    ImVec4 selected_color_ = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    int brush_size_ = 4;
    uint32_t selected_pen_type_ = 0;
    bool color_picker_open_ = false;
    bool brush_size_open_ = false;
};
