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
    double angle = std::fmod(rotationDeg, 180.0);
    if (angle < 0.0) angle += 180.0;

    return angle > 45.0 && angle < 135.0;
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
    double a = std::max(MIN_SEMI_MAJOR, semiMajor);
    double c = focalRadius();

    if (c > a) {
        c = a;
    }

    double minAllowed = std::sqrt(c * c + MIN_SEMI_MINOR * MIN_SEMI_MINOR);
    semiMajor = std::max(a, minAllowed);

    double rad = rotationRadians();
    double cosT = std::cos(rad);
    double sinT = std::sin(rad);

    focus1Offset = {-c * cosT, -c * sinT};
    focus2Offset = { c * cosT,  c * sinT};

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

void EllipseShape::setSemiMajor(double a) {
    double c = focalRadius();

    double minAllowed = std::sqrt(c * c + MIN_SEMI_MINOR * MIN_SEMI_MINOR);
    semiMajor = std::max(a, std::max(MIN_SEMI_MAJOR, minAllowed));

    normalizeGeometry();
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

    double rad = rotationRadians();
    double cosT = std::cos(rad);
    double sinT = std::sin(rad);

    double halfW = std::sqrt(a * a * cosT * cosT + b * b * sinT * sinT);
    double halfH = std::sqrt(a * a * sinT * sinT + b * b * cosT * cosT);

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

double EllipseShape::rotationRadians() const {
    return rotationDeg * M_PI / 180.0;
}

void EllipseShape::setRotationDeg(double degrees) {
    rotationDeg = degrees;

    while (rotationDeg < 0.0) {
        rotationDeg += 360.0;
    }

    while (rotationDeg >= 360.0) {
        rotationDeg -= 360.0;
    }

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

    painter.translate(c.x, c.y);
    painter.rotate(rotationDeg);

    QRectF rect(-a, -b, 2.0 * a, 2.0 * b);
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

    double rad = -rotationRadians();
    double cosT = std::cos(rad);
    double sinT = std::sin(rad);

    double localX = dx * cosT - dy * sinT;
    double localY = dx * sinT + dy * cosT;

    return (localX * localX) / (a * a) + (localY * localY) / (b * b) <= 1.0;
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

    Point offset = f2 - c;

    double newC = std::sqrt(offset.x * offset.x + offset.y * offset.y);
    if (newC < EPS) {
        focus1Offset = {0.0, 0.0};
        focus2Offset = {0.0, 0.0};
        return;
    }

    rotationDeg = std::atan2(offset.y, offset.x) * 180.0 / M_PI;

    if (rotationDeg < 0.0) {
        rotationDeg += 360.0;
    }

    if (semiMajor < newC) {
        semiMajor = newC;
    }

    focus1Offset = {-newC * std::cos(rotationRadians()), -newC * std::sin(rotationRadians())};
    focus2Offset = { newC * std::cos(rotationRadians()),  newC * std::sin(rotationRadians())};

    normalizeGeometry();
}
void EllipseShape::setSemiAxes(double a, double b, bool vertical) {
    b = std::max(MIN_SEMI_MINOR, b);
    a = std::max(a, b);
    a = std::max(a, MIN_SEMI_MAJOR);

    semiMajor = a;

    double c = std::sqrt(std::max(0.0, a * a - b * b));

    if (vertical) {
        rotationDeg = 90.0;
    }

    double rad = rotationRadians();
    double cosT = std::cos(rad);
    double sinT = std::sin(rad);

    focus1Offset = {-c * cosT, -c * sinT};
    focus2Offset = { c * cosT,  c * sinT};

    ensureStyleArrays();
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
    obj["rotationDeg"] = rotationDeg;
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

    rotationDeg = obj["rotationDeg"].toDouble(0.0);
    semiMajor = obj["semiMajor"].toDouble(90.0);

    normalizeGeometry();
}