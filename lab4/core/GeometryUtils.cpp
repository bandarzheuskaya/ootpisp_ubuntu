#include "GeometryUtils.h"
#include <algorithm>
#include <cmath>

double distancePts(const Point& a, const Point& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

QColor contrastColor(const QColor& color) {
    if (!color.isValid() || color.alpha() == 0) return Qt::white;
    const double brightness =
        (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255.0;
    return brightness > 0.5 ? Qt::black : Qt::white;
}

Vector2D* intersectLines(const Vector2D& p1, const Vector2D& d1, const Vector2D& p2, const Vector2D& d2) {
    const double det = d1.X * d2.Y - d1.Y * d2.X;
    if (std::abs(det) < 1e-9) return nullptr;

    const double t = ((p2.X - p1.X) * d2.Y - (p2.Y - p1.Y) * d2.X) / det;
    return new Vector2D(p1.X + d1.X * t, p1.Y + d1.Y * t);
}

Point rotateAround(const Point& p, const Point& center, double radians) {
    const double s = std::sin(radians);
    const double c = std::cos(radians);
    const double dx = p.x - center.x;
    const double dy = p.y - center.y;
    return {
        center.x + dx * c - dy * s,
        center.y + dx * s + dy * c
    };
}

double angleAtVertex(const std::vector<Point>& pts, int index) {
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

    const double smallAngle = std::acos(dot) * 180.0 / M_PI;
    const double cross = v1x * v2y - v1y * v2x;

    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    const bool ccw = area > 0.0;
    const bool isConvex = ccw ? (cross < 0.0) : (cross > 0.0);

    return isConvex ? smallAngle : (360.0 - smallAngle);
}

std::vector<Point> rotateTailFromIndex(
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

bool setAngleByMovingCurrentVertexAlongBisector(
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

    double d1 = distancePts(curr, cand1);
    double d2 = distancePts(curr, cand2);
    pts[index] = (d1 <= d2) ? cand1 : cand2;
    return true;
}

double polygonSignedArea(const std::vector<Point>& pts) {
    if (pts.size() < 3) return 0.0;

    double area = 0.0;
    for (int i = 0; i < (int)pts.size(); ++i) {
        int j = (i + 1) % (int)pts.size();
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return area * 0.5;
}

bool segmentsProperlyIntersect(const Point& a, const Point& b, const Point& c, const Point& d) {
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

bool polygonHasSelfIntersection(const std::vector<Point>& pts) {
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

bool setInternalAngleByMovingCurrentVertex(
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