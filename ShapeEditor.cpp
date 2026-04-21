#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QWheelEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QKeyEvent>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QListWidget>
#include <QAbstractItemView>
#include <QInputDialog>
#include <QDialog>
#include <QToolButton>
#include <QButtonGroup>
#include <QTimer>
#include <QPen>
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>

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

static double distancePts(const Point& a, const Point& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

static QColor contrastColor(const QColor& color) {
    if (!color.isValid() || color.alpha() == 0) return Qt::white;
    const double brightness = (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0;
    return brightness > 0.5 ? Qt::black : Qt::white;
}

static Vector2D* intersectLines(const Vector2D& p1, const Vector2D& d1, const Vector2D& p2, const Vector2D& d2) {
    const double det = d1.X * d2.Y - d1.Y * d2.X;
    if (std::abs(det) < 1e-9) return nullptr;
    const double t = ((p2.X - p1.X) * d2.Y - (p2.Y - p1.Y) * d2.X) / det;
    return new Vector2D(p1.X + d1.X * t, p1.Y + d1.Y * t);
}

static Point rotateAround(const Point& p, const Point& center, double radians) {
    const double s = std::sin(radians);
    const double c = std::cos(radians);
    const double dx = p.x - center.x;
    const double dy = p.y - center.y;
    return {center.x + dx * c - dy * s, center.y + dx * s + dy * c};
}

template <typename T>
static void clearLayoutItems(T* layout) {
    if (!layout) return;
    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (!item) break;
        if (item->layout()) {
            clearLayoutItems(item->layout());
            delete item->layout();
        }
        if (item->widget()) delete item->widget();
        delete item;
    }
}

enum class ShapeKind {
    Circle,
    Rectangle,
    Triangle,
    Hexagon,
    Trapezoid,
    CustomPolygon,
    Group
};

class Shape {
public:
    virtual ~Shape() = default;

    inline static int s_nextId = 1;
    int id = s_nextId++;

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
        id = obj["id"].toInt(id);
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

        ensureStyleArrays();

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

static double angleAtVertex(const std::vector<Point>& pts, int index) {
    const int n = (int)pts.size();
    if (n < 3 || index < 0 || index >= n) return 0.0;

    const Point prev = pts[(index - 1 + n) % n];
    const Point curr = pts[index];
    const Point next = pts[(index + 1) % n];

    const double v1x = prev.x - curr.x;
    const double v1y = prev.y - curr.y;
    const double v2x = next.x - curr.x;
    const double v2y = next.y - curr.y;

    const double l1 = std::sqrt(v1x * v1x + v1y * v1y);
    const double l2 = std::sqrt(v2x * v2x + v2y * v2y);
    if (l1 < 1e-9 || l2 < 1e-9) return 0.0;

    double dot = v1x * v2x + v1y * v2y;
    dot /= (l1 * l2);
    dot = std::max(-1.0, std::min(1.0, dot));

    double smallAngle = std::acos(dot) * 180.0 / M_PI; // 0..180

    double cross = v1x * v2y - v1y * v2x;

    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    const bool ccw = area > 0.0;

    const bool isConvex = ccw ? (cross < 0.0) : (cross > 0.0);

    return isConvex ? smallAngle : (360.0 - smallAngle);
}

static std::vector<Point> rotateTailFromIndex(
    const std::vector<Point>& pts,
    int pivotIndex,
    int startRotateIndex,
    double radians
) {
    std::vector<Point> out = pts;
    if (pivotIndex < 0 || pivotIndex >= (int)pts.size()) return out;
    if (startRotateIndex < 0 || startRotateIndex >= (int)pts.size()) return out;

    const Point center = pts[pivotIndex];

    for (int i = startRotateIndex; i < (int)out.size(); ++i) {
        out[i] = rotateAround(out[i], center, radians);
    }

    return out;
}

static bool setAngleByMovingCurrentVertexAlongBisector(
    std::vector<Point>& pts,
    int index,
    double targetDeg
) {
    const int n = (int)pts.size();
    if (n < 3 || index < 0 || index >= n) return false;
    if (targetDeg <= 1.0 || targetDeg >= 179.0) return false;

    const int prevIdx = (index - 1 + n) % n;
    const int nextIdx = (index + 1) % n;

    const Point prev = pts[prevIdx];
    const Point curr = pts[index];
    const Point next = pts[nextIdx];

    const double baseDx = next.x - prev.x;
    const double baseDy = next.y - prev.y;
    const double baseLen = std::sqrt(baseDx * baseDx + baseDy * baseDy);
    if (baseLen < 1e-9) return false;

    const Point mid{
        (prev.x + next.x) * 0.5,
        (prev.y + next.y) * 0.5
    };

    const double targetRad = targetDeg * M_PI / 180.0;
    const double tanHalf = std::tan(targetRad / 2.0);
    if (std::abs(tanHalf) < 1e-9) return false;

    const double h = (baseLen * 0.5) / tanHalf;

    // единичный перпендикуляр к основанию
    const double nx = -baseDy / baseLen;
    const double ny =  baseDx / baseLen;

    Point cand1{ mid.x + nx * h, mid.y + ny * h };
    Point cand2{ mid.x - nx * h, mid.y - ny * h };

    auto polygonSignedAreaLocal = [](const std::vector<Point>& poly) {
        double area = 0.0;
        for (int i = 0; i < (int)poly.size(); ++i) {
            int j = (i + 1) % (int)poly.size();
            area += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
        }
        return area * 0.5;
    };

    auto orient = [](const Point& a, const Point& b, const Point& c) {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    auto onSegment = [](const Point& a, const Point& b, const Point& p) {
        return std::min(a.x, b.x) - 1e-9 <= p.x && p.x <= std::max(a.x, b.x) + 1e-9 &&
               std::min(a.y, b.y) - 1e-9 <= p.y && p.y <= std::max(a.y, b.y) + 1e-9;
    };

    auto segmentsIntersect = [&](const Point& a, const Point& b, const Point& c, const Point& d) {
        double o1 = orient(a, b, c);
        double o2 = orient(a, b, d);
        double o3 = orient(c, d, a);
        double o4 = orient(c, d, b);

        if (((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) &&
            ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0))) {
            return true;
        }

        if (std::abs(o1) < 1e-9 && onSegment(a, b, c)) return true;
        if (std::abs(o2) < 1e-9 && onSegment(a, b, d)) return true;
        if (std::abs(o3) < 1e-9 && onSegment(c, d, a)) return true;
        if (std::abs(o4) < 1e-9 && onSegment(c, d, b)) return true;

        return false;
    };

    auto hasSelfIntersection = [&](const std::vector<Point>& poly) {
        int m = (int)poly.size();
        if (m < 4) return false;

        for (int i = 0; i < m; ++i) {
            int i2 = (i + 1) % m;
            for (int j = i + 1; j < m; ++j) {
                int j2 = (j + 1) % m;

                if (i == j) continue;
                if (i2 == j) continue;
                if (j2 == i) continue;
                if (i == 0 && j2 == 0) continue;

                if (segmentsIntersect(poly[i], poly[i2], poly[j], poly[j2])) {
                    return true;
                }
            }
        }
        return false;
    };

    std::vector<Point> test1 = pts;
    std::vector<Point> test2 = pts;
    test1[index] = cand1;
    test2[index] = cand2;

    bool ok1 = !hasSelfIntersection(test1) && std::abs(polygonSignedAreaLocal(test1)) > 1e-6;
    bool ok2 = !hasSelfIntersection(test2) && std::abs(polygonSignedAreaLocal(test2)) > 1e-6;

    if (!ok1 && !ok2) return false;

    if (ok1 && !ok2) {
        pts[index] = cand1;
        return true;
    }
    if (!ok1 && ok2) {
        pts[index] = cand2;
        return true;
    }

    // Если обе подходят, берём ту, что ближе к текущему положению вершины
    double d1 = distancePts(curr, cand1);
    double d2 = distancePts(curr, cand2);
    pts[index] = (d1 <= d2) ? cand1 : cand2;
    return true;
}

static double polygonSignedArea(const std::vector<Point>& pts) {
    if (pts.size() < 3) return 0.0;
    double area = 0.0;
    for (int i = 0; i < (int)pts.size(); ++i) {
        int j = (i + 1) % (int)pts.size();
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return area * 0.5;
}

static bool segmentsProperlyIntersect(const Point& a, const Point& b, const Point& c, const Point& d) {
    auto orient = [](const Point& p, const Point& q, const Point& r) {
        double v = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        if (std::abs(v) < 1e-9) return 0.0;
        return v;
    };

    auto onSegment = [](const Point& p, const Point& q, const Point& r) {
        return std::min(p.x, r.x) - 1e-9 <= q.x && q.x <= std::max(p.x, r.x) + 1e-9 &&
               std::min(p.y, r.y) - 1e-9 <= q.y && q.y <= std::max(p.y, r.y) + 1e-9;
    };

    double o1 = orient(a, b, c);
    double o2 = orient(a, b, d);
    double o3 = orient(c, d, a);
    double o4 = orient(c, d, b);

    if ((o1 > 0.0 && o2 < 0.0 || o1 < 0.0 && o2 > 0.0) &&
        (o3 > 0.0 && o4 < 0.0 || o3 < 0.0 && o4 > 0.0)) {
        return true;
    }

    if (std::abs(o1) < 1e-9 && onSegment(a, c, b)) return true;
    if (std::abs(o2) < 1e-9 && onSegment(a, d, b)) return true;
    if (std::abs(o3) < 1e-9 && onSegment(c, a, d)) return true;
    if (std::abs(o4) < 1e-9 && onSegment(c, b, d)) return true;

    return false;
}

static bool polygonHasSelfIntersection(const std::vector<Point>& pts) {
    const int n = (int)pts.size();
    if (n < 4) return false;

    for (int i = 0; i < n; ++i) {
        int i2 = (i + 1) % n;
        for (int j = i + 1; j < n; ++j) {
            int j2 = (j + 1) % n;

            if (i == j) continue;
            if (i2 == j) continue;
            if (j2 == i) continue;
            if (i == 0 && j2 == 0) continue;

            if (segmentsProperlyIntersect(pts[i], pts[i2], pts[j], pts[j2])) {
                return true;
            }
        }
    }
    return false;
}

static bool setInternalAngleByMovingCurrentVertex(
    std::vector<Point>& pts,
    int index,
    double targetDeg
) {
    const int n = (int)pts.size();
    if (n < 3 || index < 0 || index >= n) return false;

    targetDeg = std::clamp(targetDeg, 1.0, 359.0);

    const int prevIdx = (index - 1 + n) % n;
    const int nextIdx = (index + 1) % n;

    const Point prev = pts[prevIdx];
    const Point curr = pts[index];
    const Point next = pts[nextIdx];

    const double baseDx = next.x - prev.x;
    const double baseDy = next.y - prev.y;
    const double baseLen = std::sqrt(baseDx * baseDx + baseDy * baseDy);
    if (baseLen < 1e-9) return false;

    const Point mid{
        (prev.x + next.x) * 0.5,
        (prev.y + next.y) * 0.5
    };

    double betaDeg = targetDeg;
    if (betaDeg > 180.0) betaDeg = 360.0 - betaDeg;
    betaDeg = std::clamp(betaDeg, 1.0, 179.0);

    const double betaRad = betaDeg * M_PI / 180.0;
    const double tanHalf = std::tan(betaRad / 2.0);
    if (std::abs(tanHalf) < 1e-9) return false;

    const double h = (baseLen * 0.5) / tanHalf;

    const double nx = -baseDy / baseLen;
    const double ny =  baseDx / baseLen;

    Point cand1{ mid.x + nx * h, mid.y + ny * h };
    Point cand2{ mid.x - nx * h, mid.y - ny * h };

    std::vector<Point> test1 = pts;
    std::vector<Point> test2 = pts;
    test1[index] = cand1;
    test2[index] = cand2;

    bool ok1 = !polygonHasSelfIntersection(test1) && std::abs(polygonSignedArea(test1)) > 1e-6;
    bool ok2 = !polygonHasSelfIntersection(test2) && std::abs(polygonSignedArea(test2)) > 1e-6;

    if (!ok1 && !ok2) return false;

    double err1 = 1e18;
    double err2 = 1e18;

    if (ok1) err1 = std::abs(angleAtVertex(test1, index) - targetDeg);
    if (ok2) err2 = std::abs(angleAtVertex(test2, index) - targetDeg);

    if (ok1 && !ok2) {
        pts[index] = cand1;
        return true;
    }
    if (!ok1 && ok2) {
        pts[index] = cand2;
        return true;
    }

    if (err1 < err2) {
        pts[index] = cand1;
        return true;
    }
    if (err2 < err1) {
        pts[index] = cand2;
        return true;
    }

    double d1 = distancePts(curr, cand1);
    double d2 = distancePts(curr, cand2);
    pts[index] = (d1 <= d2) ? cand1 : cand2;
    return true;
}

class PolygonShape : public Shape {
public:
    PolygonShape(ShapeKind k, const QString& n) : m_kind(k), m_name(n) {}

    ShapeKind kind() const override { return m_kind; }
    QString kindName() const override { return m_name; }
    Rectangle boundingBox() const override;

    std::vector<Point> relVerts;

    std::vector<Point> verticesAbs() const override {
        std::vector<Point> out;
        out.reserve(relVerts.size());
        for (const auto& p : relVerts) out.push_back(anchor + p);
        return out;
    }

    std::vector<Point> verticesRel() const override { return relVerts; }

    void setVerticesRel(const std::vector<Point>& rel) override {
        if (rel.size() >= 3) relVerts = rel;
        ensureStyleArrays();
    }

    void draw(QPainter& painter) const override {
        drawPolygonWithSharpBorder(painter, verticesAbs());
    }

    bool contains(const Point& p) const override {
        return pointInPolygon(p, verticesAbs());
    }

    int sideCount() const override { return (int)relVerts.size(); }

    

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<PolygonShape>(m_kind, m_name);
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->currentScale = currentScale;
        return s;
    }

    std::vector<double> sideLengths() const override {
        auto abs = verticesAbs();
        std::vector<double> out;
        out.reserve(abs.size());
        for (int i = 0; i < (int)abs.size(); ++i) {
            out.push_back(distancePts(abs[i], abs[(i + 1) % abs.size()]));
        }
        return out;
    }

    bool supportsAngleEditing() const override { return m_kind == ShapeKind::CustomPolygon; }

    std::vector<double> interiorAngles() const override {
    auto abs = verticesAbs();
    std::vector<double> out(abs.size(), 0.0);

    if (abs.size() < 3) return out;

    for (int i = 0; i < (int)abs.size(); ++i) {
        out[i] = angleAtVertex(abs, i);
    }

    return out;
}

bool setAngleAt(int index, double targetDeg) override {
    if (m_kind != ShapeKind::CustomPolygon) return false;

    targetDeg = std::clamp(targetDeg, 1.0, 359.0);

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

    bool setSideLength(int index, double targetLen) override {
        if (targetLen <= 1.0) return false;
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
            for (int i = next; i < n; ++i) abs[i] = abs[i] + delta;
        }

        std::vector<Point> rel;
        rel.reserve(n);
        for (const auto& p : abs) rel.push_back(p - anchor);
        relVerts = rel;
        return true;
    }

    QJsonObject save() const override {
    QJsonObject obj = saveBaseFields();

    QString kindStr;
    switch (m_kind) {
        case ShapeKind::Rectangle: kindStr = "Rectangle"; break;
        case ShapeKind::Triangle: kindStr = "Triangle"; break;
        case ShapeKind::Hexagon: kindStr = "Hexagon"; break;
        case ShapeKind::Trapezoid: kindStr = "Trapezoid"; break;
        case ShapeKind::CustomPolygon: kindStr = "CustomPolygon"; break;
        default: kindStr = "Polygon"; break;
    }

    obj["type"] = kindStr;
    obj["name"] = m_name;
    obj["relVerts"] = pointsToJson(relVerts);
    return obj;
}

void loadPolygonData(const QJsonObject& obj) {
    loadBaseFields(obj);
    relVerts = pointsFromJson(obj["relVerts"].toArray());
    ensureStyleArrays();
}

private:
    ShapeKind m_kind;
    QString m_name;
};



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

class RectangleShape : public PolygonShape {
public:
    RectangleShape() : PolygonShape(ShapeKind::Rectangle, "Прямоугольник") {
        relVerts = {{0, 0}, {220, 0}, {220, 150}, {0, 150}};
        ensureStyleArrays();
    }

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<RectangleShape>();
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->currentScale = currentScale;
        return s;
    }

    bool setSideLength(int index, double targetLen) override {
        if (targetLen <= 1.0) return false;

        double width = relVerts[1].x - relVerts[0].x;
        double height = relVerts[2].y - relVerts[1].y;

        if (index == 0 || index == 2) {
            width = targetLen;
        } else if (index == 1 || index == 3) {
            height = targetLen;
        } else {
            return false;
        }

        relVerts = {
            {0, 0},
            {width, 0},
            {width, height},
            {0, height}
        };
        ensureStyleArrays();
        return true;
    }

    void resizeToBoundingBox(const Rectangle& target) override {
    if (target.width < 1.0 || target.height < 1.0) return;

    Rectangle old = boundingBox();
    if (old.width > 1e-9 && old.height > 1e-9) {
        const double sx = target.width / old.width;
        const double sy = target.height / old.height;
        const double scaleFactor = std::sqrt(std::abs(sx * sy));
        currentScale *= scaleFactor;
    }

    anchor = {target.x, target.y};
    relVerts = {
        {0, 0},
        {target.width, 0},
        {target.width, target.height},
        {0, target.height}
    };
    ensureStyleArrays();
}
};

class TriangleShape : public PolygonShape {
public:
    TriangleShape() : PolygonShape(ShapeKind::Triangle, "Треугольник") {
        relVerts = {{0, 0}, {180, 0}, {90, 156}};
        ensureStyleArrays();
    }

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<TriangleShape>();
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->currentScale = currentScale;
        return s;
    }

    bool supportsAngleEditing() const override {
        return true;
    }

    std::vector<double> interiorAngles() const override {
        auto abs = verticesAbs();
        std::vector<double> out(abs.size(), 0.0);
        if (abs.size() < 3) return out;

        for (int i = 0; i < (int)abs.size(); ++i) {
            out[i] = angleAtVertex(abs, i);
        }

        return out;
    }

    bool setAngleAt(int index, double targetDeg) override {
        targetDeg = std::clamp(targetDeg, 1.0, 179.0);

        auto abs = verticesAbs();
        const int n = (int)abs.size();
        if (n != 3 || index < 0 || index >= n) return false;

        if (!setInternalAngleByMovingCurrentVertex(abs, index, targetDeg)) {
            return false;
        }

        std::vector<Point> newRel;
        newRel.reserve(abs.size());
        for (const auto& p : abs) {
            newRel.push_back(p - anchor);
        }

        relVerts = newRel;
        ensureStyleArrays();
        return true;
    }
};

class HexagonShape : public PolygonShape {
public:
    HexagonShape() : PolygonShape(ShapeKind::Hexagon, "Шестиугольник") {
        relVerts = {
            {60, 0},
            {180, 0},
            {240, 90},
            {180, 180},
            {60, 180},
            {0, 90}
        };
        ensureStyleArrays();
    }

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<HexagonShape>();
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->currentScale = currentScale;
        return s;
    }

    bool supportsAngleEditing() const override {
        return true;
    }

   std::vector<double> interiorAngles() const override {
    auto abs = verticesAbs();
    std::vector<double> out(abs.size(), 0.0);
    if (abs.size() < 3) return out;

    for (int i = 0; i < (int)abs.size(); ++i) {
        out[i] = angleAtVertex(abs, i);
    }

    return out;
}
   bool setSideLength(int index, double targetLen) override {
    if (targetLen <= 1.0) return false;

    auto abs = verticesAbs();
    int n = (int)abs.size();
    if (n != 6 || index < 0 || index >= n) return false;

    int next = (index + 1) % n;

    Point a = abs[index];
    Point b = abs[next];

    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-9) return false;

    double k = targetLen / len;
    Point newB = {a.x + dx * k, a.y + dy * k};
    Point delta = newB - b;

    abs[next] = newB;

    int next2 = (next + 1) % n;
    if (next2 != index) {
        abs[next2] = abs[next2] + delta * 0.5;
    }

    std::vector<Point> newRel;
    newRel.reserve(n);
    for (const auto& p : abs) {
        newRel.push_back(p - anchor);
    }

    relVerts = newRel;
    ensureStyleArrays();
    return true;
}
  bool setAngleAt(int index, double targetDeg) override {
    targetDeg = std::clamp(targetDeg, 1.0, 359.0);

    auto abs = verticesAbs();
    const int n = (int)abs.size();
    if (n != 6 || index < 0 || index >= n) return false;

    if (!setInternalAngleByMovingCurrentVertex(abs, index, targetDeg)) {
        return false;
    }

    std::vector<Point> newRel;
    newRel.reserve(abs.size());
    for (const auto& p : abs) {
        newRel.push_back(p - anchor);
    }

    relVerts = newRel;
    ensureStyleArrays();
    return true;
}

};

class TrapezoidShape : public PolygonShape {
public:
    bool isIsosceles = false;

    TrapezoidShape() : PolygonShape(ShapeKind::Trapezoid, "Трапеция") {
        relVerts = {
            {50, 0},
            {210, 0},
            {260, 140},
            {0, 140}
        };
        ensureStyleArrays();
    }

    QJsonObject save() const override {
    QJsonObject obj = PolygonShape::save();
    obj["type"] = "Trapezoid";
    obj["isIsosceles"] = isIsosceles;
    return obj;
}

void loadTrapezoidData(const QJsonObject& obj) {
    loadPolygonData(obj);
    isIsosceles = obj["isIsosceles"].toBool(false);
}

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<TrapezoidShape>();
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->isIsosceles = isIsosceles;
        s->currentScale = currentScale;
        return s;
    }

    bool setSideLength(int index, double targetLen) override {
        if (targetLen <= 1.0) return false;

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
        }
        else if (index == 2) {
            double center = (bottomLeftX + bottomRightX) / 2.0;
            bottomWidth = targetLen;
            if (bottomWidth <= 1.0) return false;

            bottomLeftX = center - bottomWidth / 2.0;
            bottomRightX = center + bottomWidth / 2.0;
        }
        else if (index == 1) {
            if (isIsosceles) {
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
        }
        else if (index == 3) {
            if (isIsosceles) {
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
        }
        else {
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

    void resizeToBoundingBox(const Rectangle& target) override {
    if (target.width < 2.0 || target.height < 2.0) return;

    Rectangle old = boundingBox();
    if (old.width > 1e-9 && old.height > 1e-9) {
        const double sx = target.width / old.width;
        const double sy = target.height / old.height;
        const double scaleFactor = std::sqrt(std::abs(sx * sy));
        currentScale *= scaleFactor;
    }

    double currentTopWidth = relVerts[1].x - relVerts[0].x;
    double currentBottomWidth = relVerts[2].x - relVerts[3].x;
    double ratio = 0.6;

    if (currentBottomWidth > 1e-9) {
        ratio = currentTopWidth / currentBottomWidth;
    }

    ratio = std::clamp(ratio, 0.1, 0.95);

    double bottomWidth = target.width;
    double topWidth = bottomWidth * ratio;

    double topLeftX = (bottomWidth - topWidth) / 2.0;
    double topRightX = topLeftX + topWidth;
    double bottomLeftX = 0.0;
    double bottomRightX = bottomWidth;

    anchor = {target.x, target.y};
    relVerts = {
        {topLeftX, 0.0},
        {topRightX, 0.0},
        {bottomRightX, target.height},
        {bottomLeftX, target.height}
    };

    ensureStyleArrays();
}

    std::vector<double> sideLengths() const override {
        auto abs = verticesAbs();
        std::vector<double> out;
        out.reserve(abs.size());

        for (int i = 0; i < (int)abs.size(); ++i) {
            out.push_back(distancePts(abs[i], abs[(i + 1) % abs.size()]));
        }

        return out;
    }
};

class CustomPolygonShape : public PolygonShape {
public:
    CustomPolygonShape() : PolygonShape(ShapeKind::CustomPolygon, "Кастомная фигура") {
        relVerts = {{0, 0}, {160, 0}, {200, 100}, {40, 160}};
        ensureStyleArrays();
    }

    std::unique_ptr<Shape> clone() const override {
        auto s = std::make_unique<CustomPolygonShape>();
        s->id = id;
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        s->currentScale = currentScale;
        return s;
    }
};

class CircleShape : public Shape {
public:
    double radius = 80.0;
    Point centerOffset{0.0, 0.0}; // смещение центра окружности относительно anchor

    Point circleCenter() const {
        return anchor + centerOffset;
    }

    ShapeKind kind() const override { return ShapeKind::Circle; }
    QString kindName() const override { return "Окружность"; }

    std::vector<Point> verticesAbs() const override {
        Point c = circleCenter();
        return {
            {c.x - radius, c.y - radius},
            {c.x + radius, c.y + radius}
        };
    }

    std::vector<Point> verticesRel() const override {
        return {centerOffset};
    }

    void setVerticesRel(const std::vector<Point>& rel) override {
        if (!rel.empty()) {
            centerOffset = rel[0];
        }
    }

    int sideCount() const override { return 1; }
    bool supportsVerticesEditing() const override { return false; }

    std::unique_ptr<Shape> clone() const override {
        auto c = std::make_unique<CircleShape>();
        c->id = id;
        c->anchor = anchor;
        c->radius = radius;
        c->centerOffset = centerOffset;
        c->fillColor = fillColor;
        c->sideColors = sideColors;
        c->sideWidths = sideWidths;
        c->currentScale = currentScale;
        return c;
    }

    Rectangle boundingBox() const override {
    Point c = circleCenter();

    double stroke = 3.0;
    if (!sideWidths.empty()) {
        stroke = sideWidths[0];
    }

    double extra = stroke / 2.0;

    return {
        c.x - radius - extra,
        c.y - radius - extra,
        (radius + extra) * 2.0,
        (radius + extra) * 2.0
    };
}

    std::vector<double> sideLengths() const override {
        return {2.0 * M_PI * radius};
    }

    bool setSideLength(int, double target) override {
        if (target <= 1.0) return false;
        radius = target / (2.0 * M_PI);
        return true;
    }

    void moveBy(const Point& delta) override {
        anchor = anchor + delta;
    }

    void scaleUniform(double factor) override {
        if (factor <= 0.0) return;
        radius = std::max(1.0, radius * factor);
        centerOffset = centerOffset * factor;
        currentScale *= factor;
    }

    void resizeToBoundingBox(const Rectangle& target) override {
    Rectangle old = boundingBox();
    if (old.width <= 1e-9 || old.height <= 1e-9) return;

    Point oldCenter = circleCenter();

    const double anchorNormX = (anchor.x - old.x) / old.width;
    const double anchorNormY = (anchor.y - old.y) / old.height;

    const double centerNormX = (oldCenter.x - old.x) / old.width;
    const double centerNormY = (oldCenter.y - old.y) / old.height;

    Point newAnchor{
        target.x + anchorNormX * target.width,
        target.y + anchorNormY * target.height
    };

    Point newCenter{
        target.x + centerNormX * target.width,
        target.y + centerNormY * target.height
    };

    double stroke = 3.0;
    if (!sideWidths.empty()) {
        stroke = sideWidths[0];
    }
    double extra = stroke / 2.0;

    double newDiameter = std::min(target.width, target.height) - 2.0 * extra;
    newDiameter = std::max(2.0, newDiameter);

    double oldDiameter = radius * 2.0;
    radius = newDiameter / 2.0;

    anchor = newAnchor;
    centerOffset = newCenter - anchor;

    if (oldDiameter > 1e-9) {
        currentScale *= (newDiameter / oldDiameter);
    }
}

    void draw(QPainter& painter) const override {
        Point c = circleCenter();
        QRectF rect(c.x - radius, c.y - radius, radius * 2.0, radius * 2.0);

        if (fillColor != Qt::transparent && fillColor.alpha() > 0) {
            painter.setBrush(fillColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(rect);
        }

        QPen pen(sideColors.empty() ? QColor(Qt::black) : sideColors[0],
                 sideWidths.empty() ? 3.0 : sideWidths[0]);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(pen);
        painter.drawEllipse(rect);
    }

    bool contains(const Point& p) const override {
        return distancePts(circleCenter(), p) <= radius;
    }

    void setAnchorInside(const Point& newAnchor) override {
        Point c = circleCenter();     // сам круг остаётся на месте
        anchor = newAnchor;           // меняется только точка привязки
        centerOffset = c - anchor;    // пересчитываем смещение центра
    }

    QJsonObject save() const override {
    QJsonObject obj = saveBaseFields();
    obj["type"] = "Circle";
    obj["radius"] = radius;
    obj["centerOffsetX"] = centerOffset.x;
    obj["centerOffsetY"] = centerOffset.y;
    return obj;
}

void loadCircleData(const QJsonObject& obj) {
    loadBaseFields(obj);
    radius = obj["radius"].toDouble(80.0);
    centerOffset.x = obj["centerOffsetX"].toDouble(0.0);
    centerOffset.y = obj["centerOffsetY"].toDouble(0.0);
    ensureStyleArrays();
}
};


class GroupShape : public Shape {
public:
    std::vector<std::unique_ptr<Shape>> children;
    std::vector<Point> offsets;

    ShapeKind kind() const override { return ShapeKind::Group; }
    QString kindName() const override { return "Составная фигура"; }
    bool isGroup() const override { return true; }
    bool supportsVerticesEditing() const override { return false; }

    std::vector<Point> verticesAbs() const override {
        Rectangle b = boundingBox();
        return {
            {b.x, b.y},
            {b.x + b.width, b.y},
            {b.x + b.width, b.y + b.height},
            {b.x, b.y + b.height}
        };
    }

    std::vector<Point> verticesRel() const override { return {}; }
    void setVerticesRel(const std::vector<Point>&) override {}

    int sideCount() const override { return 0; }

    std::vector<Shape*> groupedShapes() override {
        std::vector<Shape*> out;
        for (auto& c : children) out.push_back(c.get());
        return out;
    }

    std::unique_ptr<Shape> clone() const override {
        auto g = std::make_unique<GroupShape>();
        g->id = id;
        g->anchor = anchor;
        g->offsets = offsets;
        g->fillColor = fillColor;
g->sideColors = sideColors;
g->sideWidths = sideWidths;
g->currentScale = currentScale;

        for (const auto& c : children) {
            g->children.push_back(c->clone());
        }
        return g;
    }

    Rectangle boundingBox() const override {
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

    void draw(QPainter& painter) const override {
        for (const auto& c : children) {
            c->draw(painter);
        }
    }

    bool contains(const Point& p) const override {
        for (const auto& c : children) {
            if (c->contains(p)) return true;
        }
        return false;
    }

    void moveBy(const Point& delta) override {
        anchor = anchor + delta;

        for (auto& child : children) {
            child->moveBy(delta);
        }

        rebuildOffsets();
    }

    void setAnchorInside(const Point& newAnchor) override {
    anchor = newAnchor;
    rebuildOffsets();
}

void scaleUniform(double factor) override {
    if (factor <= 0.0) return;
    if (children.empty()) return;

    for (auto& child : children) {
        Point oldChildAnchor = child->anchor;
        Point oldOffset = oldChildAnchor - anchor;
        Point newChildAnchor = anchor + oldOffset * factor;

        child->scaleUniform(factor);

        Point delta = newChildAnchor - child->anchor;
        child->moveBy(delta);
    }

    rebuildOffsets();
    currentScale *= factor;
}

void resizeToBoundingBox(const Rectangle& target) override {
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

    rebuildOffsets();

    const double scaleFactor = std::sqrt(std::abs(sx * sy));
    currentScale *= scaleFactor;
}
    void rebuildOffsets() {
        offsets.clear();
        offsets.reserve(children.size());

        for (const auto& child : children) {
            offsets.push_back(child->anchor - anchor);
        }
    }

    void setAnchorToBoundingBoxCenter() {
        Rectangle b = boundingBox();
        anchor = {b.x + b.width / 2.0, b.y + b.height / 2.0};
        rebuildOffsets();
    }

    QJsonObject save() const override {
    QJsonObject obj = saveBaseFields();
    obj["type"] = "Group";

    QJsonArray childrenArray;
    for (const auto& child : children) {
        childrenArray.append(child->save());
    }
    obj["children"] = childrenArray;

    return obj;
}

void loadGroupData(const QJsonObject& obj) {
    loadBaseFields(obj);
    children.clear();

    QJsonArray arr = obj["children"].toArray();
    for (const auto& v : arr) {
        QJsonObject childObj = v.toObject();
        std::unique_ptr<Shape> child = Shape::load(childObj);
        if (child) {
            children.push_back(std::move(child));
        }
    }

    rebuildOffsets();
}
};

std::unique_ptr<Shape> Shape::load(const QJsonObject& obj) {
    QString type = obj["type"].toString();

    std::unique_ptr<Shape> shape;

    if (type == "Circle") {
        auto s = std::make_unique<CircleShape>();
        s->loadCircleData(obj);
        shape = std::move(s);
    }
    else if (type == "Rectangle") {
        auto s = std::make_unique<RectangleShape>();
        s->loadPolygonData(obj);
        shape = std::move(s);
    }
    else if (type == "Triangle") {
        auto s = std::make_unique<TriangleShape>();
        s->loadPolygonData(obj);
        shape = std::move(s);
    }
    else if (type == "Hexagon") {
        auto s = std::make_unique<HexagonShape>();
        s->loadPolygonData(obj);
        shape = std::move(s);
    }
    else if (type == "Trapezoid") {
        auto s = std::make_unique<TrapezoidShape>();
        s->loadTrapezoidData(obj);
        shape = std::move(s);
    }
    else if (type == "CustomPolygon") {
        auto s = std::make_unique<CustomPolygonShape>();
        s->loadPolygonData(obj);
        shape = std::move(s);
    }
    else if (type == "Group") {
        auto s = std::make_unique<GroupShape>();
        s->loadGroupData(obj);
        shape = std::move(s);
    }

    return shape;
}

class MainWindow : public QMainWindow {
public:
    MainWindow() {
    setupUi();
    applyLightTheme();
    createInitialShapes();
    resize(1500, 920);
    showFullScreen();

    m_viewOffset = {
        (m_canvas->width() - m_pageWidth * m_viewScale) / 2.0,
        (m_canvas->height() - m_pageHeight * m_viewScale) / 2.0
    };

    updatePropertiesPanel();
    m_canvas->update();
}

    ~MainWindow() override {
    clearShapesArray();
}

protected:
       bool eventFilter(QObject* obj, QEvent* event) override {
    if (obj == m_canvas) {
        if (event->type() == QEvent::Paint) {
            paintCanvas();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            onMousePress(static_cast<QMouseEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            onMouseDoubleClick(static_cast<QMouseEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::MouseMove) {
            onMouseMove(static_cast<QMouseEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            onMouseRelease(static_cast<QMouseEvent*>(event));
            return true;
        }
        if (event->type() == QEvent::Wheel) {
            onWheel(static_cast<QWheelEvent*>(event));
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

        void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Delete) {
            deleteSelected();
            return;
        }

        if (event->key() == Qt::Key_Escape) {
            cancelCustomMode();
            return;
        }

        QMainWindow::keyPressEvent(event);
    }

private:
    QWidget* m_canvas = nullptr;
    QWidget* m_propertiesPanel = nullptr;
    QWidget* m_leftPanel = nullptr;
    QStatusBar* m_statusBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_togglePropertiesBtn = nullptr;
    QPushButton* m_toggleLeftBtn = nullptr;
    QCheckBox* m_trapezoidIsoscelesCheck = nullptr;
QColor m_pageColor = Qt::white;
    QWidget* m_shapesDock = nullptr;
    QPushButton* m_toggleShapesListBtn = nullptr;

    QWidget* m_createPanelBox = nullptr;
    QWidget* m_shapesListPanelBox = nullptr;
    QPushButton* m_showShapesListBtn = nullptr;
    QListWidget* m_allShapesList = nullptr;

    static const int MAX_SHAPES = 1000;     
    Shape* m_shapes[MAX_SHAPES] = {nullptr};
    int m_shapeCount = 0;

    std::vector<Shape*> m_selectedShapes;
    Shape* m_selectedShape = nullptr;
    Shape* m_propertiesShape = nullptr;

    int m_highlightVertexIndex = -1;
    Shape* m_highlightVertexShape  = nullptr;
    int m_highlightAngleIndex = -1;
    Shape* m_highlightAngleShape = nullptr;

    bool m_dragging = false;
    bool m_dragAnchor = false;
    bool m_resizing = false;
    int m_resizeHandleIndex = -1;
    Point m_lastMouse{0.0, 0.0};
    Rectangle m_resizeStartBounds;
    Point m_resizeStartMouse{0.0, 0.0};

    int m_highlightSideIndex = -1;
    Shape* m_highlightShape = nullptr;

    Point m_viewOffset{200.0, 120.0};   // положение листа внутри окна
    bool m_panningCanvas = false;       // сейчас двигаем холст средней кнопкой
    Point m_panStartMouse{0.0, 0.0};

    double m_pageWidth = 3200.0;        // размер белого листа
    double m_pageHeight = 2200.0;

    double m_viewScale = 1.0;           // масштаб просмотра
    double m_minViewScale = 0.2;
    double m_maxViewScale = 5.0;

    bool m_updatingUi = false;

    QLabel* m_boundsLabel = nullptr;
    QSpinBox* m_anchorX = nullptr;
    QSpinBox* m_anchorY = nullptr;
    QPushButton* m_applyAnchorBtn = nullptr;
    QSpinBox* m_anchorInsideX = nullptr;
    QSpinBox* m_anchorInsideY = nullptr;
    QPushButton* m_applyAnchorInsideBtn = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QPushButton* m_applyScaleBtn = nullptr;
    QCheckBox* m_noFillCheck = nullptr;
    QPushButton* m_fillButton = nullptr;
    QColor m_pendingFill = Qt::transparent;
    QTableWidget* m_verticesTable = nullptr;
    QPushButton* m_applyVerticesBtn = nullptr;
    QWidget* m_sidesWidget = nullptr;
    QVBoxLayout* m_sidesLayout = nullptr;
    QListWidget* m_groupChildrenList = nullptr;
    QPushButton* m_removeChildBtn = nullptr;
    QGroupBox* m_groupChildrenBox = nullptr;

    QPushButton* m_startCustomPointsBtn = nullptr;
    QPushButton* m_startCustomAnglesBtn = nullptr;
    QPushButton* m_createRegularPolygonBtn = nullptr;
    QPushButton* m_finishCustomBtn = nullptr;
    bool m_isPlacingCustomShape = false;
    double m_currentBuildDirection = 0.0; // направление последней построенной стороны в радианах
    int m_customShapeMode = 0;
    std::vector<Point> m_tempAbsolutePoints;

    void saveToFile() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить фигуры",
        "",
        "Shape Editor Files (*.json);;Text Files (*.txt);;All Files (*)"
    );

    if (fileName.isEmpty()) return;

    QJsonObject root;
    root["format"] = "shape_editor";
    root["version"] = 1;
    root["nextId"] = Shape::s_nextId;
    root["pageWidth"] = m_pageWidth;
    root["pageHeight"] = m_pageHeight;
    root["viewScale"] = m_viewScale;
    root["viewOffsetX"] = m_viewOffset.x;
    root["viewOffsetY"] = m_viewOffset.y;

    QJsonArray shapesArray;
    for (int i = 0; i < m_shapeCount; ++i) {
        if (m_shapes[i]) {
            shapesArray.append(m_shapes[i]->save());
        }
    }
    root["shapes"] = shapesArray;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл для записи.");
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_statusLabel->setText("Файл успешно сохранён.");
}

void closeCurrentCustomShape() {
    if (!m_isPlacingCustomShape || m_customShapeMode != 1) return;

    if (m_tempAbsolutePoints.size() < 3) {
        QMessageBox::warning(this, "Ошибка", "Для замыкания нужно минимум 3 вершины.");
        return;
    }

    finishCustomShape();
}

bool appendFirstHorizontalSegment(double length) {
    if (m_tempAbsolutePoints.size() != 1) return false;
    if (length <= 1.0) return false;

    const Point& a = m_tempAbsolutePoints[0];
    Point b{a.x + length, a.y};

    m_tempAbsolutePoints.push_back(b);
    m_currentBuildDirection = 0.0; // вправо, параллельно основанию
    return true;
}



bool appendNextPointByLengthAndAngle(double length, double interiorAngleDeg) {
    if (m_tempAbsolutePoints.size() < 2) return false;
    if (length <= 1.0) return false;
    if (interiorAngleDeg <= 0.0 || interiorAngleDeg >= 180.0) return false;

    const Point& b = m_tempAbsolutePoints.back();

    const double interiorRad = interiorAngleDeg * M_PI / 180.0;

    // Поворот считается через угол между сторонами:
    // угол поворота = 180° - угол между сторонами
    const double turnRad = M_PI - interiorRad;

    // Строим всегда в одном направлении обхода
    m_currentBuildDirection += turnRad;

    Point c{
        b.x + length * std::cos(m_currentBuildDirection),
        b.y + length * std::sin(m_currentBuildDirection)
    };

    m_tempAbsolutePoints.push_back(c);
    return true;
}


void startCustomShapeBySegments() {
    m_isPlacingCustomShape = true;
    m_customShapeMode = 1;
    m_tempAbsolutePoints.clear();
    m_currentBuildDirection = 0.0;

    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;

    m_statusLabel->setText(
        "Кликни первую точку кастомной фигуры. Затем сразу будет запрошена длина первого отрезка."
    );
    m_canvas->update();
}


void loadFromFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Загрузить фигуры",
        "",
        "Shape Editor Files (*.json);;Text Files (*.txt);;All Files (*)"
    );

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось открыть файл.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Ошибка", "Файл повреждён или имеет неверный формат.");
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray shapesArray = root["shapes"].toArray();

    clearShapesArray();

    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;
    m_highlightVertexIndex = -1;
    m_highlightVertexShape = nullptr;
    m_highlightSideIndex = -1;
    m_highlightShape = nullptr;
    m_highlightAngleIndex = -1;
    m_highlightAngleShape = nullptr;

    m_pageWidth = root["pageWidth"].toDouble(5000.0);
    m_pageHeight = root["pageHeight"].toDouble(3000.0);
    m_viewScale = root["viewScale"].toDouble(1.0);
    m_viewOffset.x = root["viewOffsetX"].toDouble(120.0);
    m_viewOffset.y = root["viewOffsetY"].toDouble(80.0);

    int maxLoadedId = 0;

    for (const auto& v : shapesArray) {
        std::unique_ptr<Shape> shape = Shape::load(v.toObject());
        if (!shape) continue;

        maxLoadedId = std::max(maxLoadedId, shape->id);

        Shape* raw = shape.release();
        if (!addShapeToArray(raw)) {
            delete raw;
            QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
            break;
        }
    }

    int savedNextId = root["nextId"].toInt(maxLoadedId + 1);
    Shape::s_nextId = std::max(savedNextId, maxLoadedId + 1);

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();

    m_statusLabel->setText("Файл успешно загружен.");
}

void startCustomShapeByPoints() {
        m_isPlacingCustomShape = true;
        m_customShapeMode = 0;
        m_tempAbsolutePoints.clear();

        m_selectedShape = nullptr;
        m_selectedShapes.clear();
        m_propertiesShape = nullptr;

        m_statusLabel->setText("Кликай по листу, чтобы добавить точки кастомной фигуры. Esc – отмена.");
        m_canvas->update();
    }

    bool addShapeToArray(Shape* shape) {
    if (!shape) return false;
    if (m_shapeCount >= MAX_SHAPES) return false;

    m_shapes[m_shapeCount++] = shape;
    return true;
}

int indexOfShape(Shape* shape) const {
    if (!shape) return -1;

    for (int i = 0; i < m_shapeCount; ++i) {
        if (m_shapes[i] == shape) return i;
    }
    return -1;
}

bool removeShapeAt(int index, bool deleteObject = true) {
    if (index < 0 || index >= m_shapeCount) return false;

    if (deleteObject && m_shapes[index]) {
        delete m_shapes[index];
    }

    for (int i = index; i < m_shapeCount - 1; ++i) {
        m_shapes[i] = m_shapes[i + 1];
    }

    m_shapes[m_shapeCount - 1] = nullptr;
    --m_shapeCount;
    return true;
}

bool removeShapePointer(Shape* shape, bool deleteObject = true) {
    int idx = indexOfShape(shape);
    if (idx < 0) return false;
    return removeShapeAt(idx, deleteObject);
}

void clearShapesArray() {
    for (int i = 0; i < m_shapeCount; ++i) {
        delete m_shapes[i];
        m_shapes[i] = nullptr;
    }
    m_shapeCount = 0;
}

void insertShapeAtEndOrWarn(Shape* shape) {
    if (!addShapeToArray(shape)) {
        delete shape;
        QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
    }
}

    Shape* currentPropertiesShape() const {
    if (m_propertiesShape) {
        if (auto* group = dynamic_cast<GroupShape*>(m_selectedShape)) {
            for (const auto& child : group->children) {
                if (child.get() == m_propertiesShape) {
                    return m_propertiesShape;
                }
            }
        }
    }
    return m_selectedShape;
}

        std::vector<Shape*> allShapesSortedById() const {
    std::vector<Shape*> out;
    out.reserve(m_shapeCount);

    for (int i = 0; i < m_shapeCount; ++i) {
        if (m_shapes[i]) {
            out.push_back(m_shapes[i]);
        }
    }

    std::sort(out.begin(), out.end(), [](Shape* a, Shape* b) {
        return a->id < b->id;
    });

    return out;
}


        void rebuildAllShapesList() {
        if (!m_allShapesList) return;

        m_updatingUi = true;
        m_allShapesList->clear();

        auto shapes = allShapesSortedById();

        for (Shape* s : shapes) {
            QString text = QString("ID %1 – %2").arg(s->id).arg(s->kindName());
            auto* item = new QListWidgetItem(text);
            item->setData(Qt::UserRole, s->id);
            m_allShapesList->addItem(item);
        }

        if (m_selectedShape) {
            for (int i = 0; i < m_allShapesList->count(); ++i) {
                auto* item = m_allShapesList->item(i);
                if (!item) continue;

                if (item->data(Qt::UserRole).toInt() == m_selectedShape->id) {
                    m_allShapesList->setCurrentRow(i);
                    break;
                }
            }
        }

        m_updatingUi = false;
    }

   void showLeftSubPanel(bool showShapesList) {
    if (m_createPanelBox) {
        m_createPanelBox->setVisible(!showShapesList);
    }
    if (m_shapesListPanelBox) {
        m_shapesListPanelBox->setVisible(showShapesList);
    }

    if (showShapesList) {
        rebuildAllShapesList();
    }
}

    template <typename Fn>
QWidget* makeGroup(const QString& title, Fn fn) {
    auto* box = new QGroupBox(title);
    auto* l = new QVBoxLayout(box);
    l->setContentsMargins(6, 6, 6, 6);
    l->setSpacing(4);
    fn(l);
    return box;
}

    void setupSpinBox(QSpinBox* spin) {
        if (!spin) return;
        spin->setKeyboardTracking(false);
        spin->setAccelerated(true);
    }

    void setupDoubleSpinBox(QDoubleSpinBox* spin) {
        if (!spin) return;
        spin->setKeyboardTracking(false);
        spin->setAccelerated(true);
    }

    void syncSelectedGroupGeometry() {
        auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
        if (!group) return;

        bool currentShapeIsChild = false;
        if (m_propertiesShape) {
            for (const auto& child : group->children) {
                if (child.get() == m_propertiesShape) {
                    currentShapeIsChild = true;
                    break;
                }
            }
        }

        if (currentShapeIsChild) {
            group->setAnchorToBoundingBoxCenter();
        }
    }

        QWidget* createToolbar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(72);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    m_toggleLeftBtn = new QPushButton("Скрыть инструменты");
    connect(m_toggleLeftBtn, &QPushButton::clicked, this, [this]() {
        if (!m_leftPanel) return;
        const bool visible = m_leftPanel->isVisible();
        m_leftPanel->setVisible(!visible);
        m_toggleLeftBtn->setText(visible ? "Показать инструменты" : "Скрыть инструменты");
    });
    layout->addWidget(m_toggleLeftBtn);

    m_toggleShapesListBtn = new QPushButton("Список фигур");
    connect(m_toggleShapesListBtn, &QPushButton::clicked, this, [this]() {
        if (!m_shapesDock) return;
        bool visible = m_shapesDock->isVisible();
        m_shapesDock->setVisible(!visible);
        if (!visible) rebuildAllShapesList();
    });
    layout->addWidget(m_toggleShapesListBtn);

    layout->addStretch();

    m_togglePropertiesBtn = new QPushButton("Скрыть свойства");
    connect(m_togglePropertiesBtn, &QPushButton::clicked, this, [this]() {
        const bool visible = m_propertiesPanel->isVisible();
        m_propertiesPanel->setVisible(!visible);
        m_togglePropertiesBtn->setText(visible ? "Показать свойства" : "Скрыть свойства");
    });
    layout->addWidget(m_togglePropertiesBtn);

    return bar;
}

 QWidget* createLeftPanel() {
    m_leftPanel = new QWidget;
    m_leftPanel->setMinimumWidth(300);
    m_leftPanel->setMaximumWidth(300);

    auto* outer = new QVBoxLayout(m_leftPanel);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(8);

    m_createPanelBox = new QWidget;
    auto* createLayout = new QVBoxLayout(m_createPanelBox);
    createLayout->setContentsMargins(0, 0, 0, 0);
    createLayout->setSpacing(6);

    createLayout->addWidget(new QLabel("Инструменты"));

    auto* addCircleBtn = new QPushButton("Окружность");
connect(addCircleBtn, &QPushButton::clicked, this, [this]() {
    createShapeAndSelect(createDefaultShape(new CircleShape()));
});
createLayout->addWidget(addCircleBtn);

auto* addRectBtn = new QPushButton("Прямоугольник");
connect(addRectBtn, &QPushButton::clicked, this, [this]() {
    createShapeAndSelect(createDefaultShape(new RectangleShape()));
});
createLayout->addWidget(addRectBtn);

auto* addTriangleBtn = new QPushButton("Треугольник");
connect(addTriangleBtn, &QPushButton::clicked, this, [this]() {
    createShapeAndSelect(createDefaultShape(new TriangleShape()));
});
createLayout->addWidget(addTriangleBtn);

auto* addHexBtn = new QPushButton("Шестиугольник");
connect(addHexBtn, &QPushButton::clicked, this, [this]() {
    createShapeAndSelect(createDefaultShape(new HexagonShape()));
});
createLayout->addWidget(addHexBtn);

auto* addTrapBtn = new QPushButton("Трапецию");
connect(addTrapBtn, &QPushButton::clicked, this, [this]() {
    createShapeAndSelect(createDefaultShape(new TrapezoidShape()));
});
createLayout->addWidget(addTrapBtn);

    auto* addCustomBySegmentsBtn = new QPushButton("Кастомная фигура");
connect(addCustomBySegmentsBtn, &QPushButton::clicked, this, [this]() {
    startCustomShapeBySegments();
});
createLayout->addWidget(addCustomBySegmentsBtn);

m_finishCustomBtn = new QPushButton("Замкнуть фигуру");
m_finishCustomBtn->setVisible(false);
connect(m_finishCustomBtn, &QPushButton::clicked, this, [this]() {
    closeCurrentCustomShape();
});
createLayout->addWidget(m_finishCustomBtn);

    auto* addRegularPolygonBtn = new QPushButton("Правильный многоугольник");
connect(addRegularPolygonBtn, &QPushButton::clicked, this, [this]() {
    bool ok = false;
    int sides = QInputDialog::getInt(
        this,
        "Правильный многоугольник",
        "Введите количество сторон:",
        5,
        3,
        20,
        1,
        &ok
    );

    if (!ok) return;

    Shape* poly = createRegularPolygonShape(sides);
    if (!poly) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать многоугольник.");
        return;
    }

    createShapeAndSelect(poly);
});
createLayout->addWidget(addRegularPolygonBtn);

    auto* groupBtn = new QPushButton("Составная фигура");
    connect(groupBtn, &QPushButton::clicked, this, [this]() {
        groupSelected();
    });
    createLayout->addWidget(groupBtn);

    auto* addToGroupBtn = new QPushButton("Добавить в составную");
connect(addToGroupBtn, &QPushButton::clicked, this, [this]() {
    addSelectedToGroup();
});
createLayout->addWidget(addToGroupBtn);

    auto* ungroupBtn = new QPushButton("Разгруппировать");
    connect(ungroupBtn, &QPushButton::clicked, this, [this]() {
        ungroupSelected();
    });
    createLayout->addWidget(ungroupBtn);

    createLayout->addStretch();

    outer->addWidget(m_createPanelBox);
    outer->addStretch();

    return m_leftPanel;
}

QWidget* createShapesListPanel() {
    m_shapesDock = new QWidget;
    m_shapesDock->setMinimumWidth(250);
    m_shapesDock->setMaximumWidth(250);

    auto* outer = new QVBoxLayout(m_shapesDock);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    outer->addWidget(new QLabel("Список фигур"));

    m_allShapesList = new QListWidget;
    m_allShapesList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_allShapesList->setMinimumHeight(220);
    m_allShapesList->setMaximumHeight(420);
    outer->addWidget(m_allShapesList);

    connect(m_allShapesList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_updatingUi) return;

        auto shapes = allShapesSortedById();
        if (row < 0 || row >= (int)shapes.size()) return;

        m_selectedShape = shapes[row];
        m_selectedShapes.clear();
        m_selectedShapes.push_back(m_selectedShape);
        m_propertiesShape = nullptr;

        updatePropertiesPanel();
        m_canvas->update();
    });

    outer->addStretch();
    return m_shapesDock;
}

    QWidget* createPropertiesPanel() {
        m_propertiesPanel = new QWidget;
        m_propertiesPanel->setMinimumWidth(650);
        m_propertiesPanel->setMaximumWidth(650);

        auto* outer = new QVBoxLayout(m_propertiesPanel);
        outer->setContentsMargins(8, 8, 8, 8);
        outer->setSpacing(8);

        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);

        auto* bodyWidget = new QWidget;
        auto* body = new QVBoxLayout(bodyWidget);

        body->addWidget(makeGroup("Границы", [this](QVBoxLayout* l) {
            m_boundsLabel = new QLabel("Фигура не выбрана");
            m_boundsLabel->setWordWrap(true);
            l->addWidget(m_boundsLabel);
        }));

        body->addWidget(makeGroup("Точка привязки", [this](QVBoxLayout* l) {
            auto* row1 = new QHBoxLayout;
            row1->addWidget(new QLabel("Абс. X:"));

            m_anchorX = new QSpinBox;
            m_anchorX->setRange(-5000, 5000);
            setupSpinBox(m_anchorX);
            row1->addWidget(m_anchorX);

            row1->addWidget(new QLabel("Абс. Y:"));

            m_anchorY = new QSpinBox;
            m_anchorY->setRange(-5000, 5000);
            setupSpinBox(m_anchorY);
            row1->addWidget(m_anchorY);

            m_applyAnchorBtn = new QPushButton("Применить");
            m_applyAnchorBtn->setVisible(false);
            row1->addWidget(m_applyAnchorBtn);
            l->addLayout(row1);

            auto applyAbsAnchor = [this]() {
                if (m_updatingUi) return;
                Shape* target = currentPropertiesShape();
                if (!target) return;

                Point newAnchor(m_anchorX->value(), m_anchorY->value());
                Point delta = newAnchor - target->anchor;

                if (auto* group = dynamic_cast<GroupShape*>(target)) {
                    group->moveBy(delta);
                } else {
                    target->anchor = newAnchor;
                }

                syncSelectedGroupGeometry();
                m_canvas->update();
                updatePropertiesPanel();
            };

            connect(m_anchorX, &QSpinBox::editingFinished, this, applyAbsAnchor);
            connect(m_anchorY, &QSpinBox::editingFinished, this, applyAbsAnchor);
            connect(m_applyAnchorBtn, &QPushButton::clicked, this, applyAbsAnchor);

            auto* row2 = new QHBoxLayout;
            row2->addWidget(new QLabel("Лок. X:"));

            m_anchorInsideX = new QSpinBox;
            m_anchorInsideX->setRange(-5000, 5000);
            setupSpinBox(m_anchorInsideX);
            row2->addWidget(m_anchorInsideX);

            row2->addWidget(new QLabel("Лок. Y:"));

            m_anchorInsideY = new QSpinBox;
            m_anchorInsideY->setRange(-5000, 5000);
            setupSpinBox(m_anchorInsideY);
            row2->addWidget(m_anchorInsideY);

            m_applyAnchorInsideBtn = new QPushButton("Перенести");
            m_applyAnchorInsideBtn->setVisible(false);
            row2->addWidget(m_applyAnchorInsideBtn);
            l->addLayout(row2);

            auto applyInnerAnchor = [this]() {
                if (m_updatingUi) return;
                Shape* target = currentPropertiesShape();
                if (!target) return;

                Point p(m_anchorInsideX->value(), m_anchorInsideY->value());
                target->setAnchorInside(p);

                syncSelectedGroupGeometry();
                m_canvas->update();
                updatePropertiesPanel();
            };

            connect(m_anchorInsideX, &QSpinBox::editingFinished, this, applyInnerAnchor);
            connect(m_anchorInsideY, &QSpinBox::editingFinished, this, applyInnerAnchor);
            connect(m_applyAnchorInsideBtn, &QPushButton::clicked, this, applyInnerAnchor);
        }));

        body->addWidget(makeGroup("Масштаб", [this](QVBoxLayout* l) {
            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel("Коэффициент:"));

            m_scaleSpin = new QDoubleSpinBox;
            m_scaleSpin->setRange(1.0, 5000.0);
m_scaleSpin->setSingleStep(5.0);
m_scaleSpin->setDecimals(0);
m_scaleSpin->setSuffix("%");
m_scaleSpin->setValue(100.0);
            setupDoubleSpinBox(m_scaleSpin);
            row->addWidget(m_scaleSpin);

            m_applyScaleBtn = new QPushButton("Масштабировать");
            m_applyScaleBtn->setVisible(false);
            row->addWidget(m_applyScaleBtn);

            row->addStretch();
            l->addLayout(row);

            auto applyScale = [this]() {
    if (m_updatingUi) return;
    Shape* target = currentPropertiesShape();
    if (!target) return;

    const double newPercent = m_scaleSpin->value();
    if (newPercent <= 0.0) return;

    const double factor = newPercent / target->currentScale;
    if (std::abs(factor - 1.0) < 1e-9) return;

    target->scaleUniform(factor);

    syncSelectedGroupGeometry();
    m_canvas->update();
    updatePropertiesPanel();
};

            connect(m_scaleSpin, &QDoubleSpinBox::editingFinished, this, applyScale);
            connect(m_applyScaleBtn, &QPushButton::clicked, this, applyScale);
        }));

  body->addWidget(makeGroup("Заливка", [this](QVBoxLayout* l) {
    m_noFillCheck = new QCheckBox("Без заливки");
    l->addWidget(m_noFillCheck);

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel("Цвет:"));

    m_fillButton = new QPushButton("Прозрачный");
    row->addWidget(m_fillButton);

    row->addStretch();
    l->addLayout(row);

    connect(m_fillButton, &QPushButton::clicked, this, [this]() {
        Shape* target = currentPropertiesShape();
        if (!target) return;

        QColorDialog dlg(m_pendingFill == Qt::transparent ? Qt::white : m_pendingFill, this);
        dlg.setOption(QColorDialog::DontUseNativeDialog, true);
        if (dlg.exec() != QDialog::Accepted) return;

        QColor c = dlg.selectedColor();
        if (!c.isValid()) return;

        m_pendingFill = c;
        if (!m_noFillCheck->isChecked()) {
            target->fillColor = c;
        }

        updateFillButton();
        m_canvas->update();
    });

    connect(m_noFillCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_updatingUi) return;
        Shape* target = currentPropertiesShape();
        if (!target) return;

        target->fillColor = checked ? QColor(Qt::transparent) : m_pendingFill;
        updateFillButton();
        m_canvas->update();
    });
}));
    
        body->addWidget(makeGroup("Вершины", [this](QVBoxLayout* l) {
            m_verticesTable = new QTableWidget;
            m_verticesTable->setColumnCount(3);
            m_verticesTable->setHorizontalHeaderLabels({"№", "X", "Y"});
            m_verticesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            m_verticesTable->verticalHeader()->setVisible(false);
            m_verticesTable->setMinimumHeight(200);
            l->addWidget(m_verticesTable);

            m_applyVerticesBtn = new QPushButton("Применить вершины");
            m_applyVerticesBtn->setVisible(true);
            l->addWidget(m_applyVerticesBtn);

            connect(m_verticesTable, &QTableWidget::currentCellChanged,
                    this,
                    [this](int currentRow, int, int, int) {
                        auto* poly = dynamic_cast<PolygonShape*>(currentPropertiesShape());
                        if (!poly || !poly->supportsVerticesEditing()) {
                            m_highlightVertexIndex = -1;
                            m_highlightVertexShape = nullptr;
                            m_canvas->update();
                            return;
                        }

                        if (currentRow >= 0 && currentRow < (int)poly->verticesAbs().size()) {
                            m_highlightVertexIndex = currentRow;
                            m_highlightVertexShape = poly;
                        } else {
                            m_highlightVertexIndex = -1;
                            m_highlightVertexShape = nullptr;
                        }

                        m_canvas->update();
                    });

            connect(m_applyVerticesBtn, &QPushButton::clicked, this, [this]() {
                if (m_updatingUi) return;

                auto* poly = dynamic_cast<PolygonShape*>(currentPropertiesShape());
                if (!poly || !poly->supportsVerticesEditing()) return;

                std::vector<Point> rel;
                int rows = m_verticesTable->rowCount();

                for (int r = 0; r < rows; ++r) {
                    auto* xItem = m_verticesTable->item(r, 1);
                    auto* yItem = m_verticesTable->item(r, 2);

                    if (!xItem || !yItem) {
                        QMessageBox::warning(this, "Ошибка", "Не удалось прочитать координаты вершины.");
                        return;
                    }

                    bool okX = false;
                    bool okY = false;
                    double x = xItem->text().toDouble(&okX);
                    double y = yItem->text().toDouble(&okY);

                    if (!okX || !okY) {
                        QMessageBox::warning(this, "Ошибка", "Координаты вершин должны быть числами.");
                        return;
                    }

                    rel.push_back({x, y});
                }

                if (rel.size() < 3) {
                    QMessageBox::warning(this, "Ошибка", "У фигуры должно быть минимум 3 вершины.");
                    return;
                }

                poly->setVerticesRel(rel);

                int row = m_verticesTable->currentRow();
                if (row >= 0 && row < (int)rel.size()) {
                    m_highlightVertexIndex = row;
                    m_highlightVertexShape = poly;
                } else {
                    m_highlightVertexIndex = -1;
                    m_highlightVertexShape = nullptr;
                }

                syncSelectedGroupGeometry();
                m_canvas->update();
                updatePropertiesPanel();
            });
        }));

        body->addWidget(makeGroup("Стороны", [this](QVBoxLayout* l) {
            m_sidesWidget = new QWidget;
            m_sidesLayout = new QVBoxLayout(m_sidesWidget);
            l->addWidget(m_sidesWidget);
        }));

        m_groupChildrenBox = new QGroupBox("Фигуры в составе");
        {
            auto* l = new QVBoxLayout(m_groupChildrenBox);

            m_groupChildrenList = new QListWidget;
            m_groupChildrenList->setSelectionMode(QAbstractItemView::SingleSelection);
            m_groupChildrenList->setMinimumHeight(140);
            connect(m_groupChildrenList, &QListWidget::currentRowChanged, this, [this](int row) {
                if (m_updatingUi) return;

                auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
                if (!group) {
                    m_propertiesShape = nullptr;
                    m_canvas->update();
                    return;
                }

                if (row >= 0 && row < (int)group->children.size()) {
                    m_propertiesShape = group->children[row].get();
                } else {
                    m_propertiesShape = nullptr;
                }

                updatePropertiesPanel();
                m_canvas->update();
            });
            l->addWidget(m_groupChildrenList);

            m_removeChildBtn = new QPushButton("Удалить выбранную фигуру из состава");
            connect(m_removeChildBtn, &QPushButton::clicked, this, [this]() { removeSelectedChildFromGroup(); });
            l->addWidget(m_removeChildBtn);
        }
        m_groupChildrenBox->setVisible(false);
        body->addWidget(m_groupChildrenBox);

        body->addStretch();
        scroll->setWidget(bodyWidget);
        outer->addWidget(scroll);

        return m_propertiesPanel;
    }

void setupUi() {
    auto* central = new QWidget;
    setCentralWidget(central);

    QMenu* fileMenu = menuBar()->addMenu("Файл");

QAction* saveAction = fileMenu->addAction("Сохранить");
QAction* loadAction = fileMenu->addAction("Загрузить");

connect(saveAction, &QAction::triggered, this, [this]() {
    saveToFile();
});

connect(loadAction, &QAction::triggered, this, [this]() {
    loadFromFile();
});

auto* closeCornerBtn = new QToolButton(this);
closeCornerBtn->setText("✕");
closeCornerBtn->setFixedSize(28, 28);
closeCornerBtn->setStyleSheet(R"(
    QToolButton {
        background: #e74c3c;
        color: white;
        border: none;
        border-radius: 14px;
        font-weight: bold;
    }
    QToolButton:hover { background: #ff5b4d; }
    QToolButton:pressed { background: #c0392b; }
)");
connect(closeCornerBtn, &QToolButton::clicked, this, &QWidget::close);
menuBar()->setCornerWidget(closeCornerBtn, Qt::TopRightCorner);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(createToolbar());

    auto* content = new QWidget;
    auto* contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_canvas = new QWidget;
    m_canvas->setMouseTracking(true);
    m_canvas->installEventFilter(this);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

   contentLayout->addWidget(createLeftPanel(), 0);
contentLayout->addWidget(createShapesListPanel(), 0);
contentLayout->addWidget(m_canvas, 1);
contentLayout->addWidget(createPropertiesPanel(), 0);

    root->addWidget(content, 1);

    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);
    m_statusLabel = new QLabel("Выделение: Shift + клик. Любая клавиша показывает панель свойств.");
    m_statusBar->addWidget(m_statusLabel, 1);

    rebuildAllShapesList();
    m_leftPanel->hide();
m_shapesDock->hide();
m_propertiesPanel->hide();

m_toggleLeftBtn->setText("Показать инструменты");
m_togglePropertiesBtn->setText("Показать свойства");
}

    void applyLightTheme() {
        setStyleSheet(R"(
            QMainWindow, QWidget {
                background: #f6fbff;
                color: #16324d;
                font-size: 18px;
            }
            QPushButton {
                background: #d9efff;
                color: #123;
                border: 1px solid #9bc9ee;
                border-radius: 8px;
                padding: 8px 12px;
            }
            QPushButton:hover { background: #c7e6ff; }
            QPushButton:pressed { background: #b6dcff; }
            QGroupBox {
                font-weight: bold;
                border: 1px solid #c6def2;
                border-radius: 10px;
                margin-top: 10px;
                padding-top: 8px;
                background: #fbfeff;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QScrollArea { border: none; }
            QPlainTextEdit, QLineEdit, QSpinBox, QDoubleSpinBox, QTableWidget, QListWidget {
                background: white;
                border: 1px solid #b8d5ea;
                border-radius: 6px;
            }
            QCheckBox { spacing: 8px; }
            QStatusBar {
                background: #eef7ff;
                border-top: 1px solid #c6def2;
            }
            QHeaderView::section {
                background: #eaf6ff;
                border: 1px solid #c6def2;
                padding: 4px;
            }
            QTableWidget {
                gridline-color: #d7e8f5;
                alternate-background-color: #f8fcff;
            }
        )");
    }


   void createInitialShapes() {
    insertShapeAtEndOrWarn(createStyledShape(new CircleShape()));
    insertShapeAtEndOrWarn(createStyledShape(new RectangleShape()));
    insertShapeAtEndOrWarn(createStyledShape(new TriangleShape()));
    insertShapeAtEndOrWarn(createStyledShape(new HexagonShape()));
    insertShapeAtEndOrWarn(createStyledShape(new TrapezoidShape()));

    for (int i = 0; i < m_shapeCount; ++i) {
        if (m_shapes[i]) {
            Rectangle b = m_shapes[i]->boundingBox();
            double shapeW = b.width;
            double shapeH = b.height;

            m_shapes[i]->anchor = {
                250.0 + i * 320.0,
                m_pageHeight * 0.5 - shapeH * 0.5
            };
        }
    }

    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

Point mouseToScene(const QPoint& pos) const {
        return {
            (double(pos.x()) - m_viewOffset.x) / m_viewScale,
            (double(m_canvas->height() - pos.y()) - m_viewOffset.y) / m_viewScale
        };
    }

    Shape* hitTest(const Point& p) {
    for (int i = m_shapeCount - 1; i >= 0; --i) {
        if (m_shapes[i] && m_shapes[i]->contains(p)) {
            return m_shapes[i];
        }
    }
    return nullptr;
}

       int hitResizeHandle(const Point& p, const Rectangle& b) const {
        std::vector<Point> handles = {
            {b.x, b.y + b.height},
            {b.x + b.width, b.y + b.height},
            {b.x + b.width, b.y},
            {b.x, b.y}
        };

        double tolerance = 12.0 / m_viewScale;

        for (int i = 0; i < (int)handles.size(); ++i) {
            if (distancePts(p, handles[i]) <= tolerance) return i;
        }
        return -1;
    }

   void createShapeAndSelect(Shape* shape) {
    if (!shape) return;

    Rectangle b = shape->boundingBox();
    double shapeW = b.width;
    double shapeH = b.height;

    shape->anchor = {
        m_pageWidth * 0.5 - shapeW * 0.5,
        m_pageHeight * 0.5 - shapeH * 0.5
    };

    shape->ensureStyleArrays();

    if (!addShapeToArray(shape)) {
        delete shape;
        QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
        return;
    }

    m_selectedShape = shape;
    m_selectedShapes = {shape};
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

Shape* createStyledShape(Shape* shape) {
    if (!shape) return nullptr;

    shape->ensureStyleArrays();

    static std::vector<QColor> palette = {
        QColor("#e85d75"),
        QColor("#4d96ff"),
        QColor("#2bb673"),
        QColor("#f4a261"),
        QColor("#9b5de5"),
        QColor("#ef476f"),
        QColor("#118ab2")
    };

    int seed = shape->id % (int)palette.size();

    shape->fillColor = palette[seed].lighter(160);

    for (int i = 0; i < shape->sideCount(); ++i) {
        shape->sideColors[i] = palette[(seed + i) % palette.size()];
        shape->sideWidths[i] = 2.0 + ((shape->id + i) % 4) * 2.0; // 2,4,6,8
    }

    return shape;
}

Shape* createDefaultShape(Shape* shape) {
    if (!shape) return nullptr;

    shape->ensureStyleArrays();

    shape->fillColor = Qt::transparent;

    for (int i = 0; i < shape->sideCount(); ++i) {
        shape->sideColors[i] = Qt::black;
        shape->sideWidths[i] = 3.0;
    }

    return shape;
}

Shape* createRegularPolygonShape(int sides) {
    if (sides < 3) return nullptr;

    auto* poly = new CustomPolygonShape();

    const double radius = 120.0;
    const double angleStep = 2.0 * M_PI / sides;
    const double startAngle = M_PI / 2.0;

    std::vector<Point> rel;
    rel.reserve(sides);

    for (int i = 0; i < sides; ++i) {
        double angle = startAngle + i * angleStep;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        rel.push_back({x, y});
    }

    poly->setVerticesRel(rel);
    createDefaultShape(poly);

    return poly;
}

void cancelCustomMode() {
    m_isPlacingCustomShape = false;
    m_customShapeMode = 0;
    m_tempAbsolutePoints.clear();
    m_currentBuildDirection = 0.0;
    if (m_finishCustomBtn) m_finishCustomBtn->setVisible(false);
    m_statusLabel->setText("Выделение: Shift + клик. Любая клавиша показывает панель свойств.");
    m_canvas->update();
}

void finishCustomShape() {
    if (m_tempAbsolutePoints.size() < 3) {
        QMessageBox::warning(this, "Ошибка", "Нужно минимум 3 точки.");
        return;
    }

    std::vector<Point> finalPoints = m_tempAbsolutePoints;

    if (finalPoints.size() >= 2 && distancePts(finalPoints.front(), finalPoints.back()) < 1e-9) {
        finalPoints.pop_back();
    }

    if (finalPoints.size() < 3) {
        QMessageBox::warning(this, "Ошибка", "Нужно минимум 3 разные точки.");
        return;
    }

    auto* poly = new CustomPolygonShape();
    Point first = finalPoints.front();
    poly->anchor = first;

    std::vector<Point> rel;
    rel.reserve(finalPoints.size());
    for (const auto& p : finalPoints) {
        rel.push_back(p - first);
    }

    poly->setVerticesRel(rel);
    poly->ensureStyleArrays();
    poly->fillColor = Qt::transparent;

    for (int i = 0; i < poly->sideCount(); ++i) {
        poly->sideColors[i] = Qt::black;
        poly->sideWidths[i] = 3.0;
    }

    if (!addShapeToArray(poly)) {
        delete poly;
        QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
        return;
    }

    m_selectedShape = poly;
    m_selectedShapes = {m_selectedShape};
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    cancelCustomMode();
    updatePropertiesPanel();
    m_statusLabel->setText("Кастомная фигура создана.");
}
void paintCanvas() {
        QPainter painter(m_canvas);
        painter.setRenderHint(QPainter::Antialiasing);

        // Серое поле вокруг листа
        painter.fillRect(m_canvas->rect(), QColor(170, 170, 170));

        // Белый лист в экранных координатах
        QRectF pageScreen(
            m_viewOffset.x,
            m_canvas->height() - (m_viewOffset.y + m_pageHeight * m_viewScale),
            m_pageWidth * m_viewScale,
            m_pageHeight * m_viewScale
        );

        // Тень листа
        QRectF shadowRect = pageScreen.translated(6, 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 35));
        painter.drawRect(shadowRect);

        // Сам лист
        painter.setPen(QPen(QColor(140, 140, 140), 1));
        painter.setBrush(m_pageColor);
        painter.drawRect(pageScreen);

        // Переход в мировые координаты
        painter.translate(0, m_canvas->height());
        painter.scale(1, -1);
        painter.translate(m_viewOffset.x, m_viewOffset.y);
        painter.scale(m_viewScale, m_viewScale);

        // Обрезаем рисование по листу
        painter.setClipRect(QRectF(0.0, 0.0, m_pageWidth, m_pageHeight));

        for (int i = 0; i < m_shapeCount; ++i) {
    if (m_shapes[i]) {
        m_shapes[i]->draw(painter);
    }
}

        for (auto* s : m_selectedShapes) {
            Rectangle b = s->boundingBox();

            QPen pen(QColor(56, 132, 201), 0, Qt::DashLine);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(b.x, b.y, b.width, b.height));

            std::vector<QPointF> handles = {
                QPointF(b.x, b.y + b.height),
                QPointF(b.x + b.width, b.y + b.height),
                QPointF(b.x + b.width, b.y),
                QPointF(b.x, b.y)
            };

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(56, 132, 201));

            const double handleSize = 8.0 / m_viewScale;
            for (const auto& h : handles) {
                painter.drawRect(QRectF(h.x() - handleSize / 2.0,
                                        h.y() - handleSize / 2.0,
                                        handleSize,
                                        handleSize));
            }
        }

        if (auto* group = dynamic_cast<GroupShape*>(m_selectedShape)) {
            Q_UNUSED(group);

            if (m_propertiesShape) {
                bool isChildOfSelectedGroup = false;

                auto* selectedGroup = dynamic_cast<GroupShape*>(m_selectedShape);
                if (selectedGroup) {
                    for (const auto& child : selectedGroup->children) {
                        if (child.get() == m_propertiesShape) {
                            isChildOfSelectedGroup = true;
                            break;
                        }
                    }
                }

                if (isChildOfSelectedGroup) {
                    Rectangle b = m_propertiesShape->boundingBox();

                    QPen childPen(QColor(255, 140, 0), 0, Qt::DashLine);
                    childPen.setCosmetic(true);
                    painter.setPen(childPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(b.x, b.y, b.width, b.height));

                    std::vector<Point> verts = m_propertiesShape->verticesAbs();
                    if (!verts.empty() && m_propertiesShape->supportsVerticesEditing()) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(QColor(255, 140, 0));
                        double r = 4.0 / m_viewScale;
                        for (const auto& v : verts) {
                            painter.drawEllipse(QPointF(v.x, v.y), r, r);
                        }
                    }
                }
            }
        }

        Shape* target = currentPropertiesShape();
        if (target) {
            QPen crossPen(QColor(220, 40, 40), 0);
            crossPen.setCosmetic(true);
            painter.setPen(crossPen);
            painter.setBrush(Qt::NoBrush);

            const double crossHalf = 6.0 / m_viewScale;
            painter.drawLine(QPointF(target->anchor.x - crossHalf, target->anchor.y),
                             QPointF(target->anchor.x + crossHalf, target->anchor.y));
            painter.drawLine(QPointF(target->anchor.x, target->anchor.y - crossHalf),
                             QPointF(target->anchor.x, target->anchor.y + crossHalf));
        }

        if (auto* poly = dynamic_cast<PolygonShape*>(m_highlightShape)) {
            auto abs = poly->verticesAbs();
            if (m_highlightSideIndex >= 0 && m_highlightSideIndex < (int)abs.size()) {
                int next = (m_highlightSideIndex + 1) % (int)abs.size();
                QPen hp(QColor(255, 120, 0), 0);
                hp.setCosmetic(true);
                painter.setPen(hp);
                painter.drawLine(QPointF(abs[m_highlightSideIndex].x, abs[m_highlightSideIndex].y),
                                 QPointF(abs[next].x, abs[next].y));
            }
        }

        if (auto* poly = dynamic_cast<PolygonShape*>(m_highlightAngleShape)) {
            auto abs = poly->verticesAbs();
            if (m_highlightAngleIndex >= 0 && m_highlightAngleIndex < (int)abs.size()) {
                const int n = (int)abs.size();
                const Point prev = abs[(m_highlightAngleIndex - 1 + n) % n];
                const Point curr = abs[m_highlightAngleIndex];
                const Point next = abs[(m_highlightAngleIndex + 1) % n];

                auto normalize = [](double x, double y) -> Point {
                    double len = std::sqrt(x * x + y * y);
                    if (len < 1e-9) return {0.0, 0.0};
                    return {x / len, y / len};
                };

                Point v1 = normalize(prev.x - curr.x, prev.y - curr.y);
                Point v2 = normalize(next.x - curr.x, next.y - curr.y);

                const double rayLen = 28.0 / m_viewScale;
                Point p1 = {curr.x + v1.x * rayLen, curr.y + v1.y * rayLen};
                Point p2 = {curr.x + v2.x * rayLen, curr.y + v2.y * rayLen};

                QPen pen(QColor(255, 120, 0), 0);
                pen.setCosmetic(true);
                painter.setPen(pen);
                painter.setBrush(Qt::NoBrush);
                painter.drawLine(QPointF(curr.x, curr.y), QPointF(p1.x, p1.y));
                painter.drawLine(QPointF(curr.x, curr.y), QPointF(p2.x, p2.y));

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 160, 60));
                double r = 6.0 / m_viewScale;
                painter.drawEllipse(QPointF(curr.x, curr.y), r, r);
            }
        }

        if (auto* poly = dynamic_cast<PolygonShape*>(m_highlightVertexShape)) {
            auto abs = poly->verticesAbs();
            if (m_highlightVertexIndex >= 0 && m_highlightVertexIndex < (int)abs.size()) {
                const Point& v = abs[m_highlightVertexIndex];

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(255, 80, 80));
                painter.drawEllipse(QPointF(v.x, v.y), 7.0 / m_viewScale, 7.0 / m_viewScale);

                QPen pen(QColor(255, 80, 80), 0);
                pen.setCosmetic(true);
                painter.setPen(pen);
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QPointF(v.x, v.y), 11.0 / m_viewScale, 11.0 / m_viewScale);
            }
        }

        if (m_isPlacingCustomShape && !m_tempAbsolutePoints.empty()) {
            QPen pen(QColor(56, 132, 201), 0, Qt::DashLine);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(QColor(56, 132, 201));

            double r = 4.5 / m_viewScale;
            for (size_t i = 0; i < m_tempAbsolutePoints.size(); ++i) {
                const auto& p = m_tempAbsolutePoints[i];
                painter.drawEllipse(QPointF(p.x, p.y), r, r);

                if (i + 1 < m_tempAbsolutePoints.size()) {
                    painter.drawLine(QPointF(m_tempAbsolutePoints[i].x, m_tempAbsolutePoints[i].y),
                                     QPointF(m_tempAbsolutePoints[i + 1].x, m_tempAbsolutePoints[i + 1].y));
                }
            }
        }

        painter.resetTransform();

    }
void onMousePress(QMouseEvent* e) {
    Point p = mouseToScene(e->pos());

    if (e->button() == Qt::MiddleButton) {
        m_panningCanvas = true;
        m_panStartMouse = {double(e->pos().x()), double(e->pos().y())};
        return;
    }

    if (e->button() != Qt::LeftButton) return;

    if (m_isPlacingCustomShape) {
        if (m_customShapeMode == 0) {
            m_tempAbsolutePoints.push_back(p);
            m_statusLabel->setText(QString("Точек: %1. Двойной клик – завершить, Esc – отмена.")
                .arg((int)m_tempAbsolutePoints.size()));
            m_canvas->update();
            return;
        }

        if (m_customShapeMode == 1) {
            if (m_tempAbsolutePoints.empty()) {
                m_tempAbsolutePoints.push_back(p);
                m_finishCustomBtn->setVisible(true);

                bool ok = false;
                double length = QInputDialog::getDouble(
                    this,
                    "Первый отрезок",
                    "Введите длину первого отрезка:",
                    100.0,
                    1.0,
                    5000.0,
                    0,
                    &ok
                );

                if (!ok) {
                    cancelCustomMode();
                    return;
                }

                if (!appendFirstHorizontalSegment(length)) {
                    QMessageBox::warning(this, "Ошибка", "Не удалось построить первый отрезок.");
                    cancelCustomMode();
                    return;
                }

                m_statusLabel->setText(
                    "Первый отрезок построен. Теперь кликай для добавления следующей стороны или нажми 'Замкнуть фигуру'."
                );
                m_canvas->update();
                return;
            }

            bool okLen = false;
            double length = QInputDialog::getDouble(
                this,
                "Следующий отрезок",
                "Введите длину следующего отрезка:",
                100.0,
                1.0,
                5000.0,
                0,
                &okLen
            );
            if (!okLen) return;

            bool okAngle = false;
            double angle = QInputDialog::getDouble(
                this,
                "Угол между сторонами",
                "Введите угол между предыдущей и новой стороной:",
                120.0,
                1.0,
                179.0,
                0,
                &okAngle
            );
            if (!okAngle) return;

            if (!appendNextPointByLengthAndAngle(length, angle)) {
                QMessageBox::warning(this, "Ошибка", "Не удалось построить следующий отрезок.");
                return;
            }

            m_statusLabel->setText(
                QString("Вершин: %1. Кликни для добавления ещё одной стороны или нажми 'Замкнуть фигуру'.")
                    .arg((int)m_tempAbsolutePoints.size())
            );
            m_canvas->update();
            return;
        }
    }

    bool shift = e->modifiers() & Qt::ShiftModifier;
    Shape* clicked = hitTest(p);

    if (!shift && m_selectedShape) {
        Rectangle b = m_selectedShape->boundingBox();
        int handle = hitResizeHandle(p, b);
        if (handle != -1) {
            m_resizing = true;
            m_resizeHandleIndex = handle;
            m_resizeStartBounds = b;
            m_resizeStartMouse = p;
            return;
        }

        Shape* target = currentPropertiesShape();
        if (target && target->isPointNearAnchor(p, 14.0 / m_viewScale)) {
            m_dragging = true;
            m_dragAnchor = true;
            m_lastMouse = p;
            return;
        }
    }

    if (shift) {
        if (clicked) {
            auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clicked);
            if (it == m_selectedShapes.end()) {
                m_selectedShapes.push_back(clicked);
            } else {
                m_selectedShapes.erase(it);
            }

            m_selectedShape = m_selectedShapes.empty() ? nullptr : m_selectedShapes.back();
            m_propertiesShape = nullptr;

            rebuildAllShapesList();
            updatePropertiesPanel();
            m_canvas->update();
        }
        return;
    }

    if (clicked) {
        m_selectedShapes = {clicked};
        m_selectedShape = clicked;
        m_propertiesShape = nullptr;
        m_dragging = true;
        m_dragAnchor = false;
        m_lastMouse = p;

        rebuildAllShapesList();
        updatePropertiesPanel();
        m_canvas->update();
    } else {
        m_selectedShapes.clear();
        m_selectedShape = nullptr;
        m_propertiesShape = nullptr;
        m_highlightSideIndex = -1;
        m_highlightShape = nullptr;
        m_highlightAngleIndex = -1;
        m_highlightAngleShape = nullptr;
        m_highlightVertexIndex = -1;
        m_highlightVertexShape = nullptr;

        rebuildAllShapesList();
        updatePropertiesPanel();
        m_canvas->update();
    }
}


   void onMouseDoubleClick(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (!m_isPlacingCustomShape) return;

    if (m_customShapeMode == 0) {
        if (m_tempAbsolutePoints.size() >= 3) {
            finishCustomShape();
        } else {
            QMessageBox::warning(this, "Ошибка", "Нужно минимум 3 точки.");
        }
    }
}

        void onMouseMove(QMouseEvent* e) {
        if (m_panningCanvas) {
            Point curScreen{double(e->pos().x()), double(e->pos().y())};
            Point delta = curScreen - m_panStartMouse;

            m_viewOffset.x += delta.x;
            m_viewOffset.y -= delta.y;

            m_panStartMouse = curScreen;
            m_canvas->update();
            return;
        }

        Point p = mouseToScene(e->pos());

        if (m_resizing && m_selectedShape) {
            double dx = p.x - m_resizeStartMouse.x;
            double dy = p.y - m_resizeStartMouse.y;

            double left = m_resizeStartBounds.x;
            double right = m_resizeStartBounds.x + m_resizeStartBounds.width;
            double bottom = m_resizeStartBounds.y;
            double top = m_resizeStartBounds.y + m_resizeStartBounds.height;

            switch (m_resizeHandleIndex) {
                case 0: left += dx; top += dy; break;
                case 1: right += dx; top += dy; break;
                case 2: right += dx; bottom += dy; break;
                case 3: left += dx; bottom += dy; break;
            }

            if (right - left < 20.0) {
                if (m_resizeHandleIndex == 0 || m_resizeHandleIndex == 3) left = right - 20.0;
                else right = left + 20.0;
            }

            if (top - bottom < 20.0) {
                if (m_resizeHandleIndex == 0 || m_resizeHandleIndex == 1) top = bottom + 20.0;
                else bottom = top - 20.0;
            }

            Rectangle target{left, bottom, right - left, top - bottom};
            m_selectedShape->resizeToBoundingBox(target);
            m_canvas->update();
            return;
        }

        if (!m_dragging || m_selectedShapes.empty()) return;

        Point delta = p - m_lastMouse;
        Shape* target = currentPropertiesShape();

        if (m_dragAnchor && target) {
            target->setAnchorInside(target->anchor + delta);
        } else {
            for (auto* s : m_selectedShapes) {
                s->moveBy(delta);
            }
        }

        m_lastMouse = p;
        m_canvas->update();
    }
    
        void onMouseRelease(QMouseEvent* e) {
        Q_UNUSED(e);

        if (m_dragging || m_resizing) {
            updatePropertiesPanel();
            m_canvas->update();
        }

        m_dragging = false;
        m_dragAnchor = false;
        m_resizing = false;
        m_resizeHandleIndex = -1;
        m_panningCanvas = false;
    }

        void onWheel(QWheelEvent* e) {
        QPoint angleDelta = e->angleDelta();
        if (angleDelta.y() == 0) return;

        double factor = (angleDelta.y() > 0) ? 1.1 : (1.0 / 1.1);

        double oldScale = m_viewScale;
        double newScale = std::clamp(oldScale * factor, m_minViewScale, m_maxViewScale);
        if (std::abs(newScale - oldScale) < 1e-9) return;

        QPoint mousePos = e->position().toPoint();

        Point worldBefore{
            (double(mousePos.x()) - m_viewOffset.x) / oldScale,
            (double(m_canvas->height() - mousePos.y()) - m_viewOffset.y) / oldScale
        };

        m_viewScale = newScale;

        m_viewOffset.x = double(mousePos.x()) - worldBefore.x * m_viewScale;
        m_viewOffset.y = double(m_canvas->height() - mousePos.y()) - worldBefore.y * m_viewScale;

        m_canvas->update();
    }

    void deleteSelected() {
    if (m_selectedShapes.empty()) return;

    for (int i = 0; i < m_shapeCount; ) {
        bool shouldDelete = false;

        for (Shape* selected : m_selectedShapes) {
            if (m_shapes[i] == selected) {
                shouldDelete = true;
                break;
            }
        }

        if (shouldDelete) {
            removeShapeAt(i, true);
        } else {
            ++i;
        }
    }

    m_selectedShapes.clear();
    m_selectedShape = nullptr;
    m_propertiesShape = nullptr;
    m_highlightSideIndex = -1;
    m_highlightShape = nullptr;
    m_highlightAngleIndex = -1;
    m_highlightAngleShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}
    void groupSelected() {
    if (m_selectedShapes.size() < 2) {
        QMessageBox::warning(this, "Ошибка", "Нужно выбрать минимум 2 фигуры.");
        return;
    }

    auto* group = new GroupShape();

    std::vector<Shape*> selected = m_selectedShapes;

    for (Shape* s : selected) {
        int idx = indexOfShape(s);
        if (idx < 0) continue;

        group->children.push_back(std::unique_ptr<Shape>(m_shapes[idx]));
        removeShapeAt(idx, false);
    }

    if (group->children.empty()) {
        delete group;
        return;
    }

    group->setAnchorToBoundingBoxCenter();

    if (!addShapeToArray(group)) {
        delete group;
        QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
        return;
    }

    m_selectedShapes = {group};
    m_selectedShape = group;
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

    void addSelectedToGroup() {
    GroupShape* group = nullptr;

    for (Shape* s : m_selectedShapes) {
        group = dynamic_cast<GroupShape*>(s);
        if (group) break;
    }

    if (!group) {
        QMessageBox::information(
            this,
            "Составная фигура",
            "Сначала выдели составную фигуру и ещё одну или несколько обычных фигур через Shift + клик."
        );
        return;
    }

    std::vector<Shape*> toAdd;
    for (Shape* s : m_selectedShapes) {
        if (s != group) {
            toAdd.push_back(s);
        }
    }

    if (toAdd.empty()) {
        QMessageBox::information(
            this,
            "Составная фигура",
            "Нужно выбрать хотя бы одну дополнительную фигуру для добавления."
        );
        return;
    }

    for (Shape* s : toAdd) {
        int idx = indexOfShape(s);
        if (idx < 0) continue;

        group->children.push_back(std::unique_ptr<Shape>(m_shapes[idx]));
        removeShapeAt(idx, false);
    }

    group->setAnchorToBoundingBoxCenter();

    m_selectedShapes = {group};
    m_selectedShape = group;
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}
    void ungroupSelected() {
    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    if (!group) return;

    int idx = indexOfShape(group);
    if (idx < 0) return;

    std::vector<Shape*> newSelection;
    newSelection.reserve(group->children.size());

    std::vector<std::unique_ptr<Shape>> extracted;
    extracted.reserve(group->children.size());

    for (auto& child : group->children) {
        extracted.push_back(std::move(child));
    }
    group->children.clear();

    removeShapeAt(idx, false);

    for (auto& child : extracted) {
        Shape* raw = child.release();
        if (addShapeToArray(raw)) {
            newSelection.push_back(raw);
        } else {
            delete raw;
            QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
            break;
        }
    }

    m_selectedShapes = newSelection;
    m_selectedShape = m_selectedShapes.empty() ? nullptr : m_selectedShapes.back();
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

    void removeSelectedChildFromGroup() {
    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    if (!group) return;

    int row = m_groupChildrenList->currentRow();
    if (row < 0 || row >= (int)group->children.size()) return;

    auto extracted = std::move(group->children[row]);
    group->children.erase(group->children.begin() + row);

    Shape* extractedRaw = extracted.release();
    if (!addShapeToArray(extractedRaw)) {
        delete extractedRaw;
        QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
        return;
    }

    m_propertiesShape = nullptr;

    if (group->children.empty()) {
        removeShapePointer(group, true);
        m_selectedShape = nullptr;
        m_selectedShapes.clear();
    }
    else if (group->children.size() == 1) {
        auto remaining = std::move(group->children[0]);
        group->children.clear();

        removeShapePointer(group, true);

        Shape* remainingRaw = remaining.release();
        if (!addShapeToArray(remainingRaw)) {
            delete remainingRaw;
            QMessageBox::warning(this, "Ошибка", "Массив фигур заполнен.");
            m_selectedShape = nullptr;
            m_selectedShapes.clear();
            return;
        }

        m_selectedShape = remainingRaw;
        m_selectedShapes = {m_selectedShape};
        m_propertiesShape = nullptr;
    }
    else {
        group->setAnchorToBoundingBoxCenter();

        int newRow = std::min(row, (int)group->children.size() - 1);
        if (newRow >= 0) {
            m_propertiesShape = group->children[newRow].get();
        }

        m_selectedShape = group;
        m_selectedShapes = {group};
    }

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

    void updateFillButton() {
        QColor shown = (m_noFillCheck && m_noFillCheck->isChecked())
            ? Qt::white
            : (m_pendingFill == Qt::transparent ? Qt::white : m_pendingFill);

        QString text = (m_noFillCheck && m_noFillCheck->isChecked()) ? "Прозрачный" : shown.name();
        m_fillButton->setText(text);
        m_fillButton->setStyleSheet(
            QString("background:%1; color:%2; border:1px solid #9bc9ee; border-radius:6px; padding:6px;")
                .arg(shown.name())
                .arg(contrastColor(shown).name())
        );
    }

    void rebuildVerticesTable() {
        m_updatingUi = true;

        m_verticesTable->clearContents();
        m_verticesTable->setRowCount(0);

        auto* poly = dynamic_cast<PolygonShape*>(currentPropertiesShape());
        if (!poly || !poly->supportsVerticesEditing()) {
            m_verticesTable->setEnabled(false);
            m_applyVerticesBtn->setEnabled(false);
            m_updatingUi = false;
            return;
        }

        m_verticesTable->setEnabled(true);
        m_applyVerticesBtn->setEnabled(true);

        auto rel = poly->verticesRel();
        m_verticesTable->setRowCount((int)rel.size());

        for (int i = 0; i < (int)rel.size(); ++i) {
            auto* idx = new QTableWidgetItem(QString::number(i + 1));
            idx->setFlags(idx->flags() & ~Qt::ItemIsEditable);

            auto* x = new QTableWidgetItem(QString::number((int)std::round(rel[i].x)));
            auto* y = new QTableWidgetItem(QString::number((int)std::round(rel[i].y)));

            m_verticesTable->setItem(i, 0, idx);
            m_verticesTable->setItem(i, 1, x);
            m_verticesTable->setItem(i, 2, y);
        }

        m_updatingUi = false;
    }

    void rebuildGroupChildrenList() {
        int oldRow = -1;
        if (m_groupChildrenList) {
            oldRow = m_groupChildrenList->currentRow();
        }

        auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
        bool enabled = group != nullptr;

        m_updatingUi = true;

        m_groupChildrenList->clear();
        m_groupChildrenList->setEnabled(enabled);
        m_removeChildBtn->setEnabled(enabled);
        m_groupChildrenBox->setVisible(enabled);

        if (!group) {
            m_updatingUi = false;
            return;
        }

        for (size_t i = 0; i < group->children.size(); ++i) {
            QString text = QString("%1. %2 [ID: %3]")
    .arg(i + 1)
    .arg(group->children[i]->kindName())
    .arg(group->children[i]->id);

m_groupChildrenList->addItem(text);
        }

        int rowToSelect = -1;

        if (m_propertiesShape) {
            for (int i = 0; i < (int)group->children.size(); ++i) {
                if (group->children[i].get() == m_propertiesShape) {
                    rowToSelect = i;
                    break;
                }
            }
        }

        if (rowToSelect == -1 && oldRow >= 0 && oldRow < m_groupChildrenList->count()) {
            rowToSelect = oldRow;
        }

        if (rowToSelect >= 0 && rowToSelect < m_groupChildrenList->count()) {
            m_groupChildrenList->setCurrentRow(rowToSelect);
            m_propertiesShape = group->children[rowToSelect].get();
        } else {
            m_groupChildrenList->clearSelection();
            if (m_propertiesShape && std::find_if(group->children.begin(), group->children.end(),
                [this](const std::unique_ptr<Shape>& ptr) { return ptr.get() == m_propertiesShape; }) == group->children.end()) {
                m_propertiesShape = nullptr;
            }
        }

        m_updatingUi = false;
    }

    void rebuildSidesPanel() {
        clearLayoutItems(m_sidesLayout);
        m_highlightSideIndex = -1;
        m_highlightShape = nullptr;
        m_highlightAngleIndex = -1;
        m_highlightAngleShape = nullptr;
        m_trapezoidIsoscelesCheck = nullptr;

        Shape* target = currentPropertiesShape();
        if (!target) {
            m_sidesLayout->addWidget(new QLabel("Фигура не выбрана"));
            return;
        }

        if (target->isGroup()) {
            m_sidesLayout->addWidget(new QLabel("У составной фигуры стороны редактируются у вложенных фигур."));
            return;
        }

        if (auto* trap = dynamic_cast<TrapezoidShape*>(target)) {
            m_trapezoidIsoscelesCheck = new QCheckBox("Равнобедренная трапеция");
            m_trapezoidIsoscelesCheck->setChecked(trap->isIsosceles);

            connect(m_trapezoidIsoscelesCheck, &QCheckBox::toggled, this, [this, trap](bool checked) {
    if (m_updatingUi) return;
    trap->isIsosceles = checked;
    syncSelectedGroupGeometry();
    m_canvas->update();

    QTimer::singleShot(0, this, [this]() {
        updatePropertiesPanel();
    });
});

            m_sidesLayout->addWidget(m_trapezoidIsoscelesCheck);
        }

        target->ensureStyleArrays();
        std::vector<double> lengths = target->sideLengths();
        std::vector<double> angles = target->interiorAngles();

        for (int i = 0; i < target->sideCount(); ++i) {
            auto* rowWidget = new QWidget;
            auto* col = new QVBoxLayout(rowWidget);
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(4);

            auto* top = new QHBoxLayout;
            auto* sideBtn = new QToolButton;
            sideBtn->setText(QString("Сторона %1").arg(i + 1));
            sideBtn->setCheckable(true);
            connect(sideBtn, &QToolButton::clicked, this, [this, i, target]() {
                m_highlightSideIndex = i;
                m_highlightShape = target;

                if (target->supportsAngleEditing()) {
                    m_highlightAngleIndex = i;
                    m_highlightAngleShape = target;
                } else {
                    m_highlightAngleIndex = -1;
                    m_highlightAngleShape = nullptr;
                }

                m_canvas->update();
            });
            top->addWidget(sideBtn);
            top->addWidget(new QLabel(QString("Длина: %1 px").arg(i < (int)lengths.size() ? (int)std::round(lengths[i]) : 0)));
            top->addStretch();
            col->addLayout(top);

            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel("Цвет:"));

            auto* colorBtn = new QPushButton(target->sideColors[i].name());
            colorBtn->setStyleSheet(QString("background:%1; color:%2;")
                                    .arg(target->sideColors[i].name())
                                    .arg(contrastColor(target->sideColors[i]).name()));
            connect(colorBtn, &QPushButton::clicked, this, [this, i, colorBtn, target]() {
                QColorDialog dlg(target->sideColors[i], this);
                dlg.setOption(QColorDialog::DontUseNativeDialog, true);
                if (dlg.exec() != QDialog::Accepted) return;

                QColor c = dlg.selectedColor();
                if (!c.isValid()) return;

                target->sideColors[i] = c;
                colorBtn->setText(c.name());
                colorBtn->setStyleSheet(QString("background:%1; color:%2;")
                                        .arg(c.name())
                                        .arg(contrastColor(c).name()));
                m_highlightSideIndex = i;
                m_highlightShape = target;

                if (target->supportsAngleEditing()) {
                    m_highlightAngleIndex = i;
                    m_highlightAngleShape = target;
                } else {
                    m_highlightAngleIndex = -1;
                    m_highlightAngleShape = nullptr;
                }

                syncSelectedGroupGeometry();
                m_canvas->update();
                QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
            });
            row->addWidget(colorBtn);

            row->addWidget(new QLabel("Толщина:"));
            auto* wSpin = new QSpinBox;
            wSpin->setRange(1, 1000000);
            wSpin->setValue((int)std::round(target->sideWidths[i] / 2.0));
            setupSpinBox(wSpin);
            connect(wSpin, &QSpinBox::editingFinished, this, [this, i, target, wSpin]() {
                if (m_updatingUi) return;
                target->sideWidths[i] = wSpin->value() * 2.0;
                m_highlightSideIndex = i;
                m_highlightShape = target;

                if (target->supportsAngleEditing()) {
                    m_highlightAngleIndex = i;
                    m_highlightAngleShape = target;
                } else {
                    m_highlightAngleIndex = -1;
                    m_highlightAngleShape = nullptr;
                }

                syncSelectedGroupGeometry();
                m_canvas->update();
                QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
            });
            row->addWidget(wSpin);

            row->addWidget(new QLabel("Длина:"));
            auto* lenSpin = new QDoubleSpinBox;
            lenSpin->setRange(1.0, 5000.0);
            lenSpin->setDecimals(0);
            lenSpin->setValue(i < (int)lengths.size() ? std::round(lengths[i]) : 0.0);
            setupDoubleSpinBox(lenSpin);
            connect(lenSpin, &QDoubleSpinBox::editingFinished, this, [this, i, target, lenSpin]() {
                if (m_updatingUi) return;
                if (target->setSideLength(i, lenSpin->value())) {
                    m_highlightSideIndex = i;
                    m_highlightShape = target;

                    if (target->supportsAngleEditing()) {
                        m_highlightAngleIndex = i;
                        m_highlightAngleShape = target;
                    } else {
                        m_highlightAngleIndex = -1;
                        m_highlightAngleShape = nullptr;
                    }

                    syncSelectedGroupGeometry();
                    m_canvas->update();
                    QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
                } else {
                    QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
                }
            });
            row->addWidget(lenSpin);

            if (target->supportsAngleEditing() && i < (int)angles.size()) {
                row->addWidget(new QLabel("Угол:"));
                auto* angSpin = new QDoubleSpinBox;

                if (target->kind() == ShapeKind::Triangle) {
                    angSpin->setRange(1.0, 179.0);
                } else {
                    angSpin->setRange(1.0, 359.0);
                }

                angSpin->setDecimals(0);
                angSpin->setValue(std::round(angles[i]));
                setupDoubleSpinBox(angSpin);

                connect(angSpin, &QDoubleSpinBox::editingFinished, this, [this, i, target, angSpin]() {
                    if (m_updatingUi) return;
                    if (target->setAngleAt(i, angSpin->value())) {
                        m_highlightSideIndex = i;
                        m_highlightShape = target;
                        m_highlightAngleIndex = i;
                        m_highlightAngleShape = target;

                        syncSelectedGroupGeometry();
                        m_canvas->update();
                        QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
                    } else {
                        QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
                    }
                });
                row->addWidget(angSpin);
            }

            row->addStretch();
            col->addLayout(row);
            m_sidesLayout->addWidget(rowWidget);
        }

        m_sidesLayout->addStretch();
    }

void updatePropertiesPanel() {
    m_updatingUi = true;

    Shape* target = currentPropertiesShape();
    if (!target) {
        m_boundsLabel->setText("Фигура не выбрана");
        m_anchorX->setValue(0);
        m_anchorY->setValue(0);
        m_anchorInsideX->setValue(0);
        m_anchorInsideY->setValue(0);
        m_scaleSpin->setValue(100.0);
        m_pendingFill = Qt::transparent;
        m_noFillCheck->setChecked(true);

        m_highlightVertexIndex = -1;
        m_highlightVertexShape = nullptr;
        m_highlightAngleIndex = -1;
        m_highlightAngleShape = nullptr;
        m_highlightSideIndex = -1;
        m_highlightShape = nullptr;

        m_noFillCheck->setEnabled(false);
        m_fillButton->setEnabled(false);
        

        updateFillButton();
        rebuildVerticesTable();
        rebuildSidesPanel();
        rebuildGroupChildrenList();

        m_updatingUi = false;
        return;
    }

    m_noFillCheck->setEnabled(true);
    m_fillButton->setEnabled(true);
    
    Rectangle b = target->boundingBox();
    int left = (int)std::round(b.x);
    int top = (int)std::round(b.y + b.height);
    int right = (int)std::round(b.x + b.width);
    int bottom = (int)std::round(b.y);

    m_boundsLabel->setText(
        QString("Тип: %1\nID: %2\nВиртуальные границы: левый верхний (%3, %4), \nправый нижний (%5, %6)")
            .arg(target->kindName())
            .arg(target->id)
            .arg(left)
            .arg(top)
            .arg(right)
            .arg(bottom)
    );

    m_anchorX->setValue((int)std::round(target->anchor.x));
    m_anchorY->setValue((int)std::round(target->anchor.y));
    m_anchorInsideX->setValue((int)std::round(target->anchor.x));
    m_anchorInsideY->setValue((int)std::round(target->anchor.y));

    m_scaleSpin->setValue(target->currentScale);

    m_pendingFill = target->fillColor;
    m_noFillCheck->setChecked(target->fillColor == Qt::transparent);
    updateFillButton();

    rebuildVerticesTable();
    rebuildSidesPanel();
    rebuildGroupChildrenList();

    m_updatingUi = false;
}
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
