#pragma once

#include "../../Stroke_Engine/include/StrokeTypes.h"
#include <vector>
#include <cstdint>

class StrokeRenderer {
public:
    // static関数のみのユーティリティクラスのため、インスタンス化を禁止
    StrokeRenderer() = delete;

    // 1. Vulkan SSBO 転送用データのバッファ構築
    static void build_vulkan_buffer(
        const std::vector<Stroke>& strokes, 
        std::vector<VulkanPoint>& out_vulkan_points
    );

    // 2. ピクセルバッファへのSDF高品質描画 (セグメント不変量キャッシュ + IQ式カプセルSDF + 高速AA)
    static void render_to_pixel_buffer(
        const std::vector<Stroke>& strokes,
        uint32_t* pixel_buffer,
        uint32_t width,
        uint32_t height,
        uint32_t background_color = 0xFFFFFFFF, // デフォルト: 白 (ARGB/RGBA)
        uint32_t dot_color = 0xFF000000          // デフォルト: 黒
    );
};