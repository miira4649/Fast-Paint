#ifndef STROKE_MANAGER_H
#define STROKE_MANAGER_H

#include "StrokeTypes.h"
#include <vector>
#include <cstdint>
#include <chrono>

class StrokeManager {
public:
    struct Config {
        float canvas_width = 1000.0f;
        float canvas_height = 2000.0f;
        float min_sample_distance_px = 1.0f;     // 入力時の最小サンプリング距離
        float rdp_epsilon_px = 0.5f;              // RDP間引きの許容誤差
        float max_curve_angle_rad = 0.785398163f; // 最大密度(3倍)に達する曲げ角度 (π/4)
        float target_point_spacing_px = 6.0f;     // 補間後の点の間隔（ピクセル）
    };

    explicit StrokeManager(const Config& config = Config());

    void update_config(const Config& config);

    // イベント入力API
    void on_stroke_begin(float x, float y, float thickness, uint32_t pen_type = 0, const StrokeColor& color = StrokeColor{});
    void on_stroke_move(float x, float y, float thickness);
    void on_stroke_end(float x, float y, float thickness);

    // Undo / Redo API
    bool undo();
    bool redo();
    bool can_undo() const { return !completed_strokes_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    // データ取得 API
    const std::vector<Stroke>& get_strokes() const { return completed_strokes_; }
    const Stroke* get_current_stroke() const { return is_drawing_ ? &current_stroke_ : nullptr; }
    void clear_strokes() { 
        completed_strokes_.clear(); 
        redo_stack_.clear();
    }

private:
    Config config_;
    Stroke current_stroke_;
    Vec2 last_recorded_point_;
    Vec2 last_input_point_;
    std::chrono::steady_clock::time_point stroke_start_time_;
    std::chrono::steady_clock::time_point last_input_time_;
    std::chrono::steady_clock::time_point last_checkpoint_time_;
    bool is_drawing_ = false;
    uint32_t next_stroke_id_ = 0;

    std::vector<Stroke> completed_strokes_; // 有効な確定済みストローク
    std::vector<Stroke> redo_stack_;        // Undoされたストロークのスタック

    // メモリ再確保によるパフォーマンス低下を防ぐための再利用ワークスペースバッファ
    std::vector<bool> keep_flags_;
    std::vector<Point> simplified_;

    // 内部パイプライン処理
    void refresh_current_preview();
    void process_stroke(const std::vector<Point>& raw, std::vector<Point>& output);
    void rdp_recursive(const std::vector<Point>& points, size_t start_idx, size_t end_idx, float epsilon);

    static float perpendicular_distance(const Vec2& p, const Vec2& a, const Vec2& b);
    static float calculate_turn_angle(const Vec2& p0, const Vec2& p1, const Vec2& p2);
    static Point catmull_rom(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float t);
};

#endif // STROKE_MANAGER_H
