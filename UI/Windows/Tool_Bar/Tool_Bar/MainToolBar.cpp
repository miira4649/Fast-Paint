#include "MainToolBar.h"
#include "CanvasManager.h"
#include "imgui.h"
#include "imgui_internal.h" // 追加: BeginViewportSideBar を使用するために必要

MainToolBar::MainToolBar() {}

void MainToolBar::render(CanvasManager& canvas_manager) {
    // 1. メインメニューバー
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("ファイル")) {
            if (ImGui::MenuItem("新しいキャンバス", "Ctrl+N")) {
                new_canvas_dialog_.open();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // 2. 新規作成モーダルダイアログのレンダリング
    new_canvas_dialog_.render(canvas_manager);

    // 3. キャンバスタブバーの表示
    if (ImGui::BeginViewportSideBar("##CanvasTabBarHost", ImGui::GetMainViewport(), ImGuiDir_Up, ImGui::GetFrameHeight(), ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginTabBar("CanvasTabBar", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {

                // '+' ボタンによる新規作成
                if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
                    new_canvas_dialog_.open();
                }

                // タブの列挙描画と削除(xボタン)ハンドリング
                for (int i = 0; i < static_cast<int>(canvas_manager.documents.size()); ++i) {
                    auto& doc = canvas_manager.documents[i];
                    bool open = true;

                    std::string label = doc->title + " (" + std::to_string(doc->width) + "x" + std::to_string(doc->height) + ")###Tab_" + std::to_string(doc->id);

                    if (ImGui::BeginTabItem(label.c_str(), &open)) {
                        canvas_manager.active_index = i;
                        ImGui::EndTabItem();
                    }

                    // バツボタンでタブを閉じた場合
                    if (!open) {
                        canvas_manager.close_canvas(i);
                        break;
                    }
                }

                ImGui::EndTabBar();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}