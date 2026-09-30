#include "StrokeManager.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

StrokeManager::StrokeManager(const Config& config)
    : config_(config), is_drawing_(false), next_stroke_id_(0) {}

void StrokeManager::update_config(const Config& config) {
    config_ = config;
}

void StrokeManager::on_stroke_begin(float x, float y, float thickness, uint32_t pen_type, const StrokeColor& color) {
    if (x < 0.0f || x > config_.canvas_width || y < 0.0f || y > config_.canvas_height) {
        return;
    }

    current_stroke_ = Stroke();
    current_stroke_.stroke_id = next_stroke_id_++;
    current_stroke_.pen_type = pen_type;
    current_stroke_.color = color;

    Point start_pt(x, y, thickness);
    current_stroke_.raw_points.push_back(start_pt);
    current_stroke_.checkpoint_times_seconds.push_back(0.0f);
    current_stroke_.checkpoint_rates_hz.push_back(60.0f);
    last_recorded_point_ = start_pt.pos;
    last_input_point_ = start_pt.pos;
    last_input_time_ = std::chrono::steady_clock::now();
    stroke_start_time_ = last_input_time_;
    last_checkpoint_time_ = last_input_time_;
    is_drawing_ = true;
    refresh_current_preview();
}

void StrokeManager::on_stroke_move(float x, float y, float thickness) {
    if (!is_drawing_) return;

    const auto now = std::chrono::steady_clock::now();
    Vec2 current_pos(x, y);
    const Vec2 input_diff = current_pos - last_input_point_;
    const float elapsed_seconds = std::chrono::duration<float>(now - last_input_time_).count();
    const float speed = (elapsed_seconds > 0.0f) ? input_diff.length() / elapsed_seconds : 0.0f;
    const float checkpoint_hz = std::clamp(60.0f + speed * 0.25f, 60.0f, 240.0f);
    last_input_point_ = current_pos;
    last_input_time_ = now;

    const float checkpoint_interval = 1.0f / checkpoint_hz;
    if (std::chrono::duration<float>(now - last_checkpoint_time_).count() < checkpoint_interval) return;
    last_checkpoint_time_ = now;

    const Vec2 diff = current_pos - last_recorded_point_;
    if (diff.length_sq() < (config_.min_sample_distance_px * config_.min_sample_distance_px)) return;

    current_stroke_.raw_points.emplace_back(current_pos, thickness);
    current_stroke_.checkpoint_times_seconds.push_back(std::chrono::duration<float>(now - stroke_start_time_).count());
    current_stroke_.checkpoint_rates_hz.push_back(checkpoint_hz);
    last_recorded_point_ = current_pos;
    refresh_current_preview();
}

void StrokeManager::on_stroke_end(float x, float y, float thickness) {
    if (!is_drawing_) return;

    const auto now = std::chrono::steady_clock::now();
    Vec2 final_pos(x, y);
    if ((final_pos - last_recorded_point_).length_sq() > 0.001f) {
        current_stroke_.raw_points.emplace_back(final_pos, thickness);
        current_stroke_.checkpoint_times_seconds.push_back(std::chrono::duration<float>(now - stroke_start_time_).count());
        current_stroke_.checkpoint_rates_hz.push_back(current_stroke_.checkpoint_rates_hz.back());
        // Release位置が最後の時間チェックポイントから動いていた場合だけ、
        // その終点を最後のチェックポイントとしてプレビューへ反映する。
        refresh_current_preview();
    }

    // 新規描画が完了したらRedoスタックをクリア（未来分岐のリセット）
    redo_stack_.clear();

    completed_strokes_.push_back(std::move(current_stroke_));
    is_drawing_ = false;
}

void StrokeManager::refresh_current_preview() {
    process_stroke(current_stroke_.raw_points, current_stroke_.points);
}

// Undo 処理
bool StrokeManager::undo() {
    if (completed_strokes_.empty()) return false;

    // 末尾のストロークをRedoスタックへ高速移動 ($O(1)$)
    redo_stack_.push_back(std::move(completed_strokes_.back()));
    completed_strokes_.pop_back();
    return true;
}

// Redo 処理
bool StrokeManager::redo() {
    if (redo_stack_.empty()) return false;

    // Redoスタックから有効ストロークへ復元 ($O(1)$)
    completed_strokes_.push_back(std::move(redo_stack_.back()));
    redo_stack_.pop_back();
    return true;
}

// 点 P と 線分 AB の垂直距離
float StrokeManager::perpendicular_distance(const Vec2& p, const Vec2& a, const Vec2& b) {
    Vec2 ab = b - a;
    float ab_len_sq = ab.length_sq();
    if (ab_len_sq == 0.0f) return (p - a).length();

    float t = (p - a).dot(ab) / ab_len_sq;
    t = std::max(0.0f, std::min(1.0f, t));
    Vec2 projection = a + ab * t;
    return (p - projection).length();
}

// RDP 再帰間引き処理
void StrokeManager::rdp_recursive(const std::vector<Point>& points, size_t start_idx, size_t end_idx, float epsilon) {
    if (end_idx <= start_idx + 1) return;

    float max_dist = 0.0f;
    size_t max_idx = start_idx;

    for (size_t i = start_idx + 1; i < end_idx; ++i) {
        float dist = perpendicular_distance(points[i].pos, points[start_idx].pos, points[end_idx].pos);
        if (dist > max_dist) {
            max_dist = dist;
            max_idx = i;
        }
    }

    if (max_dist > epsilon) {
        keep_flags_[max_idx] = true;
        rdp_recursive(points, start_idx, max_idx, epsilon);
        rdp_recursive(points, max_idx, end_idx, epsilon);
    }
}

// 曲げ角度計算
float StrokeManager::calculate_turn_angle(const Vec2& p0, const Vec2& p1, const Vec2& p2) {
    Vec2 v1 = p1 - p0;
    Vec2 v2 = p2 - p1;
    float len1 = v1.length();
    float len2 = v2.length();
    if (len1 < 1e-5f || len2 < 1e-5f) return 0.0f;

    float cos_theta = std::max(-1.0f, std::min(1.0f, v1.dot(v2) / (len1 * len2)));
    return std::acos(cos_theta);
}

// スプライン補間（位置：Catmull-Rom / 太さ：線形補間）
Point StrokeManager::catmull_rom(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    Vec2 pos = (p0.pos * (-t3 + 2.0f * t2 - t) +
                p1.pos * (3.0f * t3 - 5.0f * t2 + 2.0f) +
                p2.pos * (-3.0f * t3 + 4.0f * t2 + t) +
                p3.pos * (t3 - t2)) * 0.5f;

    float thickness = p1.thickness + t * (p2.thickness - p1.thickness);

    return Point(pos, std::max(0.1f, thickness));
}

// メイン処理パイプライン
void StrokeManager::process_stroke(const std::vector<Point>& raw, std::vector<Point>& output) {
    if (raw.size() <= 2) {
        output = raw;
        return;
    }

    // STEP 1: 直線区間の間引き (RDP)
    keep_flags_.assign(raw.size(), false);
    keep_flags_.front() = true;
    keep_flags_.back() = true;
    rdp_recursive(raw, 0, raw.size() - 1, config_.rdp_epsilon_px);

    simplified_.clear();
    simplified_.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (keep_flags_[i]) simplified_.push_back(raw[i]);
    }

    if (simplified_.size() <= 2) {
        output.clear();
        const Point& p1 = simplified_.front();
        const Point& p2 = simplified_.back();
        float dist = (p2.pos - p1.pos).length();

        float target_spacing = (config_.target_point_spacing_px > 0.0f) 
                             ? config_.target_point_spacing_px 
                             : config_.min_sample_distance_px;
        target_spacing = std::max(1.0f, target_spacing);

        int subdivisions = std::max(1, static_cast<int>(std::ceil(dist / target_spacing)));
        output.reserve(subdivisions + 1);

        for (int sub = 0; sub < subdivisions; ++sub) {
            float t = static_cast<float>(sub) / static_cast<float>(subdivisions);
            Vec2 pos = p1.pos + (p2.pos - p1.pos) * t;
            float thickness = p1.thickness + t * (p2.thickness - p1.thickness);
            output.emplace_back(pos, std::max(0.1f, thickness));
        }
        output.push_back(p2);
        return;
    }

    // STEP 2: 距離 + 角度による動的補間
    output.clear();
    output.reserve(simplified_.size() * 4);

    float target_spacing = (config_.target_point_spacing_px > 0.0f) 
                         ? config_.target_point_spacing_px 
                         : config_.min_sample_distance_px;
    target_spacing = std::max(1.0f, target_spacing);

    size_t n = simplified_.size();
    for (size_t i = 0; i < n - 1; ++i) {
        const Point& p0 = simplified_[i == 0 ? 0 : i - 1];
        const Point& p1 = simplified_[i];
        const Point& p2 = simplified_[i + 1];
        const Point& p3 = simplified_[i + 2 >= n ? n - 1 : i + 2];

        float dist = (p2.pos - p1.pos).length();
        int base_subdivisions = std::max(1, static_cast<int>(std::ceil(dist / target_spacing)));

        float angle1 = calculate_turn_angle(p0.pos, p1.pos, p2.pos);
        float angle2 = calculate_turn_angle(p1.pos, p2.pos, p3.pos);
        float max_angle = std::max(angle1, angle2);

        float curve_multiplier = 1.0f;
        if (max_angle > 0.05f) {
            float ratio = std::min(1.0f, max_angle / config_.max_curve_angle_rad);
            curve_multiplier = 1.0f + (ratio * 2.0f);
        }

        int subdivisions = std::max(1, static_cast<int>(std::round(base_subdivisions * curve_multiplier)));

        output.push_back(p1);

        for (int sub = 1; sub < subdivisions; ++sub) {
            float t = static_cast<float>(sub) / static_cast<float>(subdivisions);
            output.push_back(catmull_rom(p0, p1, p2, p3, t));
        }
    }
    output.push_back(simplified_.back());
}
