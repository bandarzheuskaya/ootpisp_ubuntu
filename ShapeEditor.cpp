
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
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
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QFrame>
#include <QListWidget>
#include <QAbstractItemView>
#include <QInputDialog>
#include <QDialog>
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>

// ==================== Геометрия ====================
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

static QString pointText(const Point& p) {
    return QString("%1, %2").arg((int)std::round(p.x)).arg((int)std::round(p.y));
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
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

// ==================== Shapes ====================
enum class ShapeKind {
    Circle,
    Rectangle,
    Triangle,
    Trapezoid,
    Pentagon,
    RegularPolygon,
    CustomPolygon,
    Group
};

class Shape {
public:
    virtual ~Shape() = default;

    Point anchor{0.0, 0.0};
    QColor fillColor = Qt::transparent;
    std::vector<QColor> sideColors;
    std::vector<double> sideWidths;

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

    virtual void moveBy(const Point& delta) {
        anchor = anchor + delta;
    }

    Rectangle boundingBox() const {
        auto verts = verticesAbs();
        if (verts.empty()) return {anchor.x, anchor.y, 0.0, 0.0};
        double minX = verts[0].x, maxX = verts[0].x;
        double minY = verts[0].y, maxY = verts[0].y;
        for (const auto& p : verts) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        double strokeMargin = 0.0;
        for (double w : sideWidths) strokeMargin = std::max(strokeMargin, w);
        if (kind() == ShapeKind::Circle) strokeMargin = std::max(strokeMargin, 0.0);
        return {minX - strokeMargin, minY - strokeMargin,
                (maxX - minX) + strokeMargin * 2.0,
                (maxY - minY) + strokeMargin * 2.0};
    }

    bool isPointNearAnchor(const Point& p, double tolerance = 10.0) const {
        return distancePts(anchor, p) <= tolerance;
    }

    virtual void setAnchorInside(const Point& newAnchor) {
        if (!contains(newAnchor)) return;
        auto rel = verticesRel();
        const Point delta = newAnchor - anchor;
        for (auto& v : rel) {
            v = v - delta;
        }
        setVerticesRel(rel);
        anchor = newAnchor;
    }

    void ensureStyleArrays() {
        const int n = std::max(1, sideCount());
        if ((int)sideColors.size() != n) sideColors.assign(n, QColor(Qt::black));
        if ((int)sideWidths.size() != n) sideWidths.assign(n, 3.0);
    }

protected:
    static bool pointInPolygon(const Point& p, const std::vector<Point>& poly) {
        if (poly.size() < 3) return false;
        bool result = false;
        int j = (int)poly.size() - 1;
        for (int i = 0; i < (int)poly.size(); ++i) {
            if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
                (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y + 1e-12) + poly[i].x)) {
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

class PolygonShape : public Shape {
public:
    PolygonShape(ShapeKind k, const QString& n) : m_kind(k), m_name(n) {}
    ShapeKind kind() const override { return m_kind; }
    QString kindName() const override { return m_name; }

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
        s->anchor = anchor;
        s->relVerts = relVerts;
        s->fillColor = fillColor;
        s->sideColors = sideColors;
        s->sideWidths = sideWidths;
        return s;
    }

    std::vector<double> sideLengths() const {
        auto abs = verticesAbs();
        std::vector<double> out;
        for (int i = 0; i < (int)abs.size(); ++i) {
            out.push_back(distancePts(abs[i], abs[(i + 1) % abs.size()]));
        }
        return out;
    }

    std::vector<double> interiorAngles() const {
        auto abs = verticesAbs();
        std::vector<double> out(abs.size(), 0.0);
        for (int i = 0; i < (int)abs.size(); ++i) {
            const Point prev = abs[(i - 1 + abs.size()) % abs.size()];
            const Point curr = abs[i];
            const Point next = abs[(i + 1) % abs.size()];
            const double a1 = std::atan2(prev.y - curr.y, prev.x - curr.x);
            const double a2 = std::atan2(next.y - curr.y, next.x - curr.x);
            double deg = std::abs((a2 - a1) * 180.0 / M_PI);
            if (deg > 180.0) deg = 360.0 - deg;
            out[i] = deg;
        }
        return out;
    }

    void setInteriorAngleAt(int index, double targetDeg) {
        if (relVerts.size() < 3) return;
        if (m_kind != ShapeKind::CustomPolygon) return;

        auto abs = verticesAbs();
        if (index < 0 || index >= (int)abs.size()) return;
        const int nextIdx = (index + 1) % (int)abs.size();
        if (nextIdx == 0) return;

        auto current = interiorAngles();
        const double deltaDeg = targetDeg - current[index];
        const double radians = deltaDeg * M_PI / 180.0;
        const Point center = abs[index];

        for (int j = nextIdx; j < (int)abs.size(); ++j) {
            abs[j] = rotateAround(abs[j], center, -radians);
        }

        std::vector<Point> newRel;
        for (const auto& p : abs) newRel.push_back(p - anchor);
        relVerts = newRel;
    }

private:
    ShapeKind m_kind;
    QString m_name;
};

class CircleShape : public Shape {
public:
    double radius = 80.0;

    ShapeKind kind() const override { return ShapeKind::Circle; }
    QString kindName() const override { return "Окружность"; }
    std::vector<Point> verticesAbs() const override { return {anchor}; }
    std::vector<Point> verticesRel() const override { return {{0, 0}}; }
    void setVerticesRel(const std::vector<Point>&) override {}
    int sideCount() const override { return 1; }

    std::unique_ptr<Shape> clone() const override {
        auto c = std::make_unique<CircleShape>();
        c->anchor = anchor;
        c->radius = radius;
        c->fillColor = fillColor;
        c->sideColors = sideColors;
        c->sideWidths = sideWidths;
        return c;
    }

    void draw(QPainter& painter) const override {
        QRectF rect(anchor.x - radius, anchor.y - radius, radius * 2.0, radius * 2.0);
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
        return distancePts(anchor, p) <= radius;
    }

    void setAnchorInside(const Point& newAnchor) override {
        if (contains(newAnchor)) anchor = newAnchor;
    }
};

class GroupShape : public Shape {
public:
    std::vector<std::unique_ptr<Shape>> children;
    std::vector<Point> offsets;

    ShapeKind kind() const override { return ShapeKind::Group; }
    QString kindName() const override { return "Сложная фигура"; }
    bool isGroup() const override { return true; }

    std::vector<Point> verticesAbs() const override {
        Rectangle b = groupBoundingBox();
        return {{b.x, b.y}, {b.x + b.width, b.y}, {b.x + b.width, b.y + b.height}, {b.x, b.y + b.height}};
    }

    std::vector<Point> verticesRel() const override { return {}; }
    void setVerticesRel(const std::vector<Point>&) override {}
    int sideCount() const override { return 0; }

    std::unique_ptr<Shape> clone() const override {
        auto g = std::make_unique<GroupShape>();
        g->anchor = anchor;
        g->offsets = offsets;
        for (const auto& c : children) g->children.push_back(c->clone());
        return g;
    }

    std::vector<Shape*> groupedShapes() override {
        std::vector<Shape*> out;
        for (auto& c : children) out.push_back(c.get());
        return out;
    }

    void draw(QPainter& painter) const override {
        for (const auto& c : children) c->draw(painter);
    }

    bool contains(const Point& p) const override {
        for (const auto& c : children) {
            if (c->contains(p)) return true;
        }
        return false;
    }

    void moveBy(const Point& delta) override {
        anchor = anchor + delta;
        for (size_t i = 0; i < children.size(); ++i) {
            children[i]->anchor = anchor + offsets[i];
        }
    }

    Rectangle groupBoundingBox() const {
        if (children.empty()) return {anchor.x, anchor.y, 0.0, 0.0};
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
};

static std::unique_ptr<PolygonShape> makeRectangle() {
    auto s = std::make_unique<PolygonShape>(ShapeKind::Rectangle, "Прямоугольник");
    s->relVerts = {{0, 0}, {220, 0}, {220, 150}, {0, 150}};
    s->ensureStyleArrays();
    return s;
}

static std::unique_ptr<PolygonShape> makeTriangle() {
    auto s = std::make_unique<PolygonShape>(ShapeKind::Triangle, "Треугольник");
    s->relVerts = {{0, 0}, {180, 0}, {90, 156}};
    s->ensureStyleArrays();
    return s;
}

static std::unique_ptr<PolygonShape> makeTrapezoid() {
    auto s = std::make_unique<PolygonShape>(ShapeKind::Trapezoid, "Трапеция");
    s->relVerts = {{0, 0}, {260, 0}, {210, 140}, {50, 140}};
    s->ensureStyleArrays();
    return s;
}

static std::unique_ptr<PolygonShape> makePentagon() {
    auto s = std::make_unique<PolygonShape>(ShapeKind::Pentagon, "Пятиугольник");
    s->relVerts = {{80, 0}, {160, 60}, {130, 160}, {30, 160}, {0, 60}};
    s->ensureStyleArrays();
    return s;
}

static std::unique_ptr<PolygonShape> makeRegularPolygon(int sides, double radius = 90.0) {
    sides = std::max(3, sides);
    auto s = std::make_unique<PolygonShape>(ShapeKind::RegularPolygon, "Равносторонний многоугольник");
    s->relVerts.clear();
    double minX = 1e9, minY = 1e9;
    for (int i = 0; i < sides; ++i) {
        const double ang = -M_PI / 2.0 + 2.0 * M_PI * i / sides;
        Point p(radius * std::cos(ang), radius * std::sin(ang));
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        s->relVerts.push_back(p);
    }
    for (auto& p : s->relVerts) {
        p.x -= minX;
        p.y -= minY;
    }
    s->ensureStyleArrays();
    return s;
}

static std::unique_ptr<PolygonShape> makeCustomPolygon() {
    auto s = std::make_unique<PolygonShape>(ShapeKind::CustomPolygon, "Кастомная фигура");
    s->relVerts = {{0, 0}, {160, 0}, {200, 100}, {40, 160}};
    s->ensureStyleArrays();
    return s;
}

// ==================== Main Window ====================
class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setupUi();
        applyLightTheme();
        resize(1500, 920);
        showFullScreen();
        updatePropertiesPanel();
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
            if (event->type() == QEvent::MouseMove) {
                onMouseMove(static_cast<QMouseEvent*>(event));
                return true;
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                onMouseRelease(static_cast<QMouseEvent*>(event));
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
            m_isPlacingCustomShape = false;
            m_tempAbsolutePoints.clear();
            if (m_finishCustomBtn) m_finishCustomBtn->setVisible(false);
            m_statusLabel->setText("Выделение: Shift + клик. Delete – удалить.");
            m_canvas->update();
            return;
        }
        QMainWindow::keyPressEvent(event);
    }

private:
    QWidget* m_canvas = nullptr;
    QWidget* m_propertiesPanel = nullptr;
    QTabWidget* m_rightTabs = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_togglePropertiesBtn = nullptr;

    std::vector<std::unique_ptr<Shape>> m_shapes;
    std::vector<Shape*> m_selectedShapes;
    Shape* m_selectedShape = nullptr;

    bool m_dragging = false;
    bool m_dragAnchor = false;
    Point m_lastMouse{0.0, 0.0};

    // properties
    QLabel* m_infoLabel = nullptr;
    QSpinBox* m_anchorX = nullptr;
    QSpinBox* m_anchorY = nullptr;
    QPushButton* m_applyAnchorBtn = nullptr;
    QCheckBox* m_noFillCheck = nullptr;
    QPushButton* m_fillButton = nullptr;
    QColor m_pendingFill = Qt::transparent;
    QWidget* m_sidesWidget = nullptr;
    QVBoxLayout* m_sidesLayout = nullptr;
    QTableWidget* m_verticesTable = nullptr;
    QPushButton* m_applyVerticesBtn = nullptr;
    QWidget* m_geometryWidget = nullptr;
    QFormLayout* m_geometryForm = nullptr;

    // custom/group tabs
    QSpinBox* m_regularSidesSpin = nullptr;
    QPushButton* m_createCustomBtn = nullptr;
    QPushButton* m_createRegularPolygonBtn = nullptr;
    QPushButton* m_groupBtn = nullptr;
    QPushButton* m_ungroupBtn = nullptr;
    QLabel* m_groupInfo = nullptr;
    QPushButton* m_startCustomPointsBtn = nullptr;
    QPushButton* m_startCustomAnglesBtn = nullptr;
    QPushButton* m_finishCustomBtn = nullptr;
    bool m_isPlacingCustomShape = false;
    int m_customShapeMode = 0; // 0 - точки, 1 - длины и углы
    std::vector<Point> m_tempAbsolutePoints;

    QWidget* createToolbar() {
        auto* bar = new QWidget;
        bar->setFixedHeight(70);
        auto* layout = new QHBoxLayout(bar);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(8);

        auto addBtn = [&](const QString& text, auto fn) {
            auto* btn = new QPushButton(text);
            btn->setMinimumHeight(46);
            connect(btn, &QPushButton::clicked, this, fn);
            layout->addWidget(btn);
            return btn;
        };

        addBtn("Прямоугольник", [this]() { createShapeAndSelect(makeRectangle()); });
        addBtn("Треугольник", [this]() { createShapeAndSelect(makeTriangle()); });
        addBtn("Трапеция", [this]() { createShapeAndSelect(makeTrapezoid()); });
        addBtn("Окружность", [this]() {
            auto c = std::make_unique<CircleShape>();
            c->ensureStyleArrays();
            createShapeAndSelect(std::move(c));
        });
        addBtn("Пятиугольник", [this]() { createShapeAndSelect(makePentagon()); });

        layout->addStretch();

        m_togglePropertiesBtn = new QPushButton("Свернуть свойства");
        connect(m_togglePropertiesBtn, &QPushButton::clicked, this, [this]() {
            const bool visible = m_propertiesPanel->isVisible();
            m_propertiesPanel->setVisible(!visible);
            m_togglePropertiesBtn->setText(visible ? "Показать свойства" : "Свернуть свойства");
        });
        layout->addWidget(m_togglePropertiesBtn);

        auto* deleteBtn = new QPushButton("Удалить");
        connect(deleteBtn, &QPushButton::clicked, this, [this]() { deleteSelected(); });
        layout->addWidget(deleteBtn);

        auto* closeBtn = new QPushButton("Закрыть");
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
        layout->addWidget(closeBtn);

        return bar;
    }

    void setupUi() {
        auto* central = new QWidget;
        setCentralWidget(central);

        auto* root = new QVBoxLayout(central);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        root->addWidget(createToolbar());

        auto* content = new QWidget;
        auto* contentLayout = new QHBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(0);

        m_canvas = new QWidget;
        m_canvas->setMinimumSize(800, 560);
        m_canvas->setMouseTracking(true);
        m_canvas->installEventFilter(this);
        m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        contentLayout->addWidget(m_canvas, 1);
        contentLayout->addWidget(createPropertiesPanel(), 0);
        root->addWidget(content, 1);

        auto* sb = new QStatusBar;
        setStatusBar(sb);
        m_statusLabel = new QLabel("Выделение: Shift + клик. Delete – удалить.");
        sb->addWidget(m_statusLabel, 1);
    }

    QWidget* createPropertiesPanel() {
        m_propertiesPanel = new QWidget;
        m_propertiesPanel->setMinimumWidth(470);
        m_propertiesPanel->setMaximumWidth(470);
        auto* outer = new QVBoxLayout(m_propertiesPanel);
        outer->setContentsMargins(8, 8, 8, 8);
        outer->setSpacing(8);

        m_rightTabs = new QTabWidget;
        outer->addWidget(m_rightTabs);

        // tab 1
        auto* propsTab = new QWidget;
        auto* propsLayout = new QVBoxLayout(propsTab);
        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);

        auto* bodyWidget = new QWidget;
        auto* body = new QVBoxLayout(bodyWidget);

        body->addWidget(makeGroup("Информация", [this](QVBoxLayout* l) {
            m_infoLabel = new QLabel("Фигура не выбрана");
            m_infoLabel->setWordWrap(true);
            l->addWidget(m_infoLabel);
        }));

        body->addWidget(makeGroup("Положение", [this](QVBoxLayout* l) {
            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel("X:"));
            m_anchorX = new QSpinBox;
            m_anchorX->setRange(-5000, 5000);
            row->addWidget(m_anchorX);
            row->addWidget(new QLabel("Y:"));
            m_anchorY = new QSpinBox;
            m_anchorY->setRange(-5000, 5000);
            row->addWidget(m_anchorY);
            m_applyAnchorBtn = new QPushButton("Применить");
            connect(m_applyAnchorBtn, &QPushButton::clicked, this, [this]() {
                if (!m_selectedShape) return;
                Point newAnchor(m_anchorX->value(), m_anchorY->value());
                if (auto* group = dynamic_cast<GroupShape*>(m_selectedShape)) {
                    Point delta = newAnchor - group->anchor;
                    group->moveBy(delta);
                } else {
                    m_selectedShape->anchor = newAnchor;
                }
                m_canvas->update();
                updatePropertiesPanel();
            });
            row->addWidget(m_applyAnchorBtn);
            l->addLayout(row);
        }));

        body->addWidget(makeGroup("Заливка", [this](QVBoxLayout* l) {
            m_noFillCheck = new QCheckBox("Без заливки");
            l->addWidget(m_noFillCheck);

            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel("Цвет:"));
            m_fillButton = new QPushButton("Прозрачный");
            connect(m_fillButton, &QPushButton::clicked, this, [this]() {
                QColor c = QColorDialog::getColor(m_pendingFill == Qt::transparent ? Qt::white : m_pendingFill, this);
                if (!c.isValid()) return;
                m_pendingFill = c;
                if (m_selectedShape && !m_noFillCheck->isChecked()) {
                    m_selectedShape->fillColor = c;
                    m_canvas->update();
                }
                updateFillButton();
            });
            row->addWidget(m_fillButton);
            row->addStretch();
            l->addLayout(row);

            connect(m_noFillCheck, &QCheckBox::toggled, this, [this](bool checked) {
                if (!m_selectedShape) return;
                m_selectedShape->fillColor = checked ? QColor(Qt::transparent) : m_pendingFill;
                updateFillButton();
                m_canvas->update();
            });
        }));

        body->addWidget(makeGroup("Вершины", [this](QVBoxLayout* l) {
            auto* hint = new QLabel("Координаты вершин относительно точки привязки.");
            hint->setWordWrap(true);
            l->addWidget(hint);

            m_verticesTable = new QTableWidget;
            m_verticesTable->setColumnCount(3);
            m_verticesTable->setHorizontalHeaderLabels(QStringList() << "№" << "X" << "Y");
            m_verticesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            m_verticesTable->verticalHeader()->setVisible(false);
            m_verticesTable->setAlternatingRowColors(true);
            m_verticesTable->setSelectionMode(QAbstractItemView::NoSelection);
            m_verticesTable->setMinimumHeight(190);
            l->addWidget(m_verticesTable);

            m_applyVerticesBtn = new QPushButton("Применить вершины");
            connect(m_applyVerticesBtn, &QPushButton::clicked, this, [this]() {
                auto* poly = dynamic_cast<PolygonShape*>(m_selectedShape);
                if (!poly) return;
                std::vector<Point> rel;
                const int rows = m_verticesTable->rowCount();
                for (int r = 0; r < rows; ++r) {
                    QTableWidgetItem* xItem = m_verticesTable->item(r, 1);
                    QTableWidgetItem* yItem = m_verticesTable->item(r, 2);
                    if (!xItem || !yItem) continue;
                    bool okX = false, okY = false;
                    const double x = xItem->text().toDouble(&okX);
                    const double y = yItem->text().toDouble(&okY);
                    if (okX && okY) rel.push_back(Point(x, y));
                }
                if (rel.size() < 3) {
                    QMessageBox::warning(this, "Ошибка", "Нужно минимум 3 вершины.");
                    return;
                }
                poly->setVerticesRel(rel);
                poly->ensureStyleArrays();
                m_canvas->update();
                updatePropertiesPanel();
            });
            l->addWidget(m_applyVerticesBtn);
        }));

        body->addWidget(makeGroup("Стороны", [this](QVBoxLayout* l) {
            m_sidesWidget = new QWidget;
            m_sidesLayout = new QVBoxLayout(m_sidesWidget);
            l->addWidget(m_sidesWidget);
        }));

        body->addWidget(makeGroup("Размер", [this](QVBoxLayout* l) {
            m_geometryWidget = new QWidget;
            m_geometryForm = new QFormLayout(m_geometryWidget);
            l->addWidget(m_geometryWidget);
        }));

        body->addStretch();
        scroll->setWidget(bodyWidget);
        propsLayout->addWidget(scroll);
        m_rightTabs->addTab(propsTab, "Свойства");

                // custom tab
        auto* customTab = new QWidget;
        auto* customLayout = new QVBoxLayout(customTab);

        customLayout->addWidget(makeGroup("Кастомная фигура", [this](QVBoxLayout* l) {
            auto* info = new QLabel("Режимы как раньше: можно рисовать точки на холсте или строить фигуру по длинам и углам.");
            info->setWordWrap(true);
            l->addWidget(info);

            m_startCustomPointsBtn = new QPushButton("Рисовать по точкам");
            connect(m_startCustomPointsBtn, &QPushButton::clicked, this, [this]() {
                m_isPlacingCustomShape = true;
                m_customShapeMode = 0;
                m_tempAbsolutePoints.clear();
                m_finishCustomBtn->setVisible(true);
                m_statusLabel->setText("Кастомная фигура: кликай по холсту, чтобы ставить точки. После 3 точек нажми 'Замкнуть фигуру'.");
            });
            l->addWidget(m_startCustomPointsBtn);

            m_startCustomAnglesBtn = new QPushButton("Строить по длинам и углам");
            connect(m_startCustomAnglesBtn, &QPushButton::clicked, this, [this]() {
                m_isPlacingCustomShape = true;
                m_customShapeMode = 1;
                m_tempAbsolutePoints.clear();
                m_finishCustomBtn->setVisible(true);
                m_statusLabel->setText("Кастомная фигура: выбери первую точку на холсте, затем вводи длины и углы.");
            });
            l->addWidget(m_startCustomAnglesBtn);

            m_finishCustomBtn = new QPushButton("Замкнуть фигуру");
            m_finishCustomBtn->setVisible(false);
            connect(m_finishCustomBtn, &QPushButton::clicked, this, [this]() {
                if (m_tempAbsolutePoints.size() < 3) {
                    QMessageBox::warning(this, "Ошибка", "Для кастомной фигуры нужно минимум 3 точки.");
                    return;
                }
                auto poly = std::make_unique<PolygonShape>(ShapeKind::CustomPolygon, "Кастомная фигура");
                const Point first = m_tempAbsolutePoints.front();
                poly->anchor = first;
                std::vector<Point> rel;
                for (const auto& p : m_tempAbsolutePoints) rel.push_back(p - first);
                poly->setVerticesRel(rel);
                poly->ensureStyleArrays();
                m_shapes.push_back(std::move(poly));
                m_selectedShape = m_shapes.back().get();
                m_selectedShapes = {m_selectedShape};
                m_isPlacingCustomShape = false;
                m_tempAbsolutePoints.clear();
                m_finishCustomBtn->setVisible(false);
                m_statusLabel->setText("Кастомная фигура создана.");
                updatePropertiesPanel();
                m_canvas->update();
            });
            l->addWidget(m_finishCustomBtn);

            l->addSpacing(10);
            l->addWidget(new QLabel("Равносторонний многоугольник:"));
            auto* row = new QHBoxLayout;
            m_regularSidesSpin = new QSpinBox;
            m_regularSidesSpin->setRange(3, 20);
            m_regularSidesSpin->setValue(6);
            row->addWidget(new QLabel("Сторон:"));
            row->addWidget(m_regularSidesSpin);
            m_createRegularPolygonBtn = new QPushButton("Создать");
            connect(m_createRegularPolygonBtn, &QPushButton::clicked, this, [this]() {
                createShapeAndSelect(makeRegularPolygon(m_regularSidesSpin->value()));
                m_statusLabel->setText("Создан равносторонний многоугольник.");
            });
            row->addWidget(m_createRegularPolygonBtn);
            row->addStretch();
            l->addLayout(row);
        }));

        customLayout->addStretch();
        m_rightTabs->addTab(customTab, "Кастомная");

// group tab
        auto* groupTab = new QWidget;
        auto* groupLayout = new QVBoxLayout(groupTab);

        m_groupInfo = new QLabel("Выдели несколько фигур через Shift + клик.");
        m_groupInfo->setWordWrap(true);
        groupLayout->addWidget(m_groupInfo);

        m_groupBtn = new QPushButton("Создать сложную фигуру");
        connect(m_groupBtn, &QPushButton::clicked, this, [this]() { groupSelected(); });
        groupLayout->addWidget(m_groupBtn);

        m_ungroupBtn = new QPushButton("Разгруппировать");
        connect(m_ungroupBtn, &QPushButton::clicked, this, [this]() { ungroupSelected(); });
        groupLayout->addWidget(m_ungroupBtn);

        groupLayout->addStretch();
        m_rightTabs->addTab(groupTab, "Сложная фигура");

        return m_propertiesPanel;
    }

    template <typename Fn>
    QWidget* makeGroup(const QString& title, Fn fn) {
        auto* box = new QGroupBox(title);
        auto* l = new QVBoxLayout(box);
        fn(l);
        return box;
    }

    void applyLightTheme() {
        setStyleSheet(R"(
            QMainWindow, QWidget {
                background: #f6fbff;
                color: #16324d;
                font-size: 14px;
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
            QPlainTextEdit, QLineEdit, QSpinBox, QDoubleSpinBox, QTableWidget, QTabWidget::pane {
                background: white;
                border: 1px solid #b8d5ea;
                border-radius: 6px;
            }
            QTabBar::tab {
                background: #e8f5ff;
                border: 1px solid #b8d5ea;
                padding: 8px 14px;
                border-top-left-radius: 6px;
                border-top-right-radius: 6px;
                margin-right: 2px;
            }
            QTabBar::tab:selected { background: white; }
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

    Point sceneCenter() const {
        return {m_canvas->width() / 2.0 - 100.0, m_canvas->height() / 2.0};
    }

    Point mouseToScene(const QPoint& pos) const {
        return {double(pos.x()), double(m_canvas->height() - pos.y())};
    }

    Shape* hitTest(const Point& p) {
        for (int i = (int)m_shapes.size() - 1; i >= 0; --i) {
            if (m_shapes[i]->contains(p)) return m_shapes[i].get();
        }
        return nullptr;
    }

    void createShapeAndSelect(std::unique_ptr<Shape> shape) {
        shape->anchor = sceneCenter();
        shape->ensureStyleArrays();
        m_shapes.push_back(std::move(shape));
        m_selectedShape = m_shapes.back().get();
        m_selectedShapes = {m_selectedShape};
        updatePropertiesPanel();
        m_canvas->update();
    }

    void paintCanvas() {
        QPainter painter(m_canvas);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(m_canvas->rect(), Qt::white);
        painter.translate(0, m_canvas->height());
        painter.scale(1, -1);

        for (const auto& s : m_shapes) {
            s->draw(painter);
        }

        for (auto* s : m_selectedShapes) {
            Rectangle b = s->boundingBox();
            QPen pen(QColor(56, 132, 201), 2, Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(b.x, b.y, b.width, b.height));
        }

        if (m_isPlacingCustomShape && !m_tempAbsolutePoints.empty()) {
            painter.setPen(QPen(QColor(56, 132, 201), 2, Qt::DashLine));
            painter.setBrush(QColor(56, 132, 201));
            for (size_t i = 0; i < m_tempAbsolutePoints.size(); ++i) {
                const auto& p = m_tempAbsolutePoints[i];
                painter.drawEllipse(QPointF(p.x, p.y), 5, 5);
                if (i + 1 < m_tempAbsolutePoints.size()) {
                    painter.drawLine(QPointF(m_tempAbsolutePoints[i].x, m_tempAbsolutePoints[i].y),
                                     QPointF(m_tempAbsolutePoints[i + 1].x, m_tempAbsolutePoints[i + 1].y));
                }
            }
        }

        if (m_selectedShape) {
            painter.setBrush(QColor(220, 40, 40));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(m_selectedShape->anchor.x, m_selectedShape->anchor.y), 5, 5);
        }
    }

    void onMousePress(QMouseEvent* e) {
        if (e->button() != Qt::LeftButton) return;
        Point p = mouseToScene(e->pos());

        if (m_isPlacingCustomShape) {
            if (m_customShapeMode == 0) {
                m_tempAbsolutePoints.push_back(p);
                m_statusLabel->setText(QString("Точек: %1. После 3 точек можно замкнуть фигуру.").arg((int)m_tempAbsolutePoints.size()));
                m_canvas->update();
                return;
            } else {
                if (m_tempAbsolutePoints.empty()) {
                    m_tempAbsolutePoints.push_back(p);
                    bool okLen = false;
                    double len = QInputDialog::getDouble(this, "Первая сторона", "Длина:", 180.0, 10.0, 1000.0, 1, &okLen);
                    if (!okLen) return;
                    bool okAng = false;
                    double ang = QInputDialog::getDouble(this, "Первая сторона", "Угол к горизонтали:", 0.0, -180.0, 180.0, 1, &okAng);
                    if (!okAng) return;
                    double rad = ang * M_PI / 180.0;
                    Point np(p.x + std::cos(rad) * len, p.y + std::sin(rad) * len);
                    m_tempAbsolutePoints.push_back(np);
                } else {
                    bool okLen = false;
                    double len = QInputDialog::getDouble(this, "Следующая сторона", "Длина:", 180.0, 10.0, 1000.0, 1, &okLen);
                    if (!okLen) return;
                    bool okAng = false;
                    double relAng = QInputDialog::getDouble(this, "Следующая сторона", "Угол относительно предыдущей стороны:", 90.0, -180.0, 180.0, 1, &okAng);
                    if (!okAng) return;
                    Point last = m_tempAbsolutePoints.back();
                    Point prev = m_tempAbsolutePoints[m_tempAbsolutePoints.size() - 2];
                    double lastAngle = std::atan2(last.y - prev.y, last.x - prev.x) * 180.0 / M_PI;
                    double newAngle = lastAngle + relAng;
                    double rad = newAngle * M_PI / 180.0;
                    Point np(last.x + std::cos(rad) * len, last.y + std::sin(rad) * len);
                    m_tempAbsolutePoints.push_back(np);
                }
                m_statusLabel->setText(QString("Точек: %1. Нажми 'Замкнуть фигуру', когда закончишь.").arg((int)m_tempAbsolutePoints.size()));
                m_canvas->update();
                return;
            }
        }

        const bool shift = e->modifiers() & Qt::ShiftModifier;
        Shape* clicked = hitTest(p);

        if (shift) {
            if (clicked) {
                auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clicked);
                if (it == m_selectedShapes.end()) {
                    m_selectedShapes.push_back(clicked);
                } else {
                    m_selectedShapes.erase(it);
                }
                m_selectedShape = m_selectedShapes.empty() ? nullptr : m_selectedShapes.back();
                updatePropertiesPanel();
                m_canvas->update();
            }
            return;
        }

        if (m_selectedShape && m_selectedShape->isPointNearAnchor(p, 14.0)) {
            m_dragging = true;
            m_dragAnchor = true;
            m_lastMouse = p;
            return;
        }

        if (clicked) {
            m_selectedShapes = {clicked};
            m_selectedShape = clicked;
            m_dragging = true;
            m_dragAnchor = false;
            m_lastMouse = p;
            updatePropertiesPanel();
            m_canvas->update();
        } else {
            m_selectedShapes.clear();
            m_selectedShape = nullptr;
            updatePropertiesPanel();
            m_canvas->update();
        }
    }

    void onMouseMove(QMouseEvent* e) {
        if (!m_dragging || m_selectedShapes.empty()) return;
        Point p = mouseToScene(e->pos());
        Point delta = p - m_lastMouse;

        if (m_dragAnchor && m_selectedShape) {
            Point newAnchor = m_selectedShape->anchor + delta;
            m_selectedShape->setAnchorInside(newAnchor);
        } else {
            for (auto* s : m_selectedShapes) {
                s->moveBy(delta);
            }
        }

        m_lastMouse = p;
        updatePropertiesPanel();
        m_canvas->update();
    }

    void onMouseRelease(QMouseEvent*) {
        m_dragging = false;
        m_dragAnchor = false;
    }

    void deleteSelected() {
        if (m_selectedShapes.empty()) return;
        m_shapes.erase(
            std::remove_if(m_shapes.begin(), m_shapes.end(), [&](const std::unique_ptr<Shape>& s) {
                return std::find(m_selectedShapes.begin(), m_selectedShapes.end(), s.get()) != m_selectedShapes.end();
            }),
            m_shapes.end()
        );
        m_selectedShapes.clear();
        m_selectedShape = nullptr;
        updatePropertiesPanel();
        m_canvas->update();
    }

    void groupSelected() {
        if (m_selectedShapes.size() < 2) {
            QMessageBox::information(this, "Сложная фигура", "Нужно выбрать минимум 2 фигуры через Shift + клик.");
            return;
        }

        auto group = std::make_unique<GroupShape>();
        double sumX = 0.0;
        double sumY = 0.0;
        for (auto* s : m_selectedShapes) {
            sumX += s->anchor.x;
            sumY += s->anchor.y;
        }
        group->anchor = {sumX / m_selectedShapes.size(), sumY / m_selectedShapes.size()};

        std::vector<Shape*> selected = m_selectedShapes;
        for (auto* s : selected) {
            auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const std::unique_ptr<Shape>& ptr) {
                return ptr.get() == s;
            });
            if (it == m_shapes.end()) continue;
            group->offsets.push_back(s->anchor - group->anchor);
            group->children.push_back(std::move(*it));
            m_shapes.erase(it);
        }

        m_shapes.push_back(std::move(group));
        m_selectedShape = m_shapes.back().get();
        m_selectedShapes = {m_selectedShape};
        m_statusLabel->setText("Создана сложная фигура.");
        updatePropertiesPanel();
        m_canvas->update();
    }

    void ungroupSelected() {
        auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
        if (!group) return;

        auto it = std::find_if(m_shapes.begin(), m_shapes.end(), [&](const std::unique_ptr<Shape>& ptr) {
            return ptr.get() == m_selectedShape;
        });
        if (it == m_shapes.end()) return;

        std::vector<std::unique_ptr<Shape>> extracted;
        for (auto& c : group->children) {
            extracted.push_back(std::move(c));
        }

        m_shapes.erase(it);
        for (auto& c : extracted) {
            m_shapes.push_back(std::move(c));
        }

        m_selectedShapes.clear();
        m_selectedShape = nullptr;
        m_statusLabel->setText("Сложная фигура разгруппирована.");
        updatePropertiesPanel();
        m_canvas->update();
    }

    void updateFillButton() {
        QColor shown = (m_noFillCheck && m_noFillCheck->isChecked()) ? Qt::white : (m_pendingFill == Qt::transparent ? Qt::white : m_pendingFill);
        QString text = (m_noFillCheck && m_noFillCheck->isChecked()) ? "Прозрачный" : shown.name();
        m_fillButton->setText(text);
        m_fillButton->setStyleSheet(QString("background:%1; color:%2; border:1px solid #9bc9ee; border-radius:6px; padding:6px;")
                                    .arg(shown.name())
                                    .arg(contrastColor(shown).name()));
    }

    void rebuildVerticesTable() {
        m_verticesTable->clearContents();
        m_verticesTable->setRowCount(0);

        auto* poly = dynamic_cast<PolygonShape*>(m_selectedShape);
        if (!poly) {
            m_verticesTable->setEnabled(false);
            m_applyVerticesBtn->setEnabled(false);
            return;
        }

        m_verticesTable->setEnabled(true);
        m_applyVerticesBtn->setEnabled(true);

        auto rel = poly->verticesRel();
        m_verticesTable->setRowCount((int)rel.size());
        for (int i = 0; i < (int)rel.size(); ++i) {
            auto* idxItem = new QTableWidgetItem(QString::number(i + 1));
            idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
            auto* xItem = new QTableWidgetItem(QString::number((int)std::round(rel[i].x)));
            auto* yItem = new QTableWidgetItem(QString::number((int)std::round(rel[i].y)));
            m_verticesTable->setItem(i, 0, idxItem);
            m_verticesTable->setItem(i, 1, xItem);
            m_verticesTable->setItem(i, 2, yItem);
        }
    }

    void rebuildSidesPanel() {
        clearLayoutItems(m_sidesLayout);

        if (!m_selectedShape) {
            m_sidesLayout->addWidget(new QLabel("Фигура не выбрана"));
            return;
        }
        if (m_selectedShape->isGroup()) {
            m_sidesLayout->addWidget(new QLabel("Для сложной фигуры редактирование сторон недоступно."));
            return;
        }

        m_selectedShape->ensureStyleArrays();
        auto* poly = dynamic_cast<PolygonShape*>(m_selectedShape);
        std::vector<double> lengths;
        std::vector<double> angles;
        if (poly) {
            lengths = poly->sideLengths();
            angles = poly->interiorAngles();
        }

        for (int i = 0; i < m_selectedShape->sideCount(); ++i) {
            auto* rowWidget = new QWidget;
            auto* col = new QVBoxLayout(rowWidget);
            col->setContentsMargins(0, 0, 0, 0);
            col->setSpacing(4);

            auto* title = new QLabel(QString("Сторона %1 – %2 px")
                                     .arg(i + 1)
                                     .arg(i < (int)lengths.size() ? (int)std::round(lengths[i]) : 0));
            col->addWidget(title);

            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel("Цвет:"));

            auto* colorBtn = new QPushButton(m_selectedShape->sideColors[i].name());
            colorBtn->setStyleSheet(QString("background:%1; color:%2;")
                                    .arg(m_selectedShape->sideColors[i].name())
                                    .arg(contrastColor(m_selectedShape->sideColors[i]).name()));
            connect(colorBtn, &QPushButton::clicked, this, [this, i, colorBtn]() {
                QColor c = QColorDialog::getColor(m_selectedShape->sideColors[i], this);
                if (!c.isValid()) return;
                m_selectedShape->sideColors[i] = c;
                colorBtn->setText(c.name());
                colorBtn->setStyleSheet(QString("background:%1; color:%2;")
                                        .arg(c.name())
                                        .arg(contrastColor(c).name()));
                m_canvas->update();
            });
            row->addWidget(colorBtn);

            row->addWidget(new QLabel("Толщина:"));
            auto* wSpin = new QDoubleSpinBox;
            wSpin->setRange(1.0, 40.0);
            wSpin->setDecimals(1);
            wSpin->setValue(m_selectedShape->sideWidths[i]);
            connect(wSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double v) {
                m_selectedShape->sideWidths[i] = v;
                m_canvas->update();
            });
            row->addWidget(wSpin);

            if (poly && poly->kind() == ShapeKind::CustomPolygon && i < (int)angles.size()) {
                row->addWidget(new QLabel("Угол:"));
                auto* angleSpin = new QDoubleSpinBox;
                angleSpin->setRange(10.0, 170.0);
                angleSpin->setDecimals(1);
                angleSpin->setValue(angles[i]);
                connect(angleSpin, &QDoubleSpinBox::editingFinished, this, [this, poly, i, angleSpin]() {
                    poly->setInteriorAngleAt(i, angleSpin->value());
                    poly->ensureStyleArrays();
                    m_canvas->update();
                    updatePropertiesPanel();
                });
                row->addWidget(angleSpin);
            }

            row->addStretch();
            col->addLayout(row);
            m_sidesLayout->addWidget(rowWidget);
        }

        m_sidesLayout->addStretch();
    }

    void rebuildGeometryPanel() {
        clearLayoutItems(m_geometryForm);

        if (!m_selectedShape) {
            m_geometryForm->addRow(new QLabel("Фигура не выбрана"));
            return;
        }

        if (auto* circle = dynamic_cast<CircleShape*>(m_selectedShape)) {
            auto* rSpin = new QSpinBox;
            rSpin->setRange(1, 5000);
            rSpin->setValue((int)std::round(circle->radius));
            connect(rSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, circle](int v) {
                circle->radius = v;
                m_canvas->update();
            });
            m_geometryForm->addRow("Радиус:", rSpin);
            return;
        }

        auto* poly = dynamic_cast<PolygonShape*>(m_selectedShape);
        if (poly) {
            if (poly->kind() == ShapeKind::RegularPolygon) {
                auto* sidesSpin = new QSpinBox;
                sidesSpin->setRange(3, 20);
                sidesSpin->setValue(poly->sideCount());
                connect(sidesSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, poly](int n) {
                    auto temp = makeRegularPolygon(n);
                    poly->setVerticesRel(temp->relVerts);
                    poly->sideColors.assign(n, QColor(Qt::black));
                    poly->sideWidths.assign(n, 3.0);
                    m_canvas->update();
                    updatePropertiesPanel();
                });
                m_geometryForm->addRow("Сторон:", sidesSpin);
            } else if (poly->kind() == ShapeKind::CustomPolygon) {
                m_geometryForm->addRow(new QLabel("Форма меняется через вершины и углы."));
            } else {
                m_geometryForm->addRow(new QLabel("У обычных фигур углы не меняются."));
            }
            return;
        }

        if (auto* group = dynamic_cast<GroupShape*>(m_selectedShape)) {
            m_geometryForm->addRow(new QLabel(QString("Внутри фигур: %1").arg(group->children.size())));
        }
    }

    void updatePropertiesPanel() {
        if (!m_selectedShape) {
            m_infoLabel->setText("Фигура не выбрана");
            m_anchorX->setValue(0);
            m_anchorY->setValue(0);
            m_pendingFill = Qt::transparent;
            m_noFillCheck->setChecked(true);
            updateFillButton();
            rebuildVerticesTable();
            rebuildSidesPanel();
            rebuildGeometryPanel();
            m_groupInfo->setText("Выдели несколько фигур через Shift + клик.");
            return;
        }

        Rectangle b = m_selectedShape->boundingBox();
        m_infoLabel->setText(
            QString("Тип: %1\nТочка привязки: (%2, %3)\nГраницы: [%4, %5] – [%6, %7]")
                .arg(m_selectedShape->kindName())
                .arg((int)std::round(m_selectedShape->anchor.x))
                .arg((int)std::round(m_selectedShape->anchor.y))
                .arg((int)std::round(b.x))
                .arg((int)std::round(b.y + b.height))
                .arg((int)std::round(b.x + b.width))
                .arg((int)std::round(b.y))
        );

        m_anchorX->setValue((int)std::round(m_selectedShape->anchor.x));
        m_anchorY->setValue((int)std::round(m_selectedShape->anchor.y));
        m_pendingFill = m_selectedShape->fillColor;
        m_noFillCheck->setChecked(m_selectedShape->fillColor == Qt::transparent);
        updateFillButton();

        rebuildVerticesTable();
        rebuildSidesPanel();
        rebuildGeometryPanel();

        if (m_selectedShapes.size() > 1) {
            m_groupInfo->setText(QString("Выделено фигур: %1. Можно создать сложную фигуру.").arg(m_selectedShapes.size()));
        } else if (auto* group = dynamic_cast<GroupShape*>(m_selectedShape)) {
            m_groupInfo->setText(QString("Сложная фигура. Внутри фигур: %1.").arg(group->children.size()));
        } else {
            m_groupInfo->setText("Выдели несколько фигур через Shift + клик.");
        }
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
