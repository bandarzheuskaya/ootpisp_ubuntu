#pragma once

#include "Shape.h"

class EllipseShape : public Shape {
public:
    
    Point focus1Offset{-30.0, 0.0};
    Point focus2Offset{ 30.0, 0.0};
    Point centerOffset{0.0, 0.0};
    double rotationDeg = 0.0;
    double semiMajor = 90.0;
    
    EllipseShape() = default;

    ShapeKind kind() const override;
    QString kindName() const override;

    std::vector<Point> verticesAbs() const override;
    std::vector<Point> verticesRel() const override;
    void setVerticesRel(const std::vector<Point>& rel) override;

    int sideCount() const override;
    bool supportsVerticesEditing() const override;

    std::unique_ptr<Shape> clone() const override;

    Rectangle boundingBox() const override;

    std::vector<double> sideLengths() const override;
    bool setSideLength(int index, double target) override;

    void moveBy(const Point& delta) override;
    void scaleUniform(double factor) override;
    void resizeToBoundingBox(const Rectangle& target) override;
    void draw(QPainter& painter) const override;
    bool contains(const Point& p) const override;
    void setAnchorInside(const Point& newAnchor) override;

    QJsonObject save() const override;
    void loadFromJson(const QJsonObject& obj);

    Point center() const;
    Point focus1Abs() const;
    Point focus2Abs() const;

    bool isVertical() const;
    bool isCircle() const;

    double rotationRadians() const;
    void setRotationDeg(double degrees);

    double focalRadius() const;     // c
    double semiMinorAxis() const;   // b
void setCenterAbsolute(const Point& c);
    void setFromAbsoluteFoci(const Point& f1, const Point& f2);
    void setSemiMajor(double a);
    void setSemiAxes(double a, double b, bool vertical);
    void setVertical(bool vertical);

private:
    void normalizeGeometry();
};