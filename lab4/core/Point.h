#pragma once

struct Point {
    double x = 0.0;
    double y = 0.0;

    Point() = default;
    Point(double xx, double yy) : x(xx), y(yy) {}

    Point operator+(const Point& other) const { return {x + other.x, y + other.y}; }
    Point operator-(const Point& other) const { return {x - other.x, y - other.y}; }
    Point operator*(double k) const { return {x * k, y * k}; }
};

struct PointF2 {
    double x = 0.0;
    double y = 0.0;

    PointF2() = default;
    PointF2(double xx, double yy) : x(xx), y(yy) {}
};

struct Vector2D {
    double X = 0.0;
    double Y = 0.0;

    Vector2D() = default;
    Vector2D(double xx, double yy) : X(xx), Y(yy) {}
};

struct Rectangle {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};