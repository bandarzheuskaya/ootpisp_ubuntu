#include "EllipseShape.h"

#include <QPainter>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
 
namespace {
constexpr double EPS = 1e-9;
constexpr double MIN_SEMI_MAJOR = 1.0;
constexpr double MIN_SEMI_MINOR = 12.0;
}

Point EllipseShape::center() const {
    return anchor + centerOffset;
}

Point EllipseShape::focus1Abs() const {
    return center() + focus1Offset;
}

Point EllipseShape::focus2Abs() const {
    return center() + focus2Offset;
}

bool EllipseShape::isVertical() const {
    return std::abs(focus1Offset.x) < std::abs(focus1Offset.y);
}

bool EllipseShape::isCircle() const {
    return focalRadius() < EPS;
}

double EllipseShape::focalRadius() const {
    return std::sqrt(focus1Offset.x * focus1Offset.x + focus1Offset.y * focus1Offset.y);
}

double EllipseShape::semiMinorAxis() const {
    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double c = focalRadius();

    if (c > a) {
        c = a;
    }

    double b = std::sqrt(std::max(0.0, a * a - c * c));
    return std::max(MIN_SEMI_MINOR, b);
}

void EllipseShape::setCenterAbsolute(const Point& c) {
    centerOffset = c - anchor;
}

void EllipseShape::normalizeGeometry() {
    Point mid{
        (focus1Offset.x + focus2Offset.x) * 0.5,
        (focus1Offset.y + focus2Offset.y) * 0.5
    };

    focus1Offset = focus1Offset - mid;
    focus2Offset = focus2Offset - mid;

    Point d{
        (focus2Offset.x - focus1Offset.x) * 0.5,
        (focus2Offset.y - focus1Offset.y) * 0.5
    };

    if (std::abs(d.x) >= std::abs(d.y)) {
        d.y = 0.0;
    } else {
        d.x = 0.0;
    }

    focus1Offset = {-d.x, -d.y};
    focus2Offset = { d.x,  d.y};

    double c = std::sqrt(d.x * d.x + d.y * d.y);

    if (semiMajor < c) {
        semiMajor = c;
    }

    semiMajor = std::max(semiMajor, std::max(MIN_SEMI_MAJOR, std::sqrt(c * c + MIN_SEMI_MINOR * MIN_SEMI_MINOR)));

    ensureStyleArrays();
}

ShapeKind EllipseShape::kind() const {
    return ShapeKind::Ellipse;
}

QString EllipseShape::kindName() const {
    return isCircle() ? "Окружность" : "Эллипс";
}

std::vector<Point> EllipseShape::verticesAbs() const {
    Rectangle b = boundingBox();

    return {
        {b.x, b.y},
        {b.x + b.width, b.y},
        {b.x + b.width, b.y + b.height},
        {b.x, b.y + b.height}
    };
}

std::vector<Point> EllipseShape::verticesRel() const {
    auto abs = verticesAbs();
    std::vector<Point> rel;
    rel.reserve(abs.size());

    for (const auto& p : abs) {
        rel.push_back(p - anchor);
    }

    return rel;
}

void EllipseShape::setVerticesRel(const std::vector<Point>& rel) {
    if (rel.empty()) return;

    double minX = rel[0].x;
    double maxX = rel[0].x;
    double minY = rel[0].y;
    double maxY = rel[0].y;

    for (const auto& p : rel) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }

    double width = std::max(2.0, maxX - minX);
    double height = std::max(2.0, maxY - minY);

    Point newCenterRel{
        (minX + maxX) * 0.5,
        (minY + maxY) * 0.5
    };
    centerOffset = newCenterRel;

    bool vertical = height > width;
    double a = vertical ? height * 0.5 : width * 0.5;
    double b = vertical ? width * 0.5 : height * 0.5;

    b = std::max(MIN_SEMI_MINOR, b);
    a = std::max(a, b);

    setSemiAxes(a, b, vertical);
    ensureStyleArrays();
}

int EllipseShape::sideCount() const {
    return 1;
}

bool EllipseShape::supportsVerticesEditing() const {
    return false;
}

std::unique_ptr<Shape> EllipseShape::clone() const {
    return std::make_unique<EllipseShape>(*this);
}

Rectangle EllipseShape::boundingBox() const {
    Point c = center();

    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double b = semiMinorAxis();

    double halfW = isVertical() ? b : a;
    double halfH = isVertical() ? a : b;

    double stroke = sideWidths.empty() ? 0.0 : sideWidths[0];
    double halfStroke = stroke * 0.5;

    return {
        c.x - halfW - halfStroke,
        c.y - halfH - halfStroke,
        halfW * 2.0 + stroke,
        halfH * 2.0 + stroke
    };
}

std::vector<double> EllipseShape::sideLengths() const {
    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double b = std::max(MIN_SEMI_MINOR, semiMinorAxis());

    double h = std::pow(a - b, 2.0) / std::pow(a + b, 2.0);
    double perimeter = M_PI * (a + b) * (1.0 + (3.0 * h) / (10.0 + std::sqrt(4.0 - 3.0 * h)));

    return {perimeter};
}

bool EllipseShape::setSideLength(int index, double target) {
    if (index != 0 || target <= 1.0) return false;

    double current = sideLengths()[0];
    if (current <= EPS) return false;

    double factor = target / current;
    scaleUniform(factor);
    return true;
}

void EllipseShape::moveBy(const Point& delta) {
    anchor = anchor + delta;
}

void EllipseShape::scaleUniform(double factor) {
    if (factor <= 0.0) return;

    centerOffset = centerOffset * factor;
    focus1Offset = focus1Offset * factor;
    focus2Offset = focus2Offset * factor;
    semiMajor *= factor;
    currentScale *= factor;

    normalizeGeometry();
}

void EllipseShape::resizeToBoundingBox(const Rectangle& target) {
    Point c{
        target.x + target.width * 0.5,
        target.y + target.height * 0.5
    };
    setCenterAbsolute(c);

    bool vertical = target.height > target.width;

    double a = vertical ? target.height * 0.5 : target.width * 0.5;
    double b = vertical ? target.width * 0.5 : target.height * 0.5;

    b = std::max(MIN_SEMI_MINOR, b);
    a = std::max(a, b);

    setSemiAxes(a, b, vertical);
}

void EllipseShape::draw(QPainter& painter) const {
    painter.save();

    painter.setBrush(fillColor);

    QColor borderColor = sideColors.empty() ? QColor(Qt::black) : sideColors[0];
    double borderWidth = sideWidths.empty() ? 3.0 : sideWidths[0];

    QPen pen(borderColor, borderWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);

    Point c = center();

    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double b = semiMinorAxis();

    QRectF rect;
    if (isVertical()) {
        rect = QRectF(c.x - b, c.y - a, 2.0 * b, 2.0 * a);
    } else {
        rect = QRectF(c.x - a, c.y - b, 2.0 * a, 2.0 * b);
    }

    painter.drawEllipse(rect);
    painter.restore();
}

bool EllipseShape::contains(const Point& p) const {
    Point c = center();

    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double b = semiMinorAxis();

    if (a <= EPS || b <= EPS) return false;

    double dx = p.x - c.x;
    double dy = p.y - c.y;

    double nx = isVertical() ? dx / b : dx / a;
    double ny = isVertical() ? dy / a : dy / b;

    return nx * nx + ny * ny <= 1.0;
}

void EllipseShape::setAnchorInside(const Point& newAnchor) {
    Point oldCenter = center();
    anchor = newAnchor;
    centerOffset = oldCenter - anchor;
}

void EllipseShape::setFromAbsoluteFoci(const Point& f1, const Point& f2) {
    Point c{
        (f1.x + f2.x) * 0.5,
        (f1.y + f2.y) * 0.5
    };
    setCenterAbsolute(c);

    focus1Offset = f1 - c;
    focus2Offset = f2 - c;
    normalizeGeometry();
}

void EllipseShape::setSemiMajor(double a) {
    double c = focalRadius();
    double minAllowed = std::sqrt(c * c + MIN_SEMI_MINOR * MIN_SEMI_MINOR);
    semiMajor = std::max(a, std::max(MIN_SEMI_MAJOR, minAllowed));
    normalizeGeometry();
}

void EllipseShape::setSemiAxes(double a, double b, bool vertical) {
    b = std::max(MIN_SEMI_MINOR, b);
    a = std::max(a, b);
    a = std::max(a, MIN_SEMI_MAJOR);

    semiMajor = a;
    double c = std::sqrt(std::max(0.0, a * a - b * b));

    if (vertical) {
        focus1Offset = {0.0, -c};
        focus2Offset = {0.0,  c};
    } else {
        focus1Offset = {-c, 0.0};
        focus2Offset = { c, 0.0};
    }

    normalizeGeometry();
}

void EllipseShape::setVertical(bool vertical) {
    double c = focalRadius();

    if (vertical) {
        focus1Offset = {0.0, -c};
        focus2Offset = {0.0,  c};
    } else {
        focus1Offset = {-c, 0.0};
        focus2Offset = { c, 0.0};
    }

    normalizeGeometry();
}

QJsonObject EllipseShape::save() const {
    QJsonObject obj = saveBaseFields();
    obj["type"] = "Ellipse";
    obj["centerOffsetX"] = centerOffset.x;
    obj["centerOffsetY"] = centerOffset.y;
    obj["focus1OffsetX"] = focus1Offset.x;
    obj["focus1OffsetY"] = focus1Offset.y;
    obj["focus2OffsetX"] = focus2Offset.x;
    obj["focus2OffsetY"] = focus2Offset.y;
    obj["semiMajor"] = semiMajor;
    return obj;
}

void EllipseShape::loadFromJson(const QJsonObject& obj) {
    loadBaseFields(obj);

    centerOffset.x = obj["centerOffsetX"].toDouble(0.0);
    centerOffset.y = obj["centerOffsetY"].toDouble(0.0);

    focus1Offset.x = obj["focus1OffsetX"].toDouble(-30.0);
    focus1Offset.y = obj["focus1OffsetY"].toDouble(0.0);
    focus2Offset.x = obj["focus2OffsetX"].toDouble(30.0);
    focus2Offset.y = obj["focus2OffsetY"].toDouble(0.0);

    semiMajor = obj["semiMajor"].toDouble(90.0);

    normalizeGeometry();
}