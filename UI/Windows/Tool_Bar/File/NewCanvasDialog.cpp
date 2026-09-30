#include "NewCanvasDialog.h"
#include "CanvasManager.h"
#include "imgui.h"

NewCanvasDialog::NewCanvasDialog() {}

void NewCanvasDialog::open() {
    is_open_ = true;
}

void NewCanvasDialog::render(CanvasManager& canvas_manager) {
    if (!is_open_) return;

    ImGui::OpenPopup("新しいキャンバス");

    // ダイアログ（モーダルウィンドウ）の表示
    if (ImGui::BeginPopupModal("新しいキャンバス", &is_open_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("キャンバス解像度を設定してください:");
        ImGui::Separator();

        // 幅・高さの手動入力
        ImGui::InputInt("幅 (px)", &input_width_, 100, 500);
        ImGui::InputInt("高さ (px)", &input_height_, 100, 500);

        // 値のバリデーション
        if (input_width_ < 100) input_width_ = 100;
        if (input_height_ < 100) input_height_ = 100;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 「作ります」ボタン
        if (ImGui::Button("作る", ImVec2(120, 0))) {
            canvas_manager.create_new_canvas(input_width_, input_height_);
            is_open_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();

        // 「キャンセル」ボタン
        if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
            is_open_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}