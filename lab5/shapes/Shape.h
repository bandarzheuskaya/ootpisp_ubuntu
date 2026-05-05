#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <memory>
#include <vector>
#include <algorithm>
#include <cmath>

#include "Point.h"
#include "GeometryUtils.h"

enum class ShapeKind {
    Polygon,
    Ellipse,
    Group
};

class Shape {
public:
    virtual ~Shape() = default;

    inline static int s_nextId = 1;
    int id = 0;

    static int generateId() {
        return s_nextId++;
    }

    Point anchor{0.0, 0.0};
    QColor fillColor = Qt::transparent;
    std::vector<QColor> sideColors;
    std::vector<double> sideWidths;
    double currentScale = 100.0;

    virtual ShapeKind kind() const = 0;
    virtual QString kindName() const = 0;
    virtual std::vector<Point> verticesAbs() const = 0;
    virtual std::vector<Point> verticesRel() const = 0;
    virtual void setVerticesRel(const std::vector<Point>& rel) = 0;
    virtual void draw(QPainter& painter) const = 0;
    virtual bool contains(const Point& p) const = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual int sideCount() const = 0;

    virtual bool isGroup() const { return false; }
    virtual std::vector<Shape*> groupedShapes() { return {}; }

    virtual std::vector<double> sideLengths() const { return {}; }
    virtual bool setSideLength(int, double) { return false; }

    virtual bool supportsVerticesEditing() const { return true; }
    virtual bool supportsAngleEditing() const { return false; }
    virtual std::vector<double> interiorAngles() const { return {}; }
    virtual bool setAngleAt(int, double) { return false; }

    virtual void moveBy(const Point& delta) { anchor = anchor + delta; }

    virtual QJsonObject save() const = 0;

    static std::unique_ptr<Shape> load(const QJsonObject& obj);

    virtual Rectangle boundingBox() const {
        auto verts = verticesAbs();
        if (verts.empty()) return {anchor.x, anchor.y, 0.0, 0.0};

        double minX = verts[0].x;
        double maxX = verts[0].x;
        double minY = verts[0].y;
        double maxY = verts[0].y;

        for (const auto& p : verts) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        return {minX, minY, maxX - minX, maxY - minY};
    }

    bool isPointNearAnchor(const Point& p, double tolerance = 10.0) const {
        return distancePts(anchor, p) <= tolerance;
    }

    virtual void setAnchorInside(const Point& newAnchor) {
        auto rel = verticesRel();
        const Point delta = newAnchor - anchor;
        for (auto& v : rel) v = v - delta;
        setVerticesRel(rel);
        anchor = newAnchor;
    }

    virtual void scaleUniform(double factor) {
        if (factor <= 0.0) return;

        auto rel = verticesRel();
        if (rel.empty()) return;

        for (auto& p : rel) {
            p = p * factor;
        }

        setVerticesRel(rel);
        currentScale *= factor;
    }

    virtual void resizeToBoundingBox(const Rectangle& target) {
        Rectangle old = boundingBox();
        if (old.width <= 1e-9 || old.height <= 1e-9) return;

        auto abs = verticesAbs();
        if (abs.empty()) return;

        const double axNorm = (anchor.x - old.x) / old.width;
        const double ayNorm = (anchor.y - old.y) / old.height;

        const double sx = target.width / old.width;
        const double sy = target.height / old.height;
        const double scaleFactor = std::sqrt(std::abs(sx * sy));

        std::vector<Point> newAbs;
        newAbs.reserve(abs.size());

        for (const auto& p : abs) {
            double nx = target.x + (p.x - old.x) / old.width * target.width;
            double ny = target.y + (p.y - old.y) / old.height * target.height;
            newAbs.push_back({nx, ny});
        }

        Point newAnchor{
            target.x + axNorm * target.width,
            target.y + ayNorm * target.height
        };

        std::vector<Point> rel;
        rel.reserve(newAbs.size());
        for (const auto& p : newAbs) {
            rel.push_back(p - newAnchor);
        }

        setVerticesRel(rel);
        anchor = newAnchor;
        currentScale *= scaleFactor;
    }

    void ensureStyleArrays() {
        const int n = std::max(1, sideCount());
        if ((int)sideColors.size() != n) sideColors.assign(n, QColor(Qt::black));
        if ((int)sideWidths.size() != n) sideWidths.assign(n, 3.0);
    }

protected:
    QJsonObject saveBaseFields() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["anchorX"] = anchor.x;
        obj["anchorY"] = anchor.y;
        obj["fillColor"] = fillColor.name(QColor::HexArgb);
        obj["currentScale"] = currentScale;

        QJsonArray colors;
        for (const auto& c : sideColors) {
            colors.append(c.name(QColor::HexArgb));
        }
        obj["sideColors"] = colors;

        QJsonArray widths;
        for (double w : sideWidths) {
            widths.append(w);
        }
        obj["sideWidths"] = widths;

        return obj;
    }

    void loadBaseFields(const QJsonObject& obj) {
    id = obj["id"].toInt(0);
    anchor.x = obj["anchorX"].toDouble(anchor.x);
    anchor.y = obj["anchorY"].toDouble(anchor.y);
    fillColor = QColor(obj["fillColor"].toString("#00000000"));
    currentScale = obj["currentScale"].toDouble(100.0);

    sideColors.clear();
    QJsonArray colors = obj["sideColors"].toArray();
    for (const auto& v : colors) {
        sideColors.push_back(QColor(v.toString()));
    }

    sideWidths.clear();
    QJsonArray widths = obj["sideWidths"].toArray();
    for (const auto& v : widths) {
        sideWidths.push_back(v.toDouble());
    }

    if (id >= s_nextId) {
        s_nextId = id + 1;
    }
}

    static QJsonArray pointsToJson(const std::vector<Point>& pts) {
        QJsonArray arr;
        for (const auto& p : pts) {
            QJsonObject o;
            o["x"] = p.x;
            o["y"] = p.y;
            arr.append(o);
        }
        return arr;
    }

    static std::vector<Point> pointsFromJson(const QJsonArray& arr) {
        std::vector<Point> pts;
        pts.reserve(arr.size());

        for (const auto& v : arr) {
            QJsonObject o = v.toObject();
            pts.push_back({o["x"].toDouble(), o["y"].toDouble()});
        }

        return pts;
    }

    static bool pointInPolygon(const Point& p, const std::vector<Point>& poly) {
        if (poly.size() < 3) return false;

        bool result = false;
        int j = (int)poly.size() - 1;

        for (int i = 0; i < (int)poly.size(); ++i) {
            if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
                (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) /
                           ((poly[j].y - poly[i].y) + 1e-12) + poly[i].x)) {
                result = !result;
            }
            j = i;
        }

        return result;
    }

    void drawPolygonWithSharpBorder(QPainter& painter, const std::vector<Point>& points) const {
        const int n = (int)points.size();
        if (n < 3) return;

        if (fillColor != Qt::transparent && fillColor.alpha() > 0) {
            QPolygonF polygon;
            for (const auto& p : points) polygon << QPointF(p.x, p.y);
            painter.setBrush(QBrush(fillColor));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(polygon);
        }

        double area = 0.0;
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            area += points[i].x * points[j].y - points[j].x * points[i].y;
        }
        const bool isClockwise = area < 0.0;

        std::vector<PointF2> outer(n);
        std::vector<PointF2> inner(n);
        std::vector<Vector2D> dirs(n);
        std::vector<Vector2D> normals(n);
        std::vector<double> halfWidths(n);

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
            halfWidths[i] = sideWidths[i] / 2.0;
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

        for (int i = 0; i < n; ++i) {
            int next = (i + 1) % n;
            QPointF quad[4] = {
                QPointF(outer[i].x, outer[i].y),
                QPointF(outer[next].x, outer[next].y),
                QPointF(inner[next].x, inner[next].y),
                QPointF(inner[i].x, inner[i].y)
            };
            painter.setBrush(QBrush(sideColors[i]));
            painter.setPen(Qt::NoPen);
            painter.drawConvexPolygon(quad, 4);
        }
    }
};