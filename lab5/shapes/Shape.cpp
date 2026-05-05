#include "Shape.h"
#include "PolygonShape.h"
#include "EllipseShape.h"
#include "GroupShape.h"

std::unique_ptr<Shape> Shape::load(const QJsonObject& obj) {
    const QString type = obj["type"].toString();

    if (type == "Polygon") {
        auto shape = std::make_unique<PolygonShape>();
        shape->loadFromJson(obj);
        return shape;
    }

    if (type == "Ellipse") {
        auto shape = std::make_unique<EllipseShape>();
        shape->loadFromJson(obj);
        return shape;
    }

    if (type == "Group") {
        auto shape = std::make_unique<GroupShape>();
        shape->loadFromJson(obj);
        return shape;
    }

    return nullptr;
}