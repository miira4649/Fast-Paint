#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "CanvasModel.h"

class CanvasManager {
public:
    std::vector<std::unique_ptr<CanvasDocument>> documents;
    int active_index = -1;
    int next_document_id = 1;

    void create_new_canvas(int width, int height) {
        std::string title = "Canvas " + std::to_string(next_document_id);
        documents.push_back(std::make_unique<CanvasDocument>(next_document_id, title, width, height));
        active_index = static_cast<int>(documents.size()) - 1;
        next_document_id++;
    }

    void close_canvas(int index) {
        if (index < 0 || index >= static_cast<int>(documents.size())) return;
        documents.erase(documents.begin() + index);

        if (documents.empty()) {
            active_index = -1;
        } else if (active_index > index) {
            --active_index;
        } else if (active_index == index) {
            active_index = std::min(index, static_cast<int>(documents.size()) - 1);
        }
    }

    CanvasDocument* get_active_canvas() {
        if (active_index >= 0 && active_index < static_cast<int>(documents.size())) {
            return documents[active_index].get();
        }
        return nullptr;
    }
};
