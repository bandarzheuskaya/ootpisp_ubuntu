#pragma once

#include "Shape.h"
#include <QString>

class PolygonShape : public Shape {
public:
    PolygonShape(const QString& name = "Многоугольник");

    QString displayName;
    std::vector<Point> relVerts;
    bool angleEditingEnabled = true;
    bool trapezoidMode = false;
    bool isIsoscelesTrapezoid = false;

    ShapeKind kind() const override;
    QString kindName() const override;

    std::vector<Point> verticesAbs() const override;
    std::vector<Point> verticesRel() const override;
    void setVerticesRel(const std::vector<Point>& rel) override;

    void draw(QPainter& painter) const override;
    bool contains(const Point& p) const override;
    std::unique_ptr<Shape> clone() const override;
    int sideCount() const override;

    std::vector<double> sideLengths() const override;
    bool setSideLength(int index, double targetLen) override;

    bool supportsAngleEditing() const override;
    std::vector<double> interiorAngles() const override;
    bool setAngleAt(int index, double targetDeg) override;

    Rectangle boundingBox() const override;

    QJsonObject save() const override;
    void loadFromJson(const QJsonObject& obj);
};