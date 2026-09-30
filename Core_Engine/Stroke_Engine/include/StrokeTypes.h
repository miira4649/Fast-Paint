#ifndef STROKE_TYPES_H
#define STROKE_TYPES_H

#include <vector>
#include <cmath>
#include <cstdint>

// 2Dベクトル構造体
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator-(const Vec2& rhs) const { return Vec2(x - rhs.x, y - rhs.y); }
    Vec2 operator+(const Vec2& rhs) const { return Vec2(x + rhs.x, y + rhs.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    
    float length_sq() const { return x * x + y * y; }
    float length() const { return std::sqrt(length_sq()); }
    float dot(const Vec2& rhs) const { return x * rhs.x + y * rhs.y; }
};

// 点構造体：位置と個別の太さ（筆圧）
struct Point {
    Vec2 pos;
    float thickness = 1.0f;

    Point() = default;
    Point(Vec2 p, float t) : pos(p), thickness(t) {}
    Point(float x, float y, float t) : pos(Vec2(x, y)), thickness(t) {}
};

struct StrokeColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

enum class StrokeTool : uint32_t {
    Brush = 0,
    Eraser = 1
};

// 1ストロークのデータ構造
struct Stroke {
    uint32_t stroke_id = 0;
    uint32_t pen_type = 0;          // ペンの種類（0:鉛筆, 1:マーカー, 2:筆 など）
    StrokeColor color;
    std::vector<Point> raw_points;  // 入力された生の点列
    std::vector<float> checkpoint_times_seconds; // raw_points と同じ添字の時間チェックポイント
    std::vector<float> checkpoint_rates_hz;       // 各チェックポイント時点の適応サンプリング周波数
    std::vector<Point> points;      // 間引き・補間完了後の最終点列
};

// Vulkan SSBO (std430) 転送用アライメント構造体（16バイト境界）
struct alignas(16) VulkanPoint {
    float x;
    float y;
    float thickness;
    float padding = 0.0f;
};

#endif // STROKE_TYPES_H
