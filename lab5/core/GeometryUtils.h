#pragma once

#include <QColor>
#include <QLayout>
#include <vector>
#include "Point.h"

double distancePts(const Point& a, const Point& b);
QColor contrastColor(const QColor& color);
Vector2D* intersectLines(const Vector2D& p1, const Vector2D& d1, const Vector2D& p2, const Vector2D& d2);
Point rotateAround(const Point& p, const Point& center, double radians);

template <typename T>
void clearLayoutItems(T* layout) {
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

double angleAtVertex(const std::vector<Point>& pts, int index);

std::vector<Point> rotateTailFromIndex(
    const std::vector<Point>& pts,
    int pivotIndex,
    int startRotateIndex,
    double radians
);

bool setAngleByMovingCurrentVertexAlongBisector(
    std::vector<Point>& pts,
    int index,
    double targetDeg
);

double polygonSignedArea(const std::vector<Point>& pts);

bool segmentsProperlyIntersect(const Point& a, const Point& b, const Point& c, const Point& d);
bool polygonHasSelfIntersection(const std::vector<Point>& pts);

bool setInternalAngleByMovingCurrentVertex(
    std::vector<Point>& pts,
    int index,
    double targetDeg
);