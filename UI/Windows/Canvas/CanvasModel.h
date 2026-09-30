#pragma once
#include <string>
#include <memory>
#include "StrokeManager.h"

struct CanvasDocument {
    int id;
    std::string title;
    int width;
    int height;
    std::unique_ptr<StrokeManager> stroke_manager;

    CanvasDocument(int doc_id, const std::string& doc_title, int w, int h)
        : id(doc_id), title(doc_title), width(w), height(h) {
        
        StrokeManager::Config config;
        config.canvas_width = static_cast<float>(w);
        config.canvas_height = static_cast<float>(h);
        config.min_sample_distance_px = 1.0f;
        config.rdp_epsilon_px = 0.5f;
        config.target_point_spacing_px = 6.0f;

        stroke_manager = std::make_unique<StrokeManager>(config);
    }
};