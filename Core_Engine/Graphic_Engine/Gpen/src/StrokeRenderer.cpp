#include "../include/StrokeRenderer.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// セグメント不変量キャッシュ構造体
// ============================================================================
struct SegmentCache {
    Vec2 a;
    Vec2 ba;
    float b_len;
    float inv_b_len;  // 除算回避のための逆数
    float r_a;
    float r_b;
    float sin_t;
    float cos_t;
    float side_len;
    bool is_point;
    bool is_contained;
    int min_x, max_x;
    int min_y, max_y;
};

// ============================================================================
// 1. Vulkan SSBO用データのバッファ変換（resizeによるゼロオーバーヘッド化）
// ============================================================================
void StrokeRenderer::build_vulkan_buffer(
    const std::vector<Stroke>& strokes, 
    std::vector<VulkanPoint>& out_vulkan_points) 
{
    size_t total_points = 0;
    for (const auto& stroke : strokes) {
        total_points += stroke.points.size();
    }
    out_vulkan_points.resize(total_points);

    size_t idx = 0;
    for (const auto& stroke : strokes) {
        for (const auto& pt : stroke.points) {
            out_vulkan_points[idx++] = {
                pt.pos.x,
                pt.pos.y,
                pt.thickness,
                0.0f
            };
        }
    }
}

// ============================================================================
// セグメント不変量の事前計算（ループ外移動）
// ============================================================================
static inline SegmentCache compute_segment_cache(
    const Vec2& a, const Vec2& b, 
    float r_a, float r_b, 
    uint32_t width, uint32_t height, 
    float aa_width) 
{
    SegmentCache cache{};
    cache.a = a;
    cache.r_a = r_a;
    cache.r_b = r_b;
    cache.ba = b - a;
    
    float h = cache.ba.dot(cache.ba);
    
    if (h < 1e-6f) {
        cache.is_point = true;
        cache.b_len = 0.0f;
        cache.inv_b_len = 0.0f;
        cache.sin_t = 0.0f;
        cache.cos_t = 1.0f;
        cache.side_len = 0.0f;
        cache.is_contained = false;
    } else {
        cache.is_point = false;
        cache.b_len = std::sqrt(h);
        cache.inv_b_len = 1.0f / cache.b_len;
        
        float r_diff = r_a - r_b;
        
        if (std::abs(r_diff) >= cache.b_len) {
            cache.is_contained = true;
            cache.sin_t = 0.0f;
            cache.cos_t = 1.0f;
            cache.side_len = 0.0f;
        } else {
            cache.is_contained = false;
            cache.sin_t = r_diff * cache.inv_b_len;
            cache.cos_t = std::sqrt(std::max(0.0f, 1.0f - cache.sin_t * cache.sin_t));
            cache.side_len = cache.b_len * cache.cos_t;
        }
    }
    
    // AABBの算出（画面外クランプ含む）
    float min_rad_x = std::min(a.x - r_a, b.x - r_b);
    float max_rad_x = std::max(a.x + r_a, b.x + r_b);
    float min_rad_y = std::min(a.y - r_a, b.y - r_b);
    float max_rad_y = std::max(a.y + r_a, b.y + r_b);
    
    cache.min_x = std::max(0, static_cast<int>(std::floor(min_rad_x - aa_width)));
    cache.max_x = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(max_rad_x + aa_width)));
    cache.min_y = std::max(0, static_cast<int>(std::floor(min_rad_y - aa_width)));
    cache.max_y = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(max_rad_y + aa_width)));
    
    return cache;
}

// ============================================================================
// 高速SDF評価（完全に乗算のみで処理）
// ============================================================================
static inline float evaluate_sdf_cached(const Vec2& p_in, const SegmentCache& c) {
    Vec2 p = p_in - c.a;
    
    if (c.is_point) {
        return p.length() - c.r_a;
    }
    
    if (c.is_contained) {
        float d1 = p.length() - c.r_a;
        Vec2 pb = p_in - (c.a + c.ba);
        float d2 = pb.length() - c.r_b;
        return std::min(d1, d2);
    }
    
    // 事前計算済みの inv_b_len を使って除算を完全排除
    float px = std::abs(p.x * c.ba.y - p.y * c.ba.x) * c.inv_b_len;
    float py = p.dot(c.ba) * c.inv_b_len;
    
    float s = -px * c.sin_t + py * c.cos_t;
    
    if (s <= 0.0f) {
        return p.length() - c.r_a;
    }
    
    if (s >= c.side_len) {
        Vec2 pb = p_in - (c.a + c.ba);
        return pb.length() - c.r_b;
    }
    
    return px * c.cos_t + py * c.sin_t - c.r_a;
}

// ============================================================================
// 2. ピクセルバッファへの SDF + AABB 描画（CPU超高速化版）
// ============================================================================
void StrokeRenderer::render_to_pixel_buffer(
    const std::vector<Stroke>& strokes,
    uint32_t* pixel_buffer,
    uint32_t width,
    uint32_t height,
    uint32_t background_color,
    uint32_t dot_color) 
{
    if (!pixel_buffer || width == 0 || height == 0) return;

    // 背景クリア
    const size_t total_pixels = static_cast<size_t>(width) * height;
    std::fill_n(pixel_buffer, total_pixels, background_color);

    // 描画色の分解
    const float src_a = static_cast<float>((dot_color >> 24) & 0xFF);
    const float src_r = static_cast<float>((dot_color >> 16) & 0xFF);
    const float src_g = static_cast<float>((dot_color >> 8) & 0xFF);
    const float src_b = static_cast<float>(dot_color & 0xFF);
    const bool is_opaque = (src_a >= 254.5f);

    const float aa_width = 1.0f;
    const float half_aa = aa_width * 0.5f;

    for (const auto& stroke : strokes) {
        const auto& pts = stroke.points;
        if (pts.empty()) continue;

        // 【ケースA】単一の点（タップ）
        if (pts.size() == 1) {
            Vec2 p = pts[0].pos;
            float r = pts[0].thickness * 0.5f;

            int min_x = std::max(0, static_cast<int>(std::floor(p.x - r - aa_width)));
            int max_x = std::min(static_cast<int>(width) - 1, static_cast<int>(std::ceil(p.x + r + aa_width)));
            int min_y = std::max(0, static_cast<int>(std::floor(p.y - r - aa_width)));
            int max_y = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(p.y + r + aa_width)));

            for (int y = min_y; y <= max_y; ++y) {
                uint32_t* row = &pixel_buffer[static_cast<size_t>(y) * width];
                float py_val = static_cast<float>(y) + 0.5f;

                for (int x = min_x; x <= max_x; ++x) {
                    Vec2 pixel_pos(static_cast<float>(x) + 0.5f, py_val);
                    float d_sdf = (pixel_pos - p).length() - r;

                    // 境界外は即座にスキップ
                    if (d_sdf >= half_aa) continue;

                    // 完全内側かつ不透明なら背景を読まずに上書き（超高速パス）
                    if (d_sdf <= -half_aa && is_opaque) {
                        row[x] = dot_color;
                        continue;
                    }

                    // アンチエイリアス境界ブレンド
                    float coverage = std::min(1.0f, 0.5f - (d_sdf / aa_width));
                    float eff_alpha = (src_a / 255.0f) * coverage;

                    uint32_t bg = row[x];
                    float bg_r = static_cast<float>((bg >> 16) & 0xFF);
                    float bg_g = static_cast<float>((bg >> 8) & 0xFF);
                    float bg_b = static_cast<float>(bg & 0xFF);

                    uint32_t out_r = static_cast<uint32_t>(src_r * eff_alpha + bg_r * (1.0f - eff_alpha));
                    uint32_t out_g = static_cast<uint32_t>(src_g * eff_alpha + bg_g * (1.0f - eff_alpha));
                    uint32_t out_b = static_cast<uint32_t>(src_b * eff_alpha + bg_b * (1.0f - eff_alpha));

                    row[x] = (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
            continue;
        }

        // 【ケースB】2点以上（セグメント描画）
        for (size_t i = 0; i < pts.size() - 1; ++i) {
            SegmentCache cache = compute_segment_cache(
                pts[i].pos, pts[i + 1].pos,
                pts[i].thickness * 0.5f, pts[i + 1].thickness * 0.5f,
                width, height, aa_width
            );

            // 画面外セグメントの早期カリング
            if (cache.min_x > cache.max_x || cache.min_y > cache.max_y) continue;

            for (int y = cache.min_y; y <= cache.max_y; ++y) {
                // 行先頭ポインタのキャッシュ（インデックス乗算の排除）
                uint32_t* row = &pixel_buffer[static_cast<size_t>(y) * width];
                float py_val = static_cast<float>(y) + 0.5f;

                for (int x = cache.min_x; x <= cache.max_x; ++x) {
                    Vec2 pixel_pos(static_cast<float>(x) + 0.5f, py_val);
                    float d_sdf = evaluate_sdf_cached(pixel_pos, cache);

                    // 1. 完全外側：即スキップ
                    if (d_sdf >= half_aa) continue;

                    // 2. 完全内側かつ不透明：背景色読み出しと計算を丸ごとスキップ（超高速パス）
                    if (d_sdf <= -half_aa && is_opaque) {
                        row[x] = dot_color;
                        continue;
                    }

                    // 3. 境界（アンチエイリアス区間）または半透明色のみブレンド計算
                    float coverage = std::min(1.0f, 0.5f - (d_sdf / aa_width));
                    float eff_alpha = (src_a / 255.0f) * coverage;

                    uint32_t bg = row[x];
                    float bg_r = static_cast<float>((bg >> 16) & 0xFF);
                    float bg_g = static_cast<float>((bg >> 8) & 0xFF);
                    float bg_b = static_cast<float>(bg & 0xFF);

                    uint32_t out_r = static_cast<uint32_t>(src_r * eff_alpha + bg_r * (1.0f - eff_alpha));
                    uint32_t out_g = static_cast<uint32_t>(src_g * eff_alpha + bg_g * (1.0f - eff_alpha));
                    uint32_t out_b = static_cast<uint32_t>(src_b * eff_alpha + bg_b * (1.0f - eff_alpha));

                    row[x] = (0xFF << 24) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }
}