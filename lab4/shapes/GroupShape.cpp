#include "GroupShape.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

ShapeKind GroupShape::kind() const {
    return ShapeKind::Group;
}

QString GroupShape::kindName() const {
    return "Составная фигура";
}

bool GroupShape::isGroup() const {
    return true;
}

std::vector<Shape*> GroupShape::groupedShapes() {
    std::vector<Shape*> out;
    out.reserve(children.size());

    for (auto& child : children) {
        out.push_back(child.get());
    }

    return out;
}

std::vector<Point> GroupShape::verticesAbs() const {
    Rectangle b = boundingBox();
    return {
        {b.x, b.y},
        {b.x + b.width, b.y},
        {b.x + b.width, b.y + b.height},
        {b.x, b.y + b.height}
    };
}

std::vector<Point> GroupShape::verticesRel() const {
    return {};
}

void GroupShape::setVerticesRel(const std::vector<Point>&) {
}

void GroupShape::draw(QPainter& painter) const {
    for (const auto& child : children) {
        if (child) {
            child->draw(painter);
        }
    }
}

bool GroupShape::contains(const Point& p) const {
    for (const auto& child : children) {
        if (child && child->contains(p)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<Shape> GroupShape::clone() const {
    auto g = std::make_unique<GroupShape>();
    g->id = id;
    g->anchor = anchor;
    g->fillColor = fillColor;
    g->sideColors = sideColors;
    g->sideWidths = sideWidths;
    g->currentScale = currentScale;

    for (const auto& child : children) {
        if (child) {
            g->children.push_back(child->clone());
        }
    }

    return g;
}

int GroupShape::sideCount() const {
    return 0;
}

bool GroupShape::supportsVerticesEditing() const {
    return false;
}

Rectangle GroupShape::boundingBox() const {
    if (children.empty()) {
        return {anchor.x, anchor.y, 0.0, 0.0};
    }

    Rectangle first = children[0]->boundingBox();

    double minX = first.x;
    double minY = first.y;
    double maxX = first.x + first.width;
    double maxY = first.y + first.height;

    for (size_t i = 1; i < children.size(); ++i) {
        Rectangle b = children[i]->boundingBox();
        minX = std::min(minX, b.x);
        minY = std::min(minY, b.y);
        maxX = std::max(maxX, b.x + b.width);
        maxY = std::max(maxY, b.y + b.height);
    }

    return {minX, minY, maxX - minX, maxY - minY};
}

void GroupShape::moveBy(const Point& delta) {
    anchor = anchor + delta;

    for (auto& child : children) {
        if (child) {
            child->moveBy(delta);
        }
    }
}

void GroupShape::scaleUniform(double factor) {
    if (factor <= 0.0 || children.empty()) return;

    for (auto& child : children) {
        Point oldOffset = child->anchor - anchor;
        Point newChildAnchor = anchor + oldOffset * factor;

        child->scaleUniform(factor);
        child->moveBy(newChildAnchor - child->anchor);
    }

    currentScale *= factor;
}

void GroupShape::resizeToBoundingBox(const Rectangle& target) {
    if (children.empty()) return;

    Rectangle old = boundingBox();
    if (old.width <= 1e-9 || old.height <= 1e-9) return;

    const double sx = target.width / old.width;
    const double sy = target.height / old.height;

    for (auto& child : children) {
        Rectangle cb = child->boundingBox();

        Rectangle newChildBox;
        newChildBox.x = target.x + (cb.x - old.x) / old.width * target.width;
        newChildBox.y = target.y + (cb.y - old.y) / old.height * target.height;
        newChildBox.width = cb.width * sx;
        newChildBox.height = cb.height * sy;

        child->resizeToBoundingBox(newChildBox);
    }

    anchor = {
        target.x + (anchor.x - old.x) / old.width * target.width,
        target.y + (anchor.y - old.y) / old.height * target.height
    };

    currentScale *= std::sqrt(std::abs(sx * sy));
}

void GroupShape::setAnchorInside(const Point& newAnchor) {
    anchor = newAnchor;
}

QJsonObject GroupShape::save() const {
    QJsonObject obj = saveBaseFields();
    obj["type"] = "Group";

    QJsonArray arr;
    for (const auto& child : children) {
        if (child) {
            arr.append(child->save());
        }
    }
    obj["children"] = arr;

    return obj;
}

void GroupShape::loadFromJson(const QJsonObject& obj) {
    loadBaseFields(obj);
    children.clear();

    QJsonArray arr = obj["children"].toArray();
    for (const auto& v : arr) {
        auto child = Shape::load(v.toObject());
        if (child) {
            children.push_back(std::move(child));
        }
    }

    rebuildAnchorToCenter();
}

void GroupShape::rebuildAnchorToCenter() {
    Rectangle b = boundingBox();
    anchor = {b.x + b.width / 2.0, b.y + b.height / 2.0};
}