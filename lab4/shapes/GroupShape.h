#pragma once

#include <memory>
#include <vector>

#include <QJsonObject>
#include <QPainter>
#include <QString>

#include "Shape.h"

class GroupShape : public Shape {
public:
    std::vector<std::unique_ptr<Shape>> children;

    GroupShape() = default;

    ShapeKind kind() const override;
    QString kindName() const override;

    bool isGroup() const override;
    std::vector<Shape*> groupedShapes() override;

    std::vector<Point> verticesAbs() const override;
    std::vector<Point> verticesRel() const override;
    void setVerticesRel(const std::vector<Point>& rel) override;

    void draw(QPainter& painter) const override;
    bool contains(const Point& p) const override;
    std::unique_ptr<Shape> clone() const override;
    int sideCount() const override;

    bool supportsVerticesEditing() const override;

    Rectangle boundingBox() const override;

    void moveBy(const Point& delta) override;
    void scaleUniform(double factor) override;
    void resizeToBoundingBox(const Rectangle& target) override;
    void setAnchorInside(const Point& newAnchor) override;

    QJsonObject save() const override;
    void loadFromJson(const QJsonObject& obj);

    void rebuildAnchorToCenter();
};