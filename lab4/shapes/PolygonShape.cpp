#include "PolygonShape.h"
#include "GeometryUtils.h"

#include <algorithm>
#include <cmath>

PolygonShape::PolygonShape(const QString& name)
    : displayName(name) {
}

ShapeKind PolygonShape::kind() const {
    return ShapeKind::Polygon;
}

QString PolygonShape::kindName() const {
    return displayName;
}

std::vector<Point> PolygonShape::verticesAbs() const {
    std::vector<Point> out;
    out.reserve(relVerts.size());
    for (const auto& p : relVerts) out.push_back(anchor + p);
    return out;
}

std::vector<Point> PolygonShape::verticesRel() const {
    return relVerts;
}

void PolygonShape::setVerticesRel(const std::vector<Point>& rel) {
    if (rel.size() >= 3) {
        relVerts = rel;
        ensureStyleArrays();
    }
}

void PolygonShape::draw(QPainter& painter) const {
    drawPolygonWithSharpBorder(painter, verticesAbs());
}

bool PolygonShape::contains(const Point& p) const {
    return pointInPolygon(p, verticesAbs());
}

std::unique_ptr<Shape> PolygonShape::clone() const {
    auto s = std::make_unique<PolygonShape>(displayName);
    s->id = id;
    s->anchor = anchor;
    s->fillColor = fillColor;
    s->sideColors = sideColors;
    s->sideWidths = sideWidths;
    s->currentScale = currentScale;
    s->relVerts = relVerts;
    s->angleEditingEnabled = angleEditingEnabled;
    s->trapezoidMode = trapezoidMode;
    s->isIsoscelesTrapezoid = isIsoscelesTrapezoid;
    return s;
}

int PolygonShape::sideCount() const {
    return (int)relVerts.size();
}

std::vector<double> PolygonShape::sideLengths() const {
    auto abs = verticesAbs();
    std::vector<double> out;
    out.reserve(abs.size());

    for (int i = 0; i < (int)abs.size(); ++i) {
        out.push_back(distancePts(abs[i], abs[(i + 1) % abs.size()]));
    }

    return out;
}

bool PolygonShape::supportsAngleEditing() const {
    return angleEditingEnabled;
}

std::vector<double> PolygonShape::interiorAngles() const {
    auto abs = verticesAbs();
    std::vector<double> out(abs.size(), 0.0);

    if (abs.size() < 3) return out;

    for (int i = 0; i < (int)abs.size(); ++i) {
        out[i] = angleAtVertex(abs, i);
    }

    return out;
}

bool PolygonShape::setAngleAt(int index, double targetDeg) {
    if (!angleEditingEnabled) return false;

    auto abs = verticesAbs();
    if (!setInternalAngleByMovingCurrentVertex(abs, index, targetDeg)) {
        return false;
    }

    std::vector<Point> rel;
    rel.reserve(abs.size());
    for (const auto& p : abs) {
        rel.push_back(p - anchor);
    }

    relVerts = rel;
    ensureStyleArrays();
    return true;
}

bool PolygonShape::setSideLength(int index, double targetLen) {
    if (targetLen <= 1.0) return false;

    if (trapezoidMode && relVerts.size() == 4) {
        double topLeftX = relVerts[0].x;
        double topLeftY = relVerts[0].y;
        double topRightX = relVerts[1].x;
        double topRightY = relVerts[1].y;
        double bottomRightX = relVerts[2].x;
        double bottomRightY = relVerts[2].y;
        double bottomLeftX = relVerts[3].x;
        double bottomLeftY = relVerts[3].y;

        double topWidth = topRightX - topLeftX;
        double bottomWidth = bottomRightX - bottomLeftX;

        if (topWidth <= 1.0 || bottomWidth <= 1.0) return false;

        if (index == 0) {
            double center = (topLeftX + topRightX) / 2.0;
            topWidth = targetLen;
            if (topWidth <= 1.0) return false;
            topLeftX = center - topWidth / 2.0;
            topRightX = center + topWidth / 2.0;
        } else if (index == 2) {
            double center = (bottomLeftX + bottomRightX) / 2.0;
            bottomWidth = targetLen;
            if (bottomWidth <= 1.0) return false;
            bottomLeftX = center - bottomWidth / 2.0;
            bottomRightX = center + bottomWidth / 2.0;
        } else if (index == 1) {
            if (isIsoscelesTrapezoid) {
                double dx = std::abs((bottomWidth - topWidth) / 2.0);
                if (targetLen <= dx) return false;

                double newHeight = std::sqrt(targetLen * targetLen - dx * dx);
                double newTopY = bottomRightY - newHeight;
                double center = (bottomLeftX + bottomRightX) / 2.0;

                topLeftX = center - topWidth / 2.0;
                topRightX = center + topWidth / 2.0;
                topLeftY = newTopY;
                topRightY = newTopY;
            } else {
                double vx = topRightX - bottomRightX;
                double vy = topRightY - bottomRightY;
                double len = std::sqrt(vx * vx + vy * vy);
                if (len < 1e-9) return false;

                double ux = vx / len;
                double uy = vy / len;

                topRightX = bottomRightX + ux * targetLen;
                topRightY = bottomRightY + uy * targetLen;

                topLeftX = topRightX - topWidth;
                topLeftY = topRightY;
            }
        } else if (index == 3) {
            if (isIsoscelesTrapezoid) {
                double dx = std::abs((bottomWidth - topWidth) / 2.0);
                if (targetLen <= dx) return false;

                double newHeight = std::sqrt(targetLen * targetLen - dx * dx);
                double newTopY = bottomLeftY - newHeight;
                double center = (bottomLeftX + bottomRightX) / 2.0;

                topLeftX = center - topWidth / 2.0;
                topRightX = center + topWidth / 2.0;
                topLeftY = newTopY;
                topRightY = newTopY;
            } else {
                double vx = topLeftX - bottomLeftX;
                double vy = topLeftY - bottomLeftY;
                double len = std::sqrt(vx * vx + vy * vy);
                if (len < 1e-9) return false;

                double ux = vx / len;
                double uy = vy / len;

                topLeftX = bottomLeftX + ux * targetLen;
                topLeftY = bottomLeftY + uy * targetLen;

                topRightX = topLeftX + topWidth;
                topRightY = topLeftY;
            }
        } else {
            return false;
        }

        if (topRightX - topLeftX <= 1.0) return false;
        if (bottomRightX - bottomLeftX <= 1.0) return false;

        relVerts = {
            {topLeftX, topLeftY},
            {topRightX, topRightY},
            {bottomRightX, bottomRightY},
            {bottomLeftX, bottomLeftY}
        };

        ensureStyleArrays();
        return true;
    }

    auto abs = verticesAbs();
    int n = (int)abs.size();
    if (index < 0 || index >= n) return false;

    int next = (index + 1) % n;
    Point a = abs[index];
    Point b = abs[next];

    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-9) return false;

    double scale = targetLen / len;
    Point newB(a.x + dx * scale, a.y + dy * scale);
    Point delta = newB - b;

    if (next == 0) {
        abs[0] = newB;
    } else {
        for (int i = next; i < n; ++i) {
            abs[i] = abs[i] + delta;
        }
    }

    std::vector<Point> rel;
    rel.reserve(n);
    for (const auto& p : abs) rel.push_back(p - anchor);
    relVerts = rel;

    return true;
}

Rectangle PolygonShape::boundingBox() const {
    auto points = verticesAbs();
    const int n = (int)points.size();
    if (n < 3) {
        return Shape::boundingBox();
    }

    std::vector<Vector2D> dirs(n);
    std::vector<Vector2D> normals(n);
    std::vector<double> halfWidths(n);
    std::vector<PointF2> outer(n);
    std::vector<PointF2> inner(n);

    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += points[i].x * points[j].y - points[j].x * points[i].y;
    }
    const bool isClockwise = area < 0.0;

    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        Point a = points[i];
        Point b = points[next];

        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9) len = 1e-9;

        dirs[i] = Vector2D(dx / len, dy / len);

        Vector2D normCCW(-dirs[i].Y, dirs[i].X);
        normals[i] = isClockwise ? Vector2D(dirs[i].Y, -dirs[i].X) : normCCW;

        double w = 3.0;
        if (i < (int)sideWidths.size()) w = sideWidths[i];
        halfWidths[i] = w / 2.0;
    }

    for (int i = 0; i < n; ++i) {
        int prev = (i - 1 + n) % n;
        Point v = points[i];

        Vector2D pPrev(v.x + normals[prev].X * halfWidths[prev],
                       v.y + normals[prev].Y * halfWidths[prev]);
        Vector2D pNext(v.x + normals[i].X * halfWidths[i],
                       v.y + normals[i].Y * halfWidths[i]);

        Vector2D* outInter = intersectLines(pPrev, dirs[prev], pNext, dirs[i]);
        if (outInter) {
            outer[i] = PointF2(outInter->X, outInter->Y);
            delete outInter;
        } else {
            outer[i] = PointF2(v.x, v.y);
        }

        Vector2D pPrevIn(v.x - normals[prev].X * halfWidths[prev],
                         v.y - normals[prev].Y * halfWidths[prev]);
        Vector2D pNextIn(v.x - normals[i].X * halfWidths[i],
                         v.y - normals[i].Y * halfWidths[i]);

        Vector2D* inInter = intersectLines(pPrevIn, dirs[prev], pNextIn, dirs[i]);
        if (inInter) {
            inner[i] = PointF2(inInter->X, inInter->Y);
            delete inInter;
        } else {
            inner[i] = PointF2(v.x, v.y);
        }
    }

    double minX = outer[0].x;
    double maxX = outer[0].x;
    double minY = outer[0].y;
    double maxY = outer[0].y;

    for (int i = 0; i < n; ++i) {
        minX = std::min(minX, outer[i].x);
        maxX = std::max(maxX, outer[i].x);
        minY = std::min(minY, outer[i].y);
        maxY = std::max(maxY, outer[i].y);

        minX = std::min(minX, inner[i].x);
        maxX = std::max(maxX, inner[i].x);
        minY = std::min(minY, inner[i].y);
        maxY = std::max(maxY, inner[i].y);
    }

    return {minX, minY, maxX - minX, maxY - minY};
}

QJsonObject PolygonShape::save() const {
    QJsonObject obj = saveBaseFields();
    obj["type"] = "Polygon";
    obj["displayName"] = displayName;
    obj["relVerts"] = pointsToJson(relVerts);
    obj["angleEditingEnabled"] = angleEditingEnabled;
    obj["trapezoidMode"] = trapezoidMode;
    obj["isIsoscelesTrapezoid"] = isIsoscelesTrapezoid;
    return obj;
}

void PolygonShape::loadFromJson(const QJsonObject& obj) {
    loadBaseFields(obj);

    displayName = obj["displayName"].toString("Многоугольник");
    relVerts = pointsFromJson(obj["relVerts"].toArray());
    angleEditingEnabled = obj["angleEditingEnabled"].toBool(true);
    trapezoidMode = obj["trapezoidMode"].toBool(false);
    isIsoscelesTrapezoid = obj["isIsoscelesTrapezoid"].toBool(false);

    const int n = std::max(1, sideCount());

    if (sideColors.empty()) {
        sideColors.assign(n, QColor(Qt::black));
    } else if ((int)sideColors.size() < n) {
        QColor last = sideColors.back();
        sideColors.resize(n, last);
    } else if ((int)sideColors.size() > n) {
        sideColors.resize(n);
    }

    if (sideWidths.empty()) {
        sideWidths.assign(n, 3.0);
    } else if ((int)sideWidths.size() < n) {
        double last = sideWidths.back();
        sideWidths.resize(n, last);
    } else if ((int)sideWidths.size() > n) {
        sideWidths.resize(n);
    }
}