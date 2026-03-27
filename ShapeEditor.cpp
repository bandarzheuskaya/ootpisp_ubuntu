#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include <QScreen>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QTimer>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QIcon>
#include <QSplitter>
#include <cmath>
#include <vector>
#include <algorithm>
#include <QDebug>

// ==================== Структуры данных ====================
struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
    Point operator+(const Point& other) const { return Point(x + other.x, y + other.y); }
    Point operator-(const Point& other) const { return Point(x - other.x, y - other.y); }
    Point operator*(double scalar) const { return Point(x * scalar, y * scalar); }
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

struct PointF {
    double x, y;
    PointF(double x = 0, double y = 0) : x(x), y(y) {}
    PointF operator+(const PointF& other) const { return PointF(x + other.x, y + other.y); }
    PointF operator-(const PointF& other) const { return PointF(x - other.x, y - other.y); }
    PointF operator*(double scalar) const { return PointF(x * scalar, y * scalar); }
};

struct Vector2D {
    double X, Y;
    Vector2D(double x = 0, double y = 0) : X(x), Y(y) {}
    Vector2D(const PointF& p) : X(p.x), Y(p.y) {}
    
    Vector2D operator+(const Vector2D& other) const { return Vector2D(X + other.X, Y + other.Y); }
    Vector2D operator-(const Vector2D& other) const { return Vector2D(X - other.X, Y - other.Y); }
    Vector2D operator*(double scalar) const { return Vector2D(X * scalar, Y * scalar); }
    Vector2D operator/(double scalar) const { return Vector2D(X / scalar, Y / scalar); }
    
    double length() const { return std::sqrt(X*X + Y*Y); }
    Vector2D normalized() const { 
        double len = length();
        if (len < 1e-6) return Vector2D(0, 0);
        return Vector2D(X / len, Y / len);
    }
};

struct Rectangle {
    int x, y, width, height;
    Rectangle(int x = 0, int y = 0, int w = 0, int h = 0) : x(x), y(y), width(w), height(h) {}
    int left() const { return x; }
    int right() const { return x + width; }
    int top() const { return y + height; }
    int bottom() const { return y; }
    
    bool contains(const Point& p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }
};

// ==================== Вспомогательные функции ====================
QColor getContrastColor(const QColor& color) {
    if (!color.isValid() || color.alpha() == 0) return Qt::white;
    double brightness = (0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()) / 255;
    return brightness > 0.5 ? Qt::black : Qt::white;
}

Vector2D* intersectLines(const Vector2D& p1, const Vector2D& d1, const Vector2D& p2, const Vector2D& d2) {
    double det = d1.X * d2.Y - d1.Y * d2.X;
    if (std::abs(det) < 1e-6) return nullptr;
    
    double t = ((p2.X - p1.X) * d2.Y - (p2.Y - p1.Y) * d2.X) / det;
    return new Vector2D(p1.X + d1.X * t, p1.Y + d1.Y * t);
}

// ==================== Базовый класс Shape ====================
class Shape : public QObject {
    Q_OBJECT

protected:
    Point m_anchorPoint;              // Абсолютные координаты точки привязки
    std::vector<Point> m_vertices;     // Все вершины ОТНОСИТЕЛЬНО точки привязки
    std::vector<QColor> m_sideColors;
    std::vector<float> m_sideWidths;
    QColor m_fillColor;
    Rectangle m_boundingBox;
    QString m_shapeName;
    QString m_displayName;             // Имя фигуры для отображения

public:
    Shape(const Point& anchor, const std::vector<Point>& vertices,
          const std::vector<QColor>& colors, 
          const std::vector<float>& widths, const QColor& fill)
        : m_anchorPoint(anchor), m_vertices(vertices), 
          m_sideColors(colors), m_sideWidths(widths), m_fillColor(fill) {
        updateBoundingBox();
    }

    virtual ~Shape() {}

    Point anchorPoint() const { return m_anchorPoint; }
    
    virtual void setAnchorPoint(const Point& p) { 
        if (!(m_anchorPoint == p)) {
            m_anchorPoint = p; 
            updateBoundingBox();
            emit geometryChanged();
        }
    }
    
    // Перемещение всей фигуры (точка привязки и все вершины смещаются одинаково)
    virtual void moveShape(const Point& delta) {
        if (delta.x == 0 && delta.y == 0) return;
        
        m_anchorPoint.x += delta.x;
        m_anchorPoint.y += delta.y;
        
        updateBoundingBox();
        emit geometryChanged();
    }
    
    // Установка новой точки привязки внутри фигуры
    // (меняются относительные координаты вершин)
    virtual void setAnchorPointInside(const Point& newAnchor) {
        if (!containsPoint(newAnchor)) return;
        
        Point delta(newAnchor.x - m_anchorPoint.x, 
                    newAnchor.y - m_anchorPoint.y);
        
        // Корректируем относительные координаты вершин
        for (auto& v : m_vertices) {
            v.x -= delta.x;
            v.y -= delta.y;
        }
        
        m_anchorPoint = newAnchor;
        updateBoundingBox();
        emit geometryChanged();
    }
    
    // Проверка, находится ли точка в области точки привязки
    virtual bool isPointNearAnchor(const Point& p, int tolerance = 10) const {
        int dx = p.x - m_anchorPoint.x;
        int dy = p.y - m_anchorPoint.y;
        return (dx * dx + dy * dy) <= tolerance * tolerance;
    }

    std::vector<QColor> sideColors() const { return m_sideColors; }
    std::vector<float> sideWidths() const { return m_sideWidths; }
    QColor fillColor() const { return m_fillColor; }
    Rectangle boundingBox() const { return m_boundingBox; }
    QString shapeName() const { return m_shapeName; }
    
    QString displayName() const { return m_displayName; }
    void setDisplayName(const QString& name) { m_displayName = name; }

    virtual int sidesCount() const = 0;
    
    // Получить все вершины в абсолютных координатах
    virtual std::vector<Point> getVertices() const {
        std::vector<Point> vertices;
        for (const auto& rel : m_vertices) {
            vertices.push_back(Point(m_anchorPoint.x + rel.x, m_anchorPoint.y + rel.y));
        }
        return vertices;
    }
    
    // Получить относительные координаты вершин (относительно точки привязки)
    virtual std::vector<Point> getRelativeVertices() const {
        return m_vertices;
    }
    
    // Установить относительные координаты вершин
    virtual void setRelativeVertices(const std::vector<Point>& vertices) {
        if ((int)vertices.size() == sidesCount()) {
            m_vertices = vertices;
            updateBoundingBox();
            emit geometryChanged();
        }
    }
    
    virtual void draw(QPainter& painter) = 0;
    virtual bool containsPoint(const Point& p) const = 0;

    virtual void updateProperties(const std::vector<QColor>& colors, 
                                   const std::vector<float>& widths, 
                                   const QColor& fill) {
        bool changed = false;
        if (m_sideColors != colors || m_sideWidths != widths || m_fillColor != fill) {
            changed = true;
        }
        m_sideColors = colors;
        m_sideWidths = widths;
        m_fillColor = fill;
        if (changed) emit geometryChanged();
    }

signals:
    void geometryChanged();

protected:
    virtual void updateBoundingBox() {
        auto vertices = getVertices();
        if (vertices.empty()) return;
        
        int minX = vertices[0].x, minY = vertices[0].y;
        int maxX = vertices[0].x, maxY = vertices[0].y;
        
        for (const auto& v : vertices) {
            minX = std::min(minX, v.x);
            minY = std::min(minY, v.y);
            maxX = std::max(maxX, v.x);
            maxY = std::max(maxY, v.y);
        }
        m_boundingBox = Rectangle(minX, minY, maxX - minX, maxY - minY);
    }

    void drawPolygonWithBorder(QPainter& painter, const std::vector<Point>& points) {
        int n = points.size();
        if (n < 3) return;

        // Сначала рисуем заливку всей фигуры
        if (m_fillColor != Qt::transparent && m_fillColor.alpha() > 0) {
            QPolygon polygon;
            for (const auto& p : points) {
                polygon << QPoint(p.x, p.y);
            }
            painter.setBrush(QBrush(m_fillColor));
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(polygon);
        }

        double area = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            area += points[i].x * points[j].y - points[j].x * points[i].y;
        }
        bool isClockwise = area < 0;

        std::vector<PointF> outer(n);
        std::vector<PointF> inner(n);

        std::vector<Vector2D> dirs(n);
        std::vector<Vector2D> normals(n);
        std::vector<double> halfWidths(n);

        for (int i = 0; i < n; i++) {
            int next = (i + 1) % n;
            Point a = points[i];
            Point b = points[next];
            double dx = b.x - a.x;
            double dy = b.y - a.y;
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6) len = 1e-6;
            
            dirs[i] = Vector2D(dx / len, dy / len);

            Vector2D normCCW(-dirs[i].Y, dirs[i].X);
            
            if (isClockwise)
                normals[i] = Vector2D(dirs[i].Y, -dirs[i].X);
            else
                normals[i] = normCCW;

            halfWidths[i] = m_sideWidths[i] / 2.0;
        }

        for (int i = 0; i < n; i++) {
            int prev = (i - 1 + n) % n;
            int next = (i + 1) % n;

            Point v = points[i];

            Vector2D p_prev(v.x + normals[prev].X * halfWidths[prev], 
                           v.y + normals[prev].Y * halfWidths[prev]);
            Vector2D p_next(v.x + normals[i].X * halfWidths[i], 
                           v.y + normals[i].Y * halfWidths[i]);

            Vector2D* outerIntersect = intersectLines(p_prev, dirs[prev], p_next, dirs[i]);
            if (outerIntersect) {
                outer[i] = PointF(outerIntersect->X, outerIntersect->Y);
                delete outerIntersect;
            } else {
                outer[i] = PointF(v.x, v.y);
            }

            Vector2D p_prev_inner(v.x - normals[prev].X * halfWidths[prev], 
                                 v.y - normals[prev].Y * halfWidths[prev]);
            Vector2D p_next_inner(v.x - normals[i].X * halfWidths[i], 
                                 v.y - normals[i].Y * halfWidths[i]);

            Vector2D* innerIntersect = intersectLines(p_prev_inner, dirs[prev], p_next_inner, dirs[i]);
            if (innerIntersect) {
                inner[i] = PointF(innerIntersect->X, innerIntersect->Y);
                delete innerIntersect;
            } else {
                inner[i] = PointF(v.x, v.y);
            }
        }

        for (int i = 0; i < n; i++) {
            int next = (i + 1) % n;
            
            QPointF quad[4] = {
                QPointF(outer[i].x, outer[i].y),
                QPointF(outer[next].x, outer[next].y),
                QPointF(inner[next].x, inner[next].y),
                QPointF(inner[i].x, inner[i].y)
            };

            painter.setBrush(QBrush(m_sideColors[i]));
            painter.setPen(Qt::NoPen);
            painter.drawConvexPolygon(quad, 4);
        }
    }

    bool isPointInPolygon(const Point& p, const std::vector<Point>& polygon) const {
        bool result = false;
        int j = polygon.size() - 1;
        for (size_t i = 0; i < polygon.size(); ++i) {
            if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
                (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) /
                (double)(polygon[j].y - polygon[i].y) + polygon[i].x)) {
                result = !result;
            }
            j = i;
        }
        return result;
    }
};

// ==================== CircleShape ====================
class CircleShape : public Shape {
private:
    int m_radius;

public:
    CircleShape(const Point& anchor, int radius, const QColor& color, 
                float width, const QColor& fill)
        : Shape(anchor, {Point(0, 0)}, {color}, {width}, fill), m_radius(std::max(1, radius)) {
        m_shapeName = "Circle";
        updateBoundingBox();
    }

    int radius() const { return m_radius; }
    void setRadius(int r) {
        if (m_radius != r) {
            m_radius = std::max(1, r);
            updateBoundingBox();
            emit geometryChanged();
        }
    }

    int sidesCount() const override { return 1; }

    std::vector<Point> getVertices() const override {
        return { m_anchorPoint };
    }

    void draw(QPainter& painter) override {
        try {
            updateBoundingBox();
            QRect rect(m_boundingBox.x, m_boundingBox.y, 
                      m_boundingBox.width, m_boundingBox.height);
            
            // Рисуем заливку
            painter.setBrush(QBrush(m_fillColor));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(rect);
            
            // Рисуем контур
            QPen pen(m_sideColors[0]);
            pen.setWidthF(m_sideWidths[0]);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(rect);
        } catch (...) {}
    }

    bool containsPoint(const Point& p) const override {
        int dx = p.x - m_anchorPoint.x;
        int dy = p.y - m_anchorPoint.y;
        return (dx * dx + dy * dy) <= m_radius * m_radius;
    }
    
    bool isPointNearAnchor(const Point& p, int tolerance = 10) const override {
        int dx = p.x - m_anchorPoint.x;
        int dy = p.y - m_anchorPoint.y;
        return (dx * dx + dy * dy) <= tolerance * tolerance;
    }

    void setAnchorPointInside(const Point& newAnchor) override {
        if (!containsPoint(newAnchor)) return;
        m_anchorPoint = newAnchor;
        updateBoundingBox();
        emit geometryChanged();
    }

protected:
    void updateBoundingBox() override {
        m_boundingBox = Rectangle(m_anchorPoint.x - m_radius, 
                                   m_anchorPoint.y - m_radius, 
                                   m_radius * 2, m_radius * 2);
    }
};

// ==================== PolygonShape (базовый для многоугольников) ====================
class PolygonShape : public Shape {
public:
    PolygonShape(const Point& anchor, const std::vector<Point>& vertices,
                 const std::vector<QColor>& colors, 
                 const std::vector<float>& widths, const QColor& fill)
        : Shape(anchor, vertices, colors, widths, fill) {}

    void draw(QPainter& painter) override {
        try {
            updateBoundingBox();
            auto vertices = getVertices();
            drawPolygonWithBorder(painter, vertices);
        } catch (...) {}
    }

    bool containsPoint(const Point& p) const override {
        auto vertices = getVertices();
        return isPointInPolygon(p, vertices);
    }
    
    bool isPointNearAnchor(const Point& p, int tolerance = 10) const override {
        int dx = p.x - m_anchorPoint.x;
        int dy = p.y - m_anchorPoint.y;
        return (dx * dx + dy * dy) <= tolerance * tolerance;
    }
};

// ==================== RectangleShape ====================
class RectangleShape : public PolygonShape {
public:
    RectangleShape(const Point& anchor, int width, int height,
                   const std::vector<QColor>& colors, 
                   const std::vector<float>& widths, const QColor& fill)
        : PolygonShape(anchor, 
                       {Point(0, 0), Point(width * 2, 0), Point(width * 2, height * 2), Point(0, height * 2)},
                       colors, widths, fill) {
        m_shapeName = "Rectangle";
    }

    int sidesCount() const override { return 4; }
};

// ==================== TriangleShape ====================
class TriangleShape : public PolygonShape {
public:
    TriangleShape(const Point& anchor, int sideLength,
                  const std::vector<QColor>& colors, 
                  const std::vector<float>& widths, const QColor& fill)
        : PolygonShape(anchor, 
                       {Point(0, 0), Point(sideLength * 2, 0), Point(sideLength, (int)(sideLength * std::sqrt(3)))},
                       colors, widths, fill) {
        m_shapeName = "Triangle";
    }

    int sidesCount() const override { return 3; }
};

// ==================== TrapezoidShape ====================
class TrapezoidShape : public PolygonShape {
public:
    TrapezoidShape(const Point& anchor, int topBase, int bottomBase, int height,
                   const std::vector<QColor>& colors, 
                   const std::vector<float>& widths, const QColor& fill)
        : PolygonShape(anchor, 
                       {Point(0, 0), Point(bottomBase * 2, 0), 
                        Point((bottomBase + topBase), height * 2), 
                        Point((bottomBase - topBase), height * 2)},
                       colors, widths, fill) {
        m_shapeName = "Trapezoid";
    }

    int sidesCount() const override { return 4; }
};

// ==================== PentagonShape ====================
class PentagonShape : public PolygonShape {
public:
    PentagonShape(const Point& anchor, int sideLength,
                  const std::vector<QColor>& colors, 
                  const std::vector<float>& widths, const QColor& fill)
        : PolygonShape(anchor, 
                       {Point(0, 0), Point(160, 0), Point(209, 152), Point(80, 246), Point(-49, 152)},
                       colors, widths, fill) {
        m_shapeName = "Pentagon";
    }

    int sidesCount() const override { return 5; }
};

// ==================== CustomPolygonShape ====================
class CustomPolygonShape : public PolygonShape {
public:
    CustomPolygonShape(const Point& anchor, const std::vector<Point>& vertices,
                       const std::vector<QColor>& colors, 
                       const std::vector<float>& widths, const QColor& fill)
        : PolygonShape(anchor, vertices, colors, widths, fill) {
        m_shapeName = "Custom";
    }

    int sidesCount() const override { return m_vertices.size(); }
};

// ==================== GroupShape ====================
class GroupShape : public Shape {
private:
    std::vector<Shape*> m_groupedShapes;
    std::vector<Point> m_originalOffsets; // смещения каждой фигуры относительно точки привязки группы
    bool m_ownsShapes; // флаг, владеет ли группа фигурами
    QString m_groupName;

public:
    GroupShape(const Point& anchor, const std::vector<Shape*>& shapes, const QString& name, bool ownsShapes = true)
        : Shape(anchor, {}, {}, {}, Qt::transparent), m_groupedShapes(shapes), m_ownsShapes(ownsShapes), m_groupName(name) {
        m_shapeName = "Group";
        setDisplayName(name);
        
        // Вычисляем смещения каждой фигуры относительно точки привязки группы
        for (auto* shape : shapes) {
            Point shapeAnchor = shape->anchorPoint();
            m_originalOffsets.push_back(Point(
                shapeAnchor.x - anchor.x,
                shapeAnchor.y - anchor.y
            ));
        }
        
        updateBoundingBox();
    }

    ~GroupShape() {
        // Удаляем фигуры только если группа ими владеет
        if (m_ownsShapes) {
            for (auto* shape : m_groupedShapes) {
                delete shape;
            }
        }
    }

    int sidesCount() const override { return 0; }

    void draw(QPainter& painter) override {
        for (auto* shape : m_groupedShapes) {
            shape->draw(painter);
        }
    }

    bool containsPoint(const Point& p) const override {
        for (auto* shape : m_groupedShapes) {
            if (shape->containsPoint(p)) return true;
        }
        return false;
    }

    void moveShape(const Point& delta) override {
        // Перемещаем точку привязки группы
        m_anchorPoint = m_anchorPoint + delta;
        
        // Перемещаем все фигуры в группе
        for (size_t i = 0; i < m_groupedShapes.size(); ++i) {
            Point newAnchor(
                m_anchorPoint.x + m_originalOffsets[i].x,
                m_anchorPoint.y + m_originalOffsets[i].y
            );
            m_groupedShapes[i]->setAnchorPoint(newAnchor);
        }
        updateBoundingBox();
    }

    void setAnchorPoint(const Point& p) override {
        Point delta(p.x - m_anchorPoint.x, p.y - m_anchorPoint.y);
        moveShape(delta);
    }

    std::vector<Shape*> getGroupedShapes() const { return m_groupedShapes; }
    
    // Метод для освобождения фигур (при разгруппировке)
    void releaseShapes() {
        m_ownsShapes = false;
    }

protected:
    void updateBoundingBox() override {
        if (m_groupedShapes.empty()) return;
        
        // Вычисляем общий ограничивающий прямоугольник всех фигур в группе
        Rectangle firstBounds = m_groupedShapes[0]->boundingBox();
        int minX = firstBounds.x, minY = firstBounds.y;
        int maxX = firstBounds.x + firstBounds.width;
        int maxY = firstBounds.y + firstBounds.height;
        
        for (size_t i = 1; i < m_groupedShapes.size(); ++i) {
            Rectangle bounds = m_groupedShapes[i]->boundingBox();
            minX = std::min(minX, bounds.x);
            minY = std::min(minY, bounds.y);
            maxX = std::max(maxX, bounds.x + bounds.width);
            maxY = std::max(maxY, bounds.y + bounds.height);
        }
        
        m_boundingBox = Rectangle(minX, minY, maxX - minX, maxY - minY);
    }
};

// ==================== NameInputDialog ====================
class NameInputDialog : public QDialog {
    Q_OBJECT

private:
    QLineEdit* m_nameEdit;
    QLabel* m_typeLabel;

public:
    NameInputDialog(const QString& shapeType, QWidget* parent = nullptr) : QDialog(parent) {
        setupUI(shapeType);
    }

    QString getName() const { return m_nameEdit->text(); }

private slots:
    void onOkClicked() {
        if (m_nameEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Имя не может быть пустым");
            return;
        }
        accept();
    }

private:
    void setupUI(const QString& shapeType) {
        setWindowTitle("Имя фигуры");
        resize(400, 200);
        setWindowFlags(Qt::FramelessWindowHint);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        
        QLabel* titleLabel = new QLabel("Введите имя фигуры:");
        titleLabel->setStyleSheet("font-size: 11pt; color: white;");
        mainLayout->addWidget(titleLabel);
        
        m_typeLabel = new QLabel(QString("Тип: %1").arg(shapeType));
        m_typeLabel->setStyleSheet("color: #aaaaaa;");
        mainLayout->addWidget(m_typeLabel);
        
        m_nameEdit = new QLineEdit;
        m_nameEdit->setPlaceholderText("Имя фигуры");
        m_nameEdit->setStyleSheet("QLineEdit { background-color: #46464b; color: white; border: 1px solid #555; padding: 8px; border-radius: 5px; }");
        mainLayout->addWidget(m_nameEdit);
        
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        
        QPushButton* okBtn = new QPushButton("OK");
        okBtn->setFixedSize(100, 35);
        okBtn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #5a9acf; }"
        );
        connect(okBtn, &QPushButton::clicked, this, &NameInputDialog::onOkClicked);
        
        QPushButton* cancelBtn = new QPushButton("Отмена");
        cancelBtn->setFixedSize(100, 35);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #b45f5f; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #c46f6f; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        buttonLayout->addStretch();
        
        mainLayout->addLayout(buttonLayout);
        
        setStyleSheet("QDialog { background-color: #2d2d2d; } QLabel { color: white; }");
    }
};

// ==================== GroupNameDialog ====================
class GroupNameDialog : public QDialog {
    Q_OBJECT

private:
    QLineEdit* m_nameEdit;

public:
    GroupNameDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setupUI();
    }

    QString getName() const { return m_nameEdit->text(); }

private slots:
    void onOkClicked() {
        if (m_nameEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Имя группы не может быть пустым");
            return;
        }
        accept();
    }

private:
    void setupUI() {
        setWindowTitle("Имя группы");
        resize(400, 180);
        setWindowFlags(Qt::FramelessWindowHint);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        
        QLabel* titleLabel = new QLabel("Введите имя группы:");
        titleLabel->setStyleSheet("font-size: 11pt; color: white;");
        mainLayout->addWidget(titleLabel);
        
        m_nameEdit = new QLineEdit;
        m_nameEdit->setPlaceholderText("Имя группы");
        m_nameEdit->setStyleSheet("QLineEdit { background-color: #46464b; color: white; border: 1px solid #555; padding: 8px; border-radius: 5px; }");
        mainLayout->addWidget(m_nameEdit);
        
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        
        QPushButton* okBtn = new QPushButton("OK");
        okBtn->setFixedSize(100, 35);
        okBtn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #5a9acf; }"
        );
        connect(okBtn, &QPushButton::clicked, this, &GroupNameDialog::onOkClicked);
        
        QPushButton* cancelBtn = new QPushButton("Отмена");
        cancelBtn->setFixedSize(100, 35);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #b45f5f; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #c46f6f; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        buttonLayout->addStretch();
        
        mainLayout->addLayout(buttonLayout);
        
        setStyleSheet("QDialog { background-color: #2d2d2d; } QLabel { color: white; }");
    }
};

// ==================== DoubleBufferedWidget ====================
class DoubleBufferedWidget : public QWidget {
public:
    DoubleBufferedWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAutoFillBackground(false);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        
        painter.translate(0, height());
        painter.scale(1, -1);
        
        QPaintEvent invertedEvent(QRegion(QRect(0, 0, width(), height())));
        customPaint(painter, &invertedEvent);
    }

    virtual void customPaint(QPainter& painter, QPaintEvent* event) {
        painter.fillRect(rect(), Qt::white);
    }
};

// ==================== CustomShapeChoiceDialog ====================
class CustomShapeChoiceDialog : public QDialog {
    Q_OBJECT

public:
    enum CreationMethod { PointsMethod, AnglesMethod };

    CustomShapeChoiceDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setupUI();
    }

    CreationMethod getSelectedMethod() const { return m_selectedMethod; }

private slots:
    void onPointsMethod() {
        m_selectedMethod = PointsMethod;
        accept();
    }

    void onAnglesMethod() {
        m_selectedMethod = AnglesMethod;
        accept();
    }

private:
    void setupUI() {
        setWindowTitle("Создание кастомной фигуры");
        resize(400, 250);
        setWindowFlags(Qt::FramelessWindowHint);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        
        QLabel* titleLabel = new QLabel("Выберите способ создания фигуры:");
        titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;");
        mainLayout->addWidget(titleLabel);
        
        QPushButton* pointsBtn = new QPushButton("Расстановка точек на холсте");
        pointsBtn->setFixedHeight(50);
        pointsBtn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; font-size: 11pt; border-radius: 5px; margin: 5px; }"
            "QPushButton:hover { background-color: #5a9acf; }"
        );
        connect(pointsBtn, &QPushButton::clicked, this, &CustomShapeChoiceDialog::onPointsMethod);
        mainLayout->addWidget(pointsBtn);
        
        QPushButton* anglesBtn = new QPushButton("Длины и углы");
        anglesBtn->setFixedHeight(50);
        anglesBtn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; font-size: 11pt; border-radius: 5px; margin: 5px; }"
            "QPushButton:hover { background-color: #5a9acf; }"
        );
        connect(anglesBtn, &QPushButton::clicked, this, &CustomShapeChoiceDialog::onAnglesMethod);
        mainLayout->addWidget(anglesBtn);
        
        QPushButton* cancelBtn = new QPushButton("Отмена");
        cancelBtn->setFixedHeight(40);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #b45f5f; color: white; border-radius: 5px; margin: 5px; }"
            "QPushButton:hover { background-color: #c46f6f; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        mainLayout->addWidget(cancelBtn);
        
        setStyleSheet("QDialog { background-color: #2d2d2d; } QLabel { color: white; }");
    }

    CreationMethod m_selectedMethod = PointsMethod;
};

// ==================== LengthAngleInputDialog ====================
class LengthAngleInputDialog : public QDialog {
    Q_OBJECT

private:
    QDoubleSpinBox* m_lengthSpin;
    QDoubleSpinBox* m_angleSpin;
    bool m_isFirstSegment;

public:
    LengthAngleInputDialog(bool isFirst, QWidget* parent = nullptr) 
        : QDialog(parent), m_isFirstSegment(isFirst) {
        setupUI();
    }

    double getLength() const { return m_lengthSpin->value(); }
    double getAngle() const { return m_angleSpin->value(); }

private slots:
    void onOkClicked() {
        accept();
    }

private:
    void setupUI() {
        setWindowTitle(m_isFirstSegment ? "Параметры первой стороны" : "Параметры следующей стороны");
        resize(350, 250);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        
        QLabel* infoLabel = new QLabel(
            m_isFirstSegment ? 
            "Первая сторона:\nУгол относительно горизонтали вправо" :
            "Следующая сторона:\nУгол относительно предыдущей стороны\n(против часовой стрелки)"
        );
        infoLabel->setWordWrap(true);
        infoLabel->setStyleSheet("color: #aaaaaa; padding: 10px; background-color: #3d3d3d; border-radius: 5px;");
        mainLayout->addWidget(infoLabel);
        
        QFormLayout* formLayout = new QFormLayout;
        formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
        formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
        formLayout->setFormAlignment(Qt::AlignHCenter | Qt::AlignTop);
        formLayout->setLabelAlignment(Qt::AlignRight);
        formLayout->setSpacing(10);
        
        m_lengthSpin = new QDoubleSpinBox;
        m_lengthSpin->setRange(10, 500);
        m_lengthSpin->setValue(200);
        m_lengthSpin->setSuffix(" px");
        m_lengthSpin->setFixedWidth(150);
        formLayout->addRow("Длина:", m_lengthSpin);
        
        m_angleSpin = new QDoubleSpinBox;
        m_angleSpin->setRange(-180, 180);
        m_angleSpin->setValue(m_isFirstSegment ? 0 : 90);
        m_angleSpin->setSuffix("°");
        m_angleSpin->setFixedWidth(150);
        formLayout->addRow("Угол:", m_angleSpin);
        
        mainLayout->addLayout(formLayout);
        
        QHBoxLayout* buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        
        QPushButton* okBtn = new QPushButton("OK");
        okBtn->setFixedSize(100, 40);
        okBtn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #5a9acf; }"
        );
        connect(okBtn, &QPushButton::clicked, this, &LengthAngleInputDialog::onOkClicked);
        
        QPushButton* cancelBtn = new QPushButton("Отмена");
        cancelBtn->setFixedSize(100, 40);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #b45f5f; color: white; border: none; border-radius: 5px; }"
            "QPushButton:hover { background-color: #c46f6f; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        buttonLayout->addStretch();
        
        mainLayout->addLayout(buttonLayout);
        
        setStyleSheet(
            "QDialog { background-color: #2d2d2d; }"
            "QLabel { color: white; }"
            "QDoubleSpinBox { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; border-radius: 3px; }"
        );
    }
};

// ==================== ShapeParametersDialog ====================
class ShapeParametersDialog : public QDialog {
    Q_OBJECT

private:
    QComboBox* m_shapeTypeCombo;
    QSpinBox* m_anchorX;
    QSpinBox* m_anchorY;
    std::vector<QPushButton*> m_colorButtons;
    std::vector<QSpinBox*> m_widthSpinBoxes;
    QPushButton* m_btnFillColor;
    QColor m_selectedFillColor;
    QList<QColor> m_selectedColors;
    QList<float> m_selectedWidths;
    std::vector<Point> m_relativeVertices;
    QSpinBox* m_radiusSpin;
    QWidget* m_verticesPanel;
    QWidget* m_sidesPanel;
    QLabel* m_vertexCoordsLabel;
    QList<QLineEdit*> m_vertexXEdits;
    QList<QLineEdit*> m_vertexYEdits;
    Point m_defaultAnchor;

public:
    std::vector<QColor> selectedColors() const {
        std::vector<QColor> colors;
        for (const auto& c : m_selectedColors) colors.push_back(c);
        return colors;
    }

    std::vector<float> selectedWidths() const {
        std::vector<float> widths;
        for (float w : m_selectedWidths) widths.push_back(w);
        return widths;
    }

    QColor selectedFillColor() const { return m_selectedFillColor; }
    Point anchorPoint() const { return Point(m_anchorX->value(), m_anchorY->value()); }
    QString shapeType() const { return m_shapeTypeCombo->currentText(); }
    int circleRadius() const { return m_radiusSpin ? m_radiusSpin->value() : 80; }
    
    std::vector<Point> getUserVertices() const {
        std::vector<Point> vertices;
        
        // Все вершины задаются относительно точки привязки
        for (int i = 0; i < m_vertexXEdits.size(); ++i) {
            bool xOk, yOk;
            int x = m_vertexXEdits[i]->text().toInt(&xOk);
            int y = m_vertexYEdits[i]->text().toInt(&yOk);
            if (xOk && yOk) {
                vertices.push_back(Point(x, y));
            } else {
                // Если данные некорректны, используем значения по умолчанию
                vertices.push_back(Point(100, 100));
            }
        }
        return vertices;
    }

    ShapeParametersDialog(const Point& screenCenter, QWidget* parent = nullptr) 
        : QDialog(parent), m_defaultAnchor(screenCenter), m_radiusSpin(nullptr) {
        
        if (parent) {
            QPoint center = parent->geometry().center();
            move(center.x() - 450, center.y() - 530);  
        }

        m_selectedFillColor = Qt::transparent;
        setupUI();
        applyDarkTheme();
        updateVerticesForShape("Прямоугольник");
    }

private slots:
    void onShapeTypeChanged(int index) {
        updateVerticesForShape(m_shapeTypeCombo->currentText());
    }

    void onFillColorClicked() {
        QColor color = QColorDialog::getColor(
            m_selectedFillColor == Qt::transparent ? Qt::white : m_selectedFillColor, 
            this, "Выберите цвет заливки");
        if (color.isValid()) {
            m_selectedFillColor = color;
            m_btnFillColor->setStyleSheet(QString("background-color: %1; color: %2;")
                .arg(color.name())
                .arg(getContrastColor(color).name()));
        }
    }

    void onColorButtonClicked() {
        QPushButton* btn = qobject_cast<QPushButton*>(sender());
        if (!btn) return;
        int index = btn->property("index").toInt();
        
        QColor color = QColorDialog::getColor(m_selectedColors[index], this, 
                                            QString("Выберите цвет для стороны %1").arg(index + 1));
        if (color.isValid()) {
            m_selectedColors[index] = color;
            // Обновляем цвет кнопки с правильным стилем
            btn->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid gray;")
                .arg(color.name())
                .arg(getContrastColor(color).name()));
        }
    }

    void updateVertexCoordinates() {
        QString text = "Координаты вершин: ";
        for (int i = 0; i < m_vertexXEdits.size(); ++i) {
            text += QString("V%1(%2,%3) ")
                .arg(i)
                .arg(m_vertexXEdits[i]->text())
                .arg(m_vertexYEdits[i]->text());
        }
        m_vertexCoordsLabel->setText(text);
    }

private:
    void setupUI() {
        setWindowTitle("Параметры фигуры");
        resize(900, 1000);
        setWindowFlags(Qt::FramelessWindowHint);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        QWidget* titleBar = createTitleBar("Создание новой фигуры");
        mainLayout->addWidget(titleBar);
        
        QWidget* content = new QWidget;
        QVBoxLayout* contentLayout = new QVBoxLayout(content);
        contentLayout->setSpacing(15);
        contentLayout->setContentsMargins(15, 15, 15, 15);
        
        QGroupBox* typeGroup = new QGroupBox("Тип фигуры");
        QHBoxLayout* typeLayout = new QHBoxLayout(typeGroup);
        typeLayout->addWidget(new QLabel("Выберите тип:"));
        m_shapeTypeCombo = new QComboBox;
        m_shapeTypeCombo->addItems({"Прямоугольник", "Треугольник", "Трапеция", 
                                     "Окружность", "Пятиугольник"});
        m_shapeTypeCombo->setCurrentIndex(0);
        connect(m_shapeTypeCombo, SIGNAL(currentIndexChanged(int)), 
                this, SLOT(onShapeTypeChanged(int)));
        typeLayout->addWidget(m_shapeTypeCombo);
        typeLayout->addStretch();
        contentLayout->addWidget(typeGroup);
        
        QGroupBox* anchorGroup = new QGroupBox("Точка привязки (абсолютные координаты)");
        QHBoxLayout* anchorLayout = new QHBoxLayout(anchorGroup);
        anchorLayout->addWidget(new QLabel("X:"));
        m_anchorX = new QSpinBox;
        m_anchorX->setRange(0, 2000);
        m_anchorX->setValue(m_defaultAnchor.x);
        anchorLayout->addWidget(m_anchorX);
        anchorLayout->addWidget(new QLabel("Y:"));
        m_anchorY = new QSpinBox;
        m_anchorY->setRange(0, 2000);
        m_anchorY->setValue(m_defaultAnchor.y);
        anchorLayout->addWidget(m_anchorY);
        anchorLayout->addStretch();
        contentLayout->addWidget(anchorGroup);
        
        QGroupBox* fillGroup = new QGroupBox("Цвет заливки");
        QHBoxLayout* fillLayout = new QHBoxLayout(fillGroup);
        fillLayout->addWidget(new QLabel("Цвет:"));
        m_btnFillColor = new QPushButton("Прозрачный");
        m_btnFillColor->setFixedSize(100, 35);
        m_btnFillColor->setStyleSheet("background-color: transparent; color: white;");
        connect(m_btnFillColor, &QPushButton::clicked, this, &ShapeParametersDialog::onFillColorClicked);
        fillLayout->addWidget(m_btnFillColor);
        fillLayout->addStretch();
        contentLayout->addWidget(fillGroup);
        
        QLabel* verticesTitle = new QLabel("Координаты вершин ОТНОСИТЕЛЬНО ТОЧКИ ПРИВЯЗКИ (X →, Y ↑):");
        verticesTitle->setStyleSheet("color: white; font-weight: bold;");
        contentLayout->addWidget(verticesTitle);
        
        m_verticesPanel = new QWidget;
        QVBoxLayout* verticesPanelLayout = new QVBoxLayout(m_verticesPanel);
        verticesPanelLayout->setSpacing(10);
        verticesPanelLayout->addStretch();
        
        QScrollArea* verticesScroll = new QScrollArea;
        verticesScroll->setWidget(m_verticesPanel);
        verticesScroll->setWidgetResizable(true);
        verticesScroll->setFixedHeight(250);
        verticesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        verticesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        contentLayout->addWidget(verticesScroll);
        
        QLabel* sidesTitle = new QLabel("Параметры сторон:");
        sidesTitle->setStyleSheet("color: white; font-weight: bold;");
        contentLayout->addWidget(sidesTitle);
        
        QGroupBox* sidesGroup = new QGroupBox("Настройка каждой стороны");
        m_sidesPanel = new QWidget;
        QVBoxLayout* sidesPanelLayout = new QVBoxLayout(m_sidesPanel);
        sidesPanelLayout->setSpacing(10);
        sidesPanelLayout->addStretch();
        
        QScrollArea* sidesScroll = new QScrollArea;
        sidesScroll->setWidget(m_sidesPanel);
        sidesScroll->setWidgetResizable(true);
        sidesScroll->setFixedHeight(250);
        sidesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        sidesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        QVBoxLayout* sidesGroupLayout = new QVBoxLayout(sidesGroup);
        sidesGroupLayout->addWidget(sidesScroll);
        contentLayout->addWidget(sidesGroup);
        
        m_vertexCoordsLabel = new QLabel("Координаты вершин: не выбраны");
        m_vertexCoordsLabel->setFixedHeight(40);
        m_vertexCoordsLabel->setStyleSheet("color: lightgreen; font-weight: bold; "
                                           "border: 1px solid gray; padding: 5px;");
        contentLayout->addWidget(m_vertexCoordsLabel);
        
        mainLayout->addWidget(content);
        
        QWidget* buttonBar = new QWidget;
        buttonBar->setFixedHeight(60);
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonBar);
        buttonLayout->addStretch();
        QPushButton* okBtn = new QPushButton("OK");
        QPushButton* cancelBtn = new QPushButton("Отмена");
        okBtn->setFixedSize(120, 40);
        cancelBtn->setFixedSize(120, 40);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        mainLayout->addWidget(buttonBar);
        
        styleButton(okBtn);
        styleButton(cancelBtn);
    }

    QWidget* createTitleBar(const QString& title) {
        QWidget* titleBar = new QWidget;
        titleBar->setFixedHeight(40);
        QHBoxLayout* layout = new QHBoxLayout(titleBar);
        layout->setContentsMargins(15, 0, 15, 0);
        
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;");
        layout->addWidget(titleLabel);
        layout->addStretch();
        
        QPushButton* closeBtn = new QPushButton("X");
        closeBtn->setFixedSize(40, 30);
        closeBtn->setStyleSheet("QPushButton { background-color: #46464b; color: white; border: none; }"
                               "QPushButton:hover { background-color: red; }");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        layout->addWidget(closeBtn);
        
        return titleBar;
    }

    void applyDarkTheme() {
        setStyleSheet(
            "QDialog { background-color: #2d2d2d; }"
            "QGroupBox { color: white; border: 1px solid #555; border-radius: 5px; margin-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
            "QLabel { color: white; }"
            "QComboBox { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; }"
            "QSpinBox { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; }"
            "QLineEdit { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; }"
            "QPushButton { background-color: #46464b; color: white; border: none; padding: 5px; }"
            "QPushButton:hover { background-color: #5a5a60; }"
            "QScrollArea { background-color: #2d2d2d; border: none; }"
            "QWidget { background-color: #2d2d2d; }"
        );
    }

    void styleButton(QPushButton* btn) {
        btn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; font-weight: bold; border: none; }"
            "QPushButton:hover { background-color: #5a9acf; }"
            "QPushButton:pressed { background-color: #2a5f8a; }"
        );
    }

    void updateVerticesForShape(const QString& shapeType) {
        // Полностью удаляем старый layout
        QLayout* oldLayout = m_verticesPanel->layout();
        if (oldLayout) {
            QLayoutItem* item;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    delete item->widget();
                }
                delete item;
            }
        }
        
        QVBoxLayout* verticesLayout = qobject_cast<QVBoxLayout*>(m_verticesPanel->layout());
        if (!verticesLayout) {
            verticesLayout = new QVBoxLayout(m_verticesPanel);
            verticesLayout->setSpacing(10);
        }
        
        // Очищаем списки редакторов вершин
        m_vertexXEdits.clear();
        m_vertexYEdits.clear();
        
        // Удаляем старый radiusSpin если он был
        if (m_radiusSpin) {
            delete m_radiusSpin;
            m_radiusSpin = nullptr;
        }
        
        if (shapeType == "Окружность") {
            QHBoxLayout* radiusLayout = new QHBoxLayout;
            radiusLayout->addWidget(new QLabel("Радиус:"));
            m_radiusSpin = new QSpinBox;
            m_radiusSpin->setRange(20, 600);
            m_radiusSpin->setValue(140);
            m_radiusSpin->setFixedWidth(100);
            radiusLayout->addWidget(m_radiusSpin);
            radiusLayout->addStretch();
            verticesLayout->addLayout(radiusLayout);
            
            // Устанавливаем количество сторон для окружности
            buildSideControls(1);
        } else {
            // Для многоугольников создаем поля для вершин
            int vertexCount = 0;
            std::vector<Point> defaultVertices;
            
            if (shapeType == "Прямоугольник") {
                vertexCount = 4;
                defaultVertices = {Point(0,0), Point(280,0), Point(280,200), Point(0,200)};
            } else if (shapeType == "Треугольник") {
                vertexCount = 3;
                defaultVertices = {Point(0,0), Point(320,0), Point(160,277)};
            } else if (shapeType == "Трапеция") {
                vertexCount = 4;
                defaultVertices = {Point(0,0), Point(280,0), Point(240,160), Point(40,160)};
            } else if (shapeType == "Пятиугольник") {
                vertexCount = 5;
                defaultVertices = {Point(0,0), Point(160,0), Point(209,152), 
                                  Point(80,246), Point(-49,152)};
            }
            
            // Создаем поля для каждой вершины
            for (int i = 0; i < vertexCount; ++i) {
                QWidget* row = new QWidget;
                row->setFixedHeight(40);
                QHBoxLayout* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(5, 0, 5, 0);
                rowLayout->setSpacing(10);
                
                QLabel* label = new QLabel(QString("Вершина %1:").arg(i));
                label->setFixedWidth(80);
                label->setStyleSheet("color: white;");
                rowLayout->addWidget(label);
                
                rowLayout->addWidget(new QLabel("X:"));
                QLineEdit* xEdit = new QLineEdit(QString::number(defaultVertices[i].x));
                xEdit->setFixedWidth(80);
                connect(xEdit, &QLineEdit::textChanged, this, &ShapeParametersDialog::updateVertexCoordinates);
                rowLayout->addWidget(xEdit);
                m_vertexXEdits.append(xEdit);
                
                rowLayout->addWidget(new QLabel("Y:"));
                QLineEdit* yEdit = new QLineEdit(QString::number(defaultVertices[i].y));
                yEdit->setFixedWidth(80);
                connect(yEdit, &QLineEdit::textChanged, this, &ShapeParametersDialog::updateVertexCoordinates);
                rowLayout->addWidget(yEdit);
                m_vertexYEdits.append(yEdit);
                
                rowLayout->addStretch();
                verticesLayout->addWidget(row);
            }
            
            // Устанавливаем количество сторон для многоугольника
            buildSideControls(vertexCount);
        }
        
        verticesLayout->addStretch();
        updateVertexCoordinates();
    }

    void buildSideControls(int sides) {
        QLayout* oldLayout = m_sidesPanel->layout();
        if (oldLayout) {
            QLayoutItem* item;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    delete item->widget();
                }
                delete item;
            }
        }
        
        QVBoxLayout* sidesLayout = qobject_cast<QVBoxLayout*>(m_sidesPanel->layout());
        if (!sidesLayout) {
            sidesLayout = new QVBoxLayout(m_sidesPanel);
            sidesLayout->setSpacing(10);
        }
        
        m_colorButtons.clear();
        m_selectedColors.clear();
        m_selectedWidths.clear();
        
        for (int i = 0; i < sides; ++i) {
            m_selectedColors.append(Qt::black);
            m_selectedWidths.append(3);
        }
        
        for (int i = 0; i < sides; ++i) {
            QWidget* row = new QWidget;
            row->setFixedHeight(45);
            QHBoxLayout* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(5, 0, 5, 0);
            rowLayout->setSpacing(10);
            
            QLabel* label = new QLabel(sides == 1 ? "Контур:" : QString("Сторона %1:").arg(i + 1));
            label->setFixedWidth(80);
            label->setStyleSheet("font-weight: bold; color: white;");
            rowLayout->addWidget(label);
            
            rowLayout->addWidget(new QLabel("Цвет:"));
            QPushButton* colorBtn = new QPushButton;
            colorBtn->setFixedSize(90, 35);
            colorBtn->setProperty("index", i);
            // Устанавливаем начальный цвет кнопки
            colorBtn->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid gray; border-radius: 3px;")
                .arg(m_selectedColors[i].name())
                .arg(getContrastColor(m_selectedColors[i]).name()));
            connect(colorBtn, &QPushButton::clicked, this, &ShapeParametersDialog::onColorButtonClicked);
            rowLayout->addWidget(colorBtn);
            m_colorButtons.push_back(colorBtn);
            
            rowLayout->addWidget(new QLabel("Толщина:"));
            QSpinBox* widthSpin = new QSpinBox;
            widthSpin->setRange(1, 50);
            widthSpin->setValue(3);
            widthSpin->setFixedWidth(80);
            connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                    [this, i](int val) { m_selectedWidths[i] = val; });
            rowLayout->addWidget(widthSpin);
            
            rowLayout->addStretch();
            sidesLayout->addWidget(row);
        }
        
        sidesLayout->addStretch();
    }
};

// ==================== EditShapeDialog ====================
class EditShapeDialog : public QDialog {
    Q_OBJECT

private:
    Shape* m_targetShape;
    std::vector<QColor> m_selectedColors;
    std::vector<float> m_selectedWidths;
    QColor m_selectedFillColor;
    QSpinBox* m_anchorX;
    QSpinBox* m_anchorY;
    QPushButton* m_btnFillColor;
    QList<QLineEdit*> m_vertexXEdits;
    QList<QLineEdit*> m_vertexYEdits;
    QSpinBox* m_radiusSpin;
    QLabel* m_vertexCoordsLabel;
    QLineEdit* m_nameEdit;

public:
    EditShapeDialog(Shape* shape, QWidget* parent = nullptr) 
        : QDialog(parent), m_targetShape(shape) {

        if (parent) {
            QPoint center = parent->geometry().center();
            move(center.x() - 450, center.y() - 500);  
        }

        m_selectedColors = shape->sideColors();
        m_selectedWidths = shape->sideWidths();
        m_selectedFillColor = shape->fillColor();
        setupUI();
        applyDarkTheme();
    }

private slots:
    void onFillColorClicked() {
        QColor color = QColorDialog::getColor(
            m_selectedFillColor == Qt::transparent ? Qt::white : m_selectedFillColor, 
            this, "Выберите цвет заливки");
        if (color.isValid()) {
            m_selectedFillColor = color;
            m_btnFillColor->setStyleSheet(QString("background-color: %1; color: %2;")
                .arg(color.name())
                .arg(getContrastColor(color).name()));
        }
    }

    void onOkClicked() {
        // Обновляем имя
        if (!m_nameEdit->text().isEmpty()) {
            m_targetShape->setDisplayName(m_nameEdit->text());
        }
        
        // Обновляем свойства
        m_targetShape->updateProperties(m_selectedColors, m_selectedWidths, m_selectedFillColor);
        
        // Обновляем абсолютную точку привязки
        m_targetShape->setAnchorPoint(Point(m_anchorX->value(), m_anchorY->value()));
        
        if (auto* circle = dynamic_cast<CircleShape*>(m_targetShape)) {
            if (m_radiusSpin) circle->setRadius(m_radiusSpin->value());
        } else {
            std::vector<Point> newVertices;
            
            // Все вершины задаются относительно точки привязки
            for (int i = 0; i < m_vertexXEdits.size(); ++i) {
                bool xOk, yOk;
                int x = m_vertexXEdits[i]->text().toInt(&xOk);
                int y = m_vertexYEdits[i]->text().toInt(&yOk);
                
                if (xOk && yOk) {
                    newVertices.push_back(Point(x, y));
                } else {
                    // Если данные некорректны, используем оригинальные значения
                    auto originalVertices = m_targetShape->getRelativeVertices();
                    if (i < (int)originalVertices.size()) {
                        newVertices.push_back(originalVertices[i]);
                    } else {
                        newVertices.push_back(Point(100, 100));
                    }
                }
            }
            
            m_targetShape->setRelativeVertices(newVertices);
        }
        accept();
    }

    void updateVertexCoordinates() {
        QString text = "Координаты вершин: ";
        for (int i = 0; i < m_vertexXEdits.size(); ++i) {
            text += QString("V%1(%2,%3) ")
                .arg(i)
                .arg(m_vertexXEdits[i]->text())
                .arg(m_vertexYEdits[i]->text());
        }
        m_vertexCoordsLabel->setText(text);
    }

private:
    void setupUI() {
        setWindowTitle(QString("Редактирование - %1").arg(m_targetShape->shapeName()));
        resize(900, 1100);
        setWindowFlags(Qt::FramelessWindowHint);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        QWidget* titleBar = createTitleBar(QString("Редактирование - %1").arg(m_targetShape->shapeName()));
        mainLayout->addWidget(titleBar);
        
        QWidget* content = new QWidget;
        QVBoxLayout* contentLayout = new QVBoxLayout(content);
        contentLayout->setSpacing(15);
        contentLayout->setContentsMargins(15, 15, 15, 15);
        
        QGroupBox* nameGroup = new QGroupBox("Имя фигуры");
        QHBoxLayout* nameLayout = new QHBoxLayout(nameGroup);
        nameLayout->addWidget(new QLabel("Имя:"));
        m_nameEdit = new QLineEdit;
        m_nameEdit->setText(m_targetShape->displayName().isEmpty() ? 
                           QString("%1_%2").arg(m_targetShape->shapeName()).arg(rand() % 1000) : 
                           m_targetShape->displayName());
        m_nameEdit->setFixedWidth(300);
        nameLayout->addWidget(m_nameEdit);
        nameLayout->addStretch();
        contentLayout->addWidget(nameGroup);
        
        QGroupBox* anchorGroup = new QGroupBox("Точка привязки (абсолютные координаты)");
        QHBoxLayout* anchorLayout = new QHBoxLayout(anchorGroup);
        anchorLayout->addWidget(new QLabel("X:"));
        m_anchorX = new QSpinBox;
        m_anchorX->setRange(0, 2000);
        m_anchorX->setValue(m_targetShape->anchorPoint().x);
        anchorLayout->addWidget(m_anchorX);
        anchorLayout->addWidget(new QLabel("Y:"));
        m_anchorY = new QSpinBox;
        m_anchorY->setRange(0, 2000);
        m_anchorY->setValue(m_targetShape->anchorPoint().y);
        anchorLayout->addWidget(m_anchorY);
        anchorLayout->addStretch();
        contentLayout->addWidget(anchorGroup);
        
        QGroupBox* fillGroup = new QGroupBox("Цвет заливки");
        QHBoxLayout* fillLayout = new QHBoxLayout(fillGroup);
        fillLayout->addWidget(new QLabel("Цвет:"));
        m_btnFillColor = new QPushButton;
        m_btnFillColor->setFixedSize(100, 35);
        m_btnFillColor->setStyleSheet(QString("background-color: %1; color: %2;")
            .arg(m_selectedFillColor.name())
            .arg(getContrastColor(m_selectedFillColor).name()));
        connect(m_btnFillColor, &QPushButton::clicked, this, &EditShapeDialog::onFillColorClicked);
        fillLayout->addWidget(m_btnFillColor);
        fillLayout->addStretch();
        contentLayout->addWidget(fillGroup);
        
        QLabel* vertexCountLabel = new QLabel(
            QString("Всего вершин: %1 (координаты указаны ОТНОСИТЕЛЬНО ТОЧКИ ПРИВЯЗКИ)")
            .arg(m_targetShape->getRelativeVertices().size()));
        vertexCountLabel->setStyleSheet("color: lightblue; font-weight: bold; padding: 5px;");
        contentLayout->addWidget(vertexCountLabel);
        
        QLabel* verticesTitle = new QLabel("Координаты вершин ОТНОСИТЕЛЬНО ТОЧКИ ПРИВЯЗКИ (X →, Y ↑):");
        verticesTitle->setStyleSheet("color: white; font-weight: bold;");
        contentLayout->addWidget(verticesTitle);
        
        QWidget* verticesPanel = new QWidget;
        QVBoxLayout* verticesPanelLayout = new QVBoxLayout(verticesPanel);
        verticesPanelLayout->setSpacing(10);
        verticesPanelLayout->addStretch();
        
        QScrollArea* verticesScroll = new QScrollArea;
        verticesScroll->setWidget(verticesPanel);
        verticesScroll->setWidgetResizable(true);
        verticesScroll->setFixedHeight(250);
        verticesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        verticesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        contentLayout->addWidget(verticesScroll);
        
        QGroupBox* sidesGroup = new QGroupBox("Параметры сторон");
        QWidget* sidesPanel = new QWidget;
        QVBoxLayout* sidesPanelLayout = new QVBoxLayout(sidesPanel);
        sidesPanelLayout->setSpacing(10);
        sidesPanelLayout->addStretch();
        
        QScrollArea* sidesScroll = new QScrollArea;
        sidesScroll->setWidget(sidesPanel);
        sidesScroll->setWidgetResizable(true);
        sidesScroll->setFixedHeight(250);
        sidesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        sidesScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        QVBoxLayout* sidesGroupLayout = new QVBoxLayout(sidesGroup);
        sidesGroupLayout->addWidget(sidesScroll);
        contentLayout->addWidget(sidesGroup);
        
        m_vertexCoordsLabel = new QLabel("Координаты вершин: обновляются при изменении");
        m_vertexCoordsLabel->setFixedHeight(40);
        m_vertexCoordsLabel->setStyleSheet("color: lightgreen; font-weight: bold; "
                                           "border: 1px solid gray; padding: 5px;");
        contentLayout->addWidget(m_vertexCoordsLabel);
        
        mainLayout->addWidget(content);
        
        QWidget* buttonBar = new QWidget;
        buttonBar->setFixedHeight(60);
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonBar);
        buttonLayout->addStretch();
        QPushButton* okBtn = new QPushButton("OK");
        QPushButton* cancelBtn = new QPushButton("Отмена");
        okBtn->setFixedSize(120, 40);
        cancelBtn->setFixedSize(120, 40);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, &EditShapeDialog::onOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(okBtn);
        buttonLayout->addWidget(cancelBtn);
        mainLayout->addWidget(buttonBar);
        
        styleButton(okBtn);
        styleButton(cancelBtn);
        
        buildVerticesControls(verticesPanel);
        buildSidesControls(sidesPanel);
        applyDarkTheme();
    }

    QWidget* createTitleBar(const QString& title) {
        QWidget* titleBar = new QWidget;
        titleBar->setFixedHeight(40);
        QHBoxLayout* layout = new QHBoxLayout(titleBar);
        layout->setContentsMargins(15, 0, 15, 0);
        
        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 12pt; font-weight: bold; color: white;");
        layout->addWidget(titleLabel);
        layout->addStretch();
        
        QPushButton* closeBtn = new QPushButton("X");
        closeBtn->setFixedSize(40, 30);
        closeBtn->setStyleSheet("QPushButton { background-color: #46464b; color: white; border: none; }"
                               "QPushButton:hover { background-color: red; }");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
        layout->addWidget(closeBtn);
        
        return titleBar;
    }

    void applyDarkTheme() {
        setStyleSheet(
            "QDialog { background-color: #2d2d2d; }"
            "QGroupBox { color: white; border: 1px solid #555; border-radius: 5px; margin-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
            "QLabel { color: white; }"
            "QSpinBox { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; }"
            "QLineEdit { background-color: #46464b; color: white; border: 1px solid #555; padding: 5px; }"
            "QPushButton { background-color: #46464b; color: white; border: none; padding: 5px; }"
            "QPushButton:hover { background-color: #5a5a60; }"
            "QScrollArea { background-color: #2d2d2d; border: none; }"
            "QWidget { background-color: #2d2d2d; }"
        );
    }

    void styleButton(QPushButton* btn) {
        btn->setStyleSheet(
            "QPushButton { background-color: #4682b4; color: white; font-weight: bold; border: none; }"
            "QPushButton:hover { background-color: #5a9acf; }"
            "QPushButton:pressed { background-color: #2a5f8a; }"
        );
    }

    void buildVerticesControls(QWidget* panel) {
        QLayout* oldLayout = panel->layout();
        if (oldLayout) {
            QLayoutItem* item;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    delete item->widget();
                }
                delete item;
            }
        }
        
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(panel->layout());
        if (!layout) {
            layout = new QVBoxLayout(panel);
            layout->setSpacing(10);
        }
        
        // Очищаем списки
        m_vertexXEdits.clear();
        m_vertexYEdits.clear();
        
        if (auto* circle = dynamic_cast<CircleShape*>(m_targetShape)) {
            QLabel* info = new QLabel("Для окружности настройка вершин не требуется. Используйте радиус:");
            info->setStyleSheet("color: white;");
            info->setWordWrap(true);
            layout->addWidget(info);
            
            QHBoxLayout* radiusLayout = new QHBoxLayout;
            radiusLayout->addWidget(new QLabel("Радиус:"));
            m_radiusSpin = new QSpinBox;
            m_radiusSpin->setRange(20, 200);
            m_radiusSpin->setValue(circle->radius());
            m_radiusSpin->setFixedWidth(100);
            radiusLayout->addWidget(m_radiusSpin);
            radiusLayout->addStretch();
            layout->addLayout(radiusLayout);
        } else {
            auto vertices = m_targetShape->getRelativeVertices();
            
            // Все вершины редактируемые, все задаются относительно точки привязки
            for (size_t i = 0; i < vertices.size(); ++i) {
                QWidget* row = new QWidget;
                row->setFixedHeight(40);
                QHBoxLayout* rowLayout = new QHBoxLayout(row);
                rowLayout->setContentsMargins(5, 0, 5, 0);
                rowLayout->setSpacing(10);
                
                QLabel* label = new QLabel(QString("Вершина %1:").arg(i));
                label->setFixedWidth(80);
                label->setStyleSheet("color: white;");
                rowLayout->addWidget(label);
                
                rowLayout->addWidget(new QLabel("X:"));
                QLineEdit* xEdit = new QLineEdit(QString::number(vertices[i].x));
                xEdit->setFixedWidth(80);
                connect(xEdit, &QLineEdit::textChanged, this, &EditShapeDialog::updateVertexCoordinates);
                rowLayout->addWidget(xEdit);
                m_vertexXEdits.append(xEdit);
                
                rowLayout->addWidget(new QLabel("Y:"));
                QLineEdit* yEdit = new QLineEdit(QString::number(vertices[i].y));
                yEdit->setFixedWidth(80);
                connect(yEdit, &QLineEdit::textChanged, this, &EditShapeDialog::updateVertexCoordinates);
                rowLayout->addWidget(yEdit);
                m_vertexYEdits.append(yEdit);
                
                rowLayout->addStretch();
                layout->addWidget(row);
            }
        }
        
        layout->addStretch();
    }

    void buildSidesControls(QWidget* panel) {
        QLayout* oldLayout = panel->layout();
        if (oldLayout) {
            QLayoutItem* item;
            while ((item = oldLayout->takeAt(0)) != nullptr) {
                if (item->widget()) {
                    delete item->widget();
                }
                delete item;
            }
        }
        
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(panel->layout());
        if (!layout) {
            layout = new QVBoxLayout(panel);
            layout->setSpacing(10);
        }
        
        int sides = m_targetShape->sidesCount();
        for (int i = 0; i < sides; ++i) {
            QWidget* row = new QWidget;
            row->setFixedHeight(45);
            QHBoxLayout* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(5, 0, 5, 0);
            rowLayout->setSpacing(10);
            
            QLabel* label = new QLabel(sides == 1 ? "Контур:" : QString("Сторона %1:").arg(i + 1));
            label->setFixedWidth(80);
            label->setStyleSheet("font-weight: bold; color: white;");
            rowLayout->addWidget(label);
            
            rowLayout->addWidget(new QLabel("Цвет:"));
            QPushButton* colorBtn = new QPushButton;
            colorBtn->setFixedSize(90, 35);
            colorBtn->setProperty("index", i);
            colorBtn->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid gray; border-radius: 3px;")
                .arg(m_selectedColors[i].name())
                .arg(getContrastColor(m_selectedColors[i]).name()));
            connect(colorBtn, &QPushButton::clicked, [this, i]() {
                QColor color = QColorDialog::getColor(m_selectedColors[i], this, 
                                                       QString("Выберите цвет для стороны %1").arg(i + 1));
                if (color.isValid()) {
                    m_selectedColors[i] = color;
                    QPushButton* btn = qobject_cast<QPushButton*>(sender());
                    if (btn) {
                        btn->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid gray; border-radius: 3px;")
                            .arg(color.name())
                            .arg(getContrastColor(color).name()));
                    }
                }
            });
            rowLayout->addWidget(colorBtn);
            
            rowLayout->addWidget(new QLabel("Толщина:"));
            QSpinBox* widthSpin = new QSpinBox;
            widthSpin->setRange(1, 50);
            widthSpin->setValue(m_selectedWidths[i]);
            widthSpin->setFixedWidth(80);
            connect(widthSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
                    [this, i](int val) { m_selectedWidths[i] = val; });
            rowLayout->addWidget(widthSpin);
            
            rowLayout->addStretch();
            layout->addWidget(row);
        }
        
        layout->addStretch();
    }
};

// ==================== MainWindow ====================
class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    std::vector<Shape*> m_shapes;
    Shape* m_selectedShape;
    std::vector<Shape*> m_selectedShapes;  // для множественного выделения
    bool m_isMultiSelecting;                // режим множественного выделения
    bool m_isDragging;
    bool m_isDraggingAnchor;
    Point m_dragStart;
    
    // Для режимов создания кастомных фигур
    bool m_isPlacingCustomShape;
    int m_customShapeMode; // 0 - точки, 1 - углы
    std::vector<Point> m_tempAbsolutePoints;
    double m_currentAngle;
    
    // Для обычного режима создания
    bool m_isPlacingShape;
    QString m_pendingShapeType;
    std::vector<QColor> m_pendingColors;
    std::vector<float> m_pendingWidths;
    QColor m_pendingFillColor;
    Point m_pendingAnchorPoint;
    std::vector<Point> m_pendingRelativeVertices;
    int m_pendingRadius;
    
    DoubleBufferedWidget* m_canvas;
    QPushButton* m_createShapeButton;
    QPushButton* m_customShapeButton;
    QPushButton* m_closeShapeButton;
    QStatusBar* m_statusBar;
    QLabel* m_labelAnchor;
    QLabel* m_labelVertexCoords;
    QLabel* m_labelInfo;
    QMenu* m_contextMenu;
    
    // Панель объектов
    QWidget* m_objectPanel;
    QTreeWidget* m_objectTree;
    QMap<Shape*, QTreeWidgetItem*> m_shapeToItem;

public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent),
        m_selectedShape(nullptr), m_selectedShapes(), m_isMultiSelecting(false),
        m_isDragging(false), m_isDraggingAnchor(false), 
        m_isPlacingCustomShape(false), m_customShapeMode(0), m_currentAngle(0),
        m_isPlacingShape(false), m_canvas(nullptr), m_createShapeButton(nullptr),
        m_customShapeButton(nullptr), m_closeShapeButton(nullptr), m_statusBar(nullptr), 
        m_labelAnchor(nullptr), m_labelVertexCoords(nullptr), m_labelInfo(nullptr), 
        m_contextMenu(nullptr), m_objectPanel(nullptr), m_objectTree(nullptr) {
        setupUI();
    }

    ~MainWindow() {
        for (auto* shape : m_shapes) {
            delete shape;
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Delete && !m_selectedShapes.empty()) {
            deleteShape();
            event->accept();
        } else if (event->key() == Qt::Key_Escape) {
            if (m_isPlacingCustomShape) {
                // Отмена построения кастомной фигуры
                m_isPlacingCustomShape = false;
                m_tempAbsolutePoints.clear();
                m_currentAngle = 0;
                if (m_closeShapeButton) m_closeShapeButton->setVisible(false);
                setCursor(Qt::ArrowCursor);
                m_canvas->update();
                updateStatusMessage();
                QMessageBox::information(this, "Отмена", "Построение кастомной фигуры отменено");
            } else if (m_isPlacingShape) {
                m_isPlacingShape = false;
                setCursor(Qt::ArrowCursor);
            } else {
                // Сброс выделения
                m_selectedShapes.clear();
                m_selectedShape = nullptr;
                m_canvas->update();
                updateObjectSelection();
            }
            event->accept();
        }
        QMainWindow::keyPressEvent(event);
    }

private slots:
    void onCreateShape() {
        if (!m_canvas) return;
        
        QPoint screenCenter = QRect(QPoint(), m_canvas->size()).center();
        Point transformedCenter = transformMouseCoordinates(screenCenter);
        
        ShapeParametersDialog dialog(transformedCenter, this);
        if (dialog.exec() == QDialog::Accepted) {
            m_pendingShapeType = dialog.shapeType();
            m_pendingColors = dialog.selectedColors();
            m_pendingWidths = dialog.selectedWidths();
            m_pendingFillColor = dialog.selectedFillColor();
            m_pendingAnchorPoint = dialog.anchorPoint();
            
            if (m_pendingShapeType == "Окружность") {
                m_pendingRadius = dialog.circleRadius();
            } else {
                m_pendingRelativeVertices = dialog.getUserVertices();
            }
            
            // Запрашиваем имя фигуры
            NameInputDialog nameDialog(m_pendingShapeType, this);
            if (nameDialog.exec() != QDialog::Accepted) return;
            QString shapeName = nameDialog.getName();
            
            m_isPlacingShape = true;
            setCursor(Qt::CrossCursor);
            
            // Сохраняем имя для будущей фигуры
            m_pendingShapeName = shapeName;
        }
    }

    void onCreateCustomShape() {
        if (!m_canvas) return;
        
        CustomShapeChoiceDialog choiceDialog(this);
        if (choiceDialog.exec() != QDialog::Accepted) return;
        
        // Запрашиваем имя фигуры
        NameInputDialog nameDialog("Custom", this);
        if (nameDialog.exec() != QDialog::Accepted) return;
        m_pendingShapeName = nameDialog.getName();
        
        m_customShapeMode = (choiceDialog.getSelectedMethod() == CustomShapeChoiceDialog::PointsMethod) ? 0 : 1;
        m_isPlacingCustomShape = true;
        m_tempAbsolutePoints.clear();
        m_currentAngle = 0;
        
        if (m_closeShapeButton) {
            m_closeShapeButton->setVisible(true);
        }
        
        setCursor(Qt::CrossCursor);
        updateStatusMessage();
        m_canvas->update();
    }

    void onCloseShape() {
        if (m_tempAbsolutePoints.size() < 3) {
            QMessageBox::warning(this, "Предупреждение", 
                "Для замыкания фигуры нужно минимум 3 точки");
            return;
        }
        
        // Создаем фигуру с учетом первой точки
        std::vector<Point> relativeVertices;
        Point firstPoint = m_tempAbsolutePoints[0];
        
        // Добавляем все точки, кроме первой, как относительные
        for (size_t i = 1; i < m_tempAbsolutePoints.size(); ++i) {
            relativeVertices.push_back(Point(
                m_tempAbsolutePoints[i].x - firstPoint.x,
                m_tempAbsolutePoints[i].y - firstPoint.y
            ));
        }
        
        // Замыкаем фигуру - добавляем относительную координату первой точки
        relativeVertices.push_back(Point(
            firstPoint.x - firstPoint.x,
            firstPoint.y - firstPoint.y
        ));
        
        std::vector<QColor> colors(relativeVertices.size(), Qt::black);
        std::vector<float> widths(relativeVertices.size(), 3);
        
        Shape* newShape = new CustomPolygonShape(firstPoint, relativeVertices, colors, widths, Qt::transparent);
        newShape->setDisplayName(m_pendingShapeName);
        
        if (newShape && m_canvas) {
            connect(newShape, &Shape::geometryChanged, m_canvas, QOverload<>::of(&QWidget::update));
            m_shapes.push_back(newShape);
            addShapeToTree(newShape);
            m_selectedShape = newShape;
            m_selectedShapes.clear();
            m_selectedShapes.push_back(newShape);
            updateObjectSelection();
        }
        
        // Выходим из режима создания
        m_isPlacingCustomShape = false;
        m_tempAbsolutePoints.clear();
        m_closeShapeButton->setVisible(false);
        setCursor(Qt::ArrowCursor);
        m_canvas->update();
        updateStatusMessage();
    }

    void groupShapes() {
        if (m_selectedShapes.size() < 2) return;
        
        // Запрашиваем имя группы
        GroupNameDialog nameDialog(this);
        if (nameDialog.exec() != QDialog::Accepted) return;
        QString groupName = nameDialog.getName();
        
        // Вычисляем центр группы как среднее арифметическое точек привязки
        int sumX = 0, sumY = 0;
        for (auto* shape : m_selectedShapes) {
            Point anchor = shape->anchorPoint();
            sumX += anchor.x;
            sumY += anchor.y;
        }
        Point groupCenter(sumX / m_selectedShapes.size(), sumY / m_selectedShapes.size());
        
        // Создаем группу (группа будет владеть фигурами)
        GroupShape* group = new GroupShape(groupCenter, m_selectedShapes, groupName, true);
        connect(group, &Shape::geometryChanged, m_canvas, QOverload<>::of(&QWidget::update));
        
        // Удаляем старые фигуры из основного списка и из дерева
        for (auto* shape : m_selectedShapes) {
            auto it = std::find(m_shapes.begin(), m_shapes.end(), shape);
            if (it != m_shapes.end()) {
                m_shapes.erase(it);
            }
            removeShapeFromTree(shape);
        }
        
        // Добавляем группу
        m_shapes.push_back(group);
        addShapeToTree(group);
        
        // Очищаем выделение и выделяем группу
        m_selectedShapes.clear();
        m_selectedShapes.push_back(group);
        m_selectedShape = group;
        updateObjectSelection();
        
        m_canvas->update();
    }

    void ungroupShapes() {
        if (m_selectedShapes.size() != 1) return;
        
        GroupShape* group = dynamic_cast<GroupShape*>(m_selectedShapes[0]);
        if (!group) return;
        
        // Получаем фигуры из группы
        std::vector<Shape*> shapes = group->getGroupedShapes();
        
        // Освобождаем фигуры (чтобы группа их не удалила)
        group->releaseShapes();
        
        // Удаляем группу из списка и из дерева
        auto it = std::find(m_shapes.begin(), m_shapes.end(), group);
        if (it != m_shapes.end()) {
            m_shapes.erase(it);
        }
        removeShapeFromTree(group);
        
        // Добавляем отдельные фигуры обратно
        for (auto* shape : shapes) {
            m_shapes.push_back(shape);
            addShapeToTree(shape);
            // Подключаем сигналы заново
            connect(shape, &Shape::geometryChanged, m_canvas, QOverload<>::of(&QWidget::update));
        }
        
        // Очищаем выделение
        m_selectedShapes.clear();
        m_selectedShape = nullptr;
        updateObjectSelection();
        
        // Удаляем группу (она уже не владеет фигурами)
        group->disconnect();
        delete group;
        
        m_canvas->update();
    }

    void onCanvasMouseClick(QMouseEvent* event) {
        if (!m_canvas) return;
        
        if (event->button() == Qt::RightButton && !m_isPlacingCustomShape && !m_isPlacingShape) {
            Point transformedPoint = transformMouseCoordinates(event->pos());
            
            // Обновляем контекстное меню перед показом
            updateContextMenu();
            
            if (m_contextMenu && !m_contextMenu->isEmpty()) {
                m_contextMenu->popup(event->globalPos());
            }
        }
    }

    void onCanvasMouseDown(QMouseEvent* event) {
        if (!m_canvas) return;
        
        if (event->button() == Qt::LeftButton) {
            Point transformedPoint = transformMouseCoordinates(event->pos());
            
            if (m_isPlacingCustomShape) {
                // Режим создания кастомной фигуры
                if (m_customShapeMode == 0) {
                    // Режим расстановки точек
                    m_tempAbsolutePoints.push_back(transformedPoint);
                    updateStatusMessage();
                    m_canvas->update();
                } else {
                    // Режим длин и углов
                    if (m_tempAbsolutePoints.empty()) {
                        // Первая точка
                        m_tempAbsolutePoints.push_back(transformedPoint);
                        updateStatusMessage();
                        m_canvas->update();
                        
                        // Запрашиваем параметры первой стороны
                        QTimer::singleShot(100, this, [this]() {
                            LengthAngleInputDialog dialog(true, this);
                            if (dialog.exec() == QDialog::Accepted) {
                                double length = dialog.getLength();
                                double angle = dialog.getAngle();
                                
                                Point lastPoint = m_tempAbsolutePoints.back();
                                double rad = angle * M_PI / 180.0;
                                Point newPoint(
                                    lastPoint.x + round(length * cos(rad)),
                                    lastPoint.y + round(length * sin(rad))
                                );
                                
                                m_tempAbsolutePoints.push_back(newPoint);
                                m_currentAngle = angle;
                                m_canvas->update();
                                updateStatusMessage();
                            }
                        });
                    } else {
                        // Запрашиваем параметры следующей стороны
                        LengthAngleInputDialog dialog(false, this);
                        if (dialog.exec() == QDialog::Accepted) {
                            double length = dialog.getLength();
                            double angle = dialog.getAngle();
                            
                            Point lastPoint = m_tempAbsolutePoints.back();
                            Point prevPoint = m_tempAbsolutePoints[m_tempAbsolutePoints.size() - 2];
                            
                            // Вычисляем угол последнего сегмента
                            double lastAngle = atan2(lastPoint.y - prevPoint.y, lastPoint.x - prevPoint.x) * 180.0 / M_PI;
                            
                            // Новый угол = последний угол + заданный угол (против часовой стрелки)
                            double newAngle = lastAngle + angle;
                            double rad = newAngle * M_PI / 180.0;
                            
                            Point newPoint(
                                lastPoint.x + round(length * cos(rad)),
                                lastPoint.y + round(length * sin(rad))
                            );
                            
                            m_tempAbsolutePoints.push_back(newPoint);
                            m_canvas->update();
                            updateStatusMessage();
                        }
                    }
                }
            } else if (m_isPlacingShape) {
                // Обычный режим создания предопределенных фигур
                try {
                    Shape* newShape = nullptr;
                    
                    if (m_pendingShapeType == "Окружность") {
                        newShape = new CircleShape(m_pendingAnchorPoint, m_pendingRadius,
                                                   m_pendingColors[0], m_pendingWidths[0],
                                                   m_pendingFillColor);
                    } else if (m_pendingShapeType == "Прямоугольник") {
                        newShape = new RectangleShape(m_pendingAnchorPoint, 140, 100,
                                                      m_pendingColors, m_pendingWidths,
                                                      m_pendingFillColor);
                        newShape->setRelativeVertices(m_pendingRelativeVertices);
                    } else if (m_pendingShapeType == "Треугольник") {
                        newShape = new TriangleShape(m_pendingAnchorPoint, 160,
                                                     m_pendingColors, m_pendingWidths,
                                                     m_pendingFillColor);
                        newShape->setRelativeVertices(m_pendingRelativeVertices);
                    } else if (m_pendingShapeType == "Трапеция") {
                        newShape = new TrapezoidShape(m_pendingAnchorPoint, 100, 140, 80,
                                                      m_pendingColors, m_pendingWidths,
                                                      m_pendingFillColor);
                        newShape->setRelativeVertices(m_pendingRelativeVertices);
                    } else if (m_pendingShapeType == "Пятиугольник") {
                        newShape = new PentagonShape(m_pendingAnchorPoint, 100,
                                                     m_pendingColors, m_pendingWidths,
                                                     m_pendingFillColor);
                        newShape->setRelativeVertices(m_pendingRelativeVertices);
                    }
                    
                    if (newShape && m_canvas) {
                        newShape->setDisplayName(m_pendingShapeName);
                        connect(newShape, &Shape::geometryChanged, m_canvas, QOverload<>::of(&QWidget::update));
                        m_shapes.push_back(newShape);
                        addShapeToTree(newShape);
                        m_selectedShape = newShape;
                        m_selectedShapes.clear();
                        m_selectedShapes.push_back(newShape);
                        updateObjectSelection();
                    }
                    
                    m_isPlacingShape = false;
                    setCursor(Qt::ArrowCursor);
                    m_canvas->update();
                } catch (const std::exception& ex) {
                    QMessageBox::critical(this, "Ошибка", 
                                          QString("Ошибка при создании фигуры: %1").arg(ex.what()));
                    m_isPlacingShape = false;
                    setCursor(Qt::ArrowCursor);
                }
            } else {
                // Обычный режим выделения и перемещения
                
                // Проверяем, зажат ли Shift
                bool shiftPressed = event->modifiers() & Qt::ShiftModifier;
                
                // Ищем фигуру под курсором
                Shape* clicked = nullptr;
                for (int i = m_shapes.size() - 1; i >= 0; --i) {
                    try {
                        if (i < (int)m_shapes.size() && m_shapes[i] && m_shapes[i]->containsPoint(transformedPoint)) {
                            clicked = m_shapes[i];
                            break;
                        }
                    } catch (...) { continue; }
                }
                
                if (shiftPressed) {
                    // Режим множественного выделения
                    if (clicked) {
                        // Проверяем, не выделена ли уже эта фигура
                        auto it = std::find(m_selectedShapes.begin(), m_selectedShapes.end(), clicked);
                        if (it != m_selectedShapes.end()) {
                            // Если уже выделена - убираем выделение
                            m_selectedShapes.erase(it);
                            if (m_selectedShapes.empty()) {
                                m_selectedShape = nullptr;
                            } else {
                                m_selectedShape = m_selectedShapes.back();
                            }
                        } else {
                            // Добавляем к выделенным
                            m_selectedShapes.push_back(clicked);
                            m_selectedShape = clicked;
                        }
                        m_isMultiSelecting = true;
                        m_canvas->update();
                        updateObjectSelection();
                    }
                } else {
                    // Обычный режим - одиночное выделение
                    
                    // Сначала проверяем, не наведены ли мы на точку привязки выделенной фигуры
                    if (m_selectedShape && m_selectedShape->isPointNearAnchor(transformedPoint, 15)) {
                        m_isDragging = true;
                        m_isDraggingAnchor = true;
                        m_dragStart = transformedPoint;
                        if (m_canvas) m_canvas->update();
                        return;
                    }
                    
                    if (clicked) {
                        // Проверяем, кликнули ли на уже выделенную фигуру в режиме множественного выделения
                        bool alreadySelected = false;
                        for (auto* shape : m_selectedShapes) {
                            if (shape == clicked) {
                                alreadySelected = true;
                                break;
                            }
                        }
                        
                        if (!alreadySelected) {
                            m_selectedShapes.clear();
                            m_selectedShapes.push_back(clicked);
                        }
                        m_selectedShape = clicked;
                        m_isDragging = true;
                        m_isDraggingAnchor = false;
                        m_dragStart = transformedPoint;
                        if (m_canvas) m_canvas->update();
                        updateObjectSelection();
                    } else {
                        if (!m_selectedShapes.empty()) {
                            m_selectedShapes.clear();
                            m_selectedShape = nullptr;
                            if (m_canvas) m_canvas->update();
                            updateObjectSelection();
                        }
                    }
                }
            }
        }
    }

    void onCanvasMouseMove(QMouseEvent* event) {
        if (!m_canvas) return;
        
        if (m_isDragging && !m_selectedShapes.empty()) {
            try {
                Point transformedPoint = transformMouseCoordinates(event->pos());
                
                int dx = transformedPoint.x - m_dragStart.x;
                int dy = transformedPoint.y - m_dragStart.y;
                
                if (m_isDraggingAnchor && m_selectedShape) {
                    // Перемещаем только точку привязки внутри фигуры
                    Point newAnchor(m_selectedShape->anchorPoint().x + dx,
                                   m_selectedShape->anchorPoint().y + dy);
                    
                    if (m_selectedShape->containsPoint(newAnchor)) {
                        m_selectedShape->setAnchorPointInside(newAnchor);
                    }
                } else {
                    // Перемещаем все выделенные фигуры
                    for (auto* shape : m_selectedShapes) {
                        shape->moveShape(Point(dx, dy));
                    }
                }
                
                m_dragStart = transformedPoint;
                m_canvas->update();
            } catch (...) {
                m_isDragging = false;
                m_isDraggingAnchor = false;
            }
        }
        
        // Обновляем отображение для предпросмотра
        if (m_isPlacingCustomShape) {
            m_canvas->update();
        }
    }

    void onCanvasMouseUp(QMouseEvent* event) {
        if (!m_canvas) return;
        
        if (m_isDragging) {
            m_isDragging = false;
            m_isDraggingAnchor = false;
            m_canvas->update();
        }
    }

    void onObjectTreeItemClicked(QTreeWidgetItem* item, int column) {
        if (!item) return;
        
        Shape* shape = item->data(0, Qt::UserRole).value<Shape*>();
        if (!shape) return;
        
        // Выделяем фигуру на холсте
        m_selectedShapes.clear();
        m_selectedShapes.push_back(shape);
        m_selectedShape = shape;
        m_canvas->update();
        
        // Обновляем выделение в дереве
        for (auto* existingItem : m_shapeToItem.values()) {
            existingItem->setSelected(false);
        }
        item->setSelected(true);
    }

    void editShape() {
        if (m_selectedShape) {
            EditShapeDialog dialog(m_selectedShape, this);
            if (dialog.exec() == QDialog::Accepted) {
                if (m_canvas) m_canvas->update();
                // Обновляем имя в дереве
                if (m_shapeToItem.contains(m_selectedShape)) {
                    m_shapeToItem[m_selectedShape]->setText(0, m_selectedShape->displayName());
                }
            }
        }
    }

    void deleteShape() {
        if (!m_selectedShapes.empty()) {
            for (auto* shape : m_selectedShapes) {
                auto it = std::find(m_shapes.begin(), m_shapes.end(), shape);
                if (it != m_shapes.end()) {
                    removeShapeFromTree(shape);
                    delete *it;
                    m_shapes.erase(it);
                }
            }
            m_selectedShapes.clear();
            m_selectedShape = nullptr;
            if (m_canvas) m_canvas->update();
        }
    }

private:
    QString m_pendingShapeName;  // Временное имя для создаваемой фигуры

    void setupUI() {
        setWindowTitle("Графический редактор");
        
        // Создаем центральный виджет с горизонтальным сплиттером
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Создаем панель объектов слева
        m_objectPanel = new QWidget;
        m_objectPanel->setFixedWidth(250);
        m_objectPanel->setStyleSheet("background-color: #525252; border-right: 1px solid #000000;");
        
        QVBoxLayout* panelLayout = new QVBoxLayout(m_objectPanel);
        panelLayout->setContentsMargins(5, 5, 5, 5);
        panelLayout->setSpacing(5);
        
        QLabel* panelTitle = new QLabel("Объекты");
        panelTitle->setStyleSheet("color: white; font-size: 14px; font-weight: bold; padding: 5px;");
        panelTitle->setAlignment(Qt::AlignCenter);
        panelLayout->addWidget(panelTitle);
        
        m_objectTree = new QTreeWidget;
        m_objectTree->setHeaderLabel("Имя фигуры");
        m_objectTree->setStyleSheet(
            "QTreeWidget { background-color: #d6d6d6; color: white; border: 1px solid #2b2b2b; }"
            "QTreeWidget::item { padding: 5px; background-color: #56bea4; border: 1px solid #000000;}"
            "QTreeWidget::item:selected { background-color: #4682b4; }"
            "QTreeWidget::item:hover { background-color: #000000; }"
        );
        connect(m_objectTree, &QTreeWidget::itemClicked, this, &MainWindow::onObjectTreeItemClicked);
        panelLayout->addWidget(m_objectTree);
        
        mainLayout->addWidget(m_objectPanel);
        
        // Создаем контейнер для холста и тулбара
        QWidget* canvasContainer = new QWidget;
        QVBoxLayout* canvasLayout = new QVBoxLayout(canvasContainer);
        canvasLayout->setContentsMargins(0, 0, 0, 0);
        canvasLayout->setSpacing(0);
        
        QWidget* toolbar = createToolbar();
        if (toolbar) canvasLayout->addWidget(toolbar);
        
        m_canvas = new DoubleBufferedWidget;
        if (m_canvas) {
            m_canvas->setStyleSheet("background-color: white;");
            canvasLayout->addWidget(m_canvas);
            m_canvas->installEventFilter(this);
        }
        
        mainLayout->addWidget(canvasContainer, 1); // 1 - растягивается
        
        setupStatusBar();
        setupContextMenu();
        
        // Кнопка замыкания фигуры (изначально скрыта)
        m_closeShapeButton = new QPushButton("Замкнуть фигуру", this);
        m_closeShapeButton->setGeometry(270, 70, 150, 40);
        m_closeShapeButton->setStyleSheet(
            "QPushButton { background-color: #b45f5f; color: white; font-weight: bold; border-radius: 5px; }"
            "QPushButton:hover { background-color: #c46f6f; }"
        );
        m_closeShapeButton->setVisible(false);
        connect(m_closeShapeButton, &QPushButton::clicked, this, &MainWindow::onCloseShape);
        
        // Устанавливаем полноэкранный режим
        setWindowFlags(Qt::FramelessWindowHint);
        showFullScreen();
    }
    
    QWidget* createToolbar() {
        QWidget* toolbar = new QWidget(this);
        if (!toolbar) return nullptr;
        
        toolbar->setFixedHeight(60);
        toolbar->setStyleSheet("background-color: #37373c;");
        
        QHBoxLayout* layout = new QHBoxLayout(toolbar);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->setSpacing(10);
        
        m_createShapeButton = new QPushButton("Создать фигуру", toolbar);
        if (m_createShapeButton) {
            m_createShapeButton->setFixedSize(200, 50);
            m_createShapeButton->setStyleSheet(
                "QPushButton { background-color: #f0f0f0; color: black; border: 2px solid #46464b; border-radius: 5px; margin: 0px; }"
                "QPushButton:hover { background-color: #dcdcdc; }"
            );
            connect(m_createShapeButton, &QPushButton::clicked, this, &MainWindow::onCreateShape);
            layout->addWidget(m_createShapeButton);
        }
        
        m_customShapeButton = new QPushButton("Кастомная фигура", toolbar);
        if (m_customShapeButton) {
            m_customShapeButton->setFixedSize(200, 50);
            m_customShapeButton->setStyleSheet(
                "QPushButton { background-color: #f0f0f0; border: 2px solid #46464b; border-radius: 5px; margin: 0px; }"
                "QPushButton:hover { background-color: #6a6a9a; }"
            );
            connect(m_customShapeButton, &QPushButton::clicked, this, &MainWindow::onCreateCustomShape);
            layout->addWidget(m_customShapeButton);
        }
        
        layout->addStretch();
        
        QPushButton* closeBtn = new QPushButton("X", toolbar);
        if (closeBtn) {
            closeBtn->setFixedSize(35, 30);
            closeBtn->setStyleSheet(
                "QPushButton { background-color: #46464b; color: white; border: none; border-radius: 5px; margin: 0px; }"
                "QPushButton:hover { background-color: red; }"
            );
            connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
            layout->addWidget(closeBtn);
        }
        
        return toolbar;
    }

    void setupStatusBar() {
        m_statusBar = new QStatusBar(this);
        setStatusBar(m_statusBar);
        m_statusBar->setStyleSheet("background-color: #37373c; color: white;");
        m_statusBar->setFixedHeight(40);
        
        m_labelAnchor = new QLabel("Точка привязки: не выбрана");
        m_labelVertexCoords = new QLabel("");
        m_labelInfo = new QLabel("");
        
        m_statusBar->addWidget(m_labelAnchor, 1);
        m_statusBar->addWidget(m_labelVertexCoords, 1);
        m_statusBar->addWidget(m_labelInfo, 1);
    }

    void updateContextMenu() {
        if (!m_contextMenu) return;
        
        m_contextMenu->clear();
        
        // Проверяем, выделена ли группа
        bool isGroup = false;
        if (m_selectedShapes.size() == 1) {
            if (dynamic_cast<GroupShape*>(m_selectedShapes[0])) {
                isGroup = true;
            }
        }
        
        if (m_selectedShapes.size() > 1 && !isGroup) {
            // Несколько фигур выделено - показываем "Сгруппировать"
            QAction* groupAction = new QAction("Сгруппировать", this);
            connect(groupAction, &QAction::triggered, this, &MainWindow::groupShapes);
            m_contextMenu->addAction(groupAction);
            m_contextMenu->addSeparator();
        } else if (isGroup) {
            // Выделена группа - показываем "Разгруппировать"
            QAction* ungroupAction = new QAction("Разгруппировать", this);
            connect(ungroupAction, &QAction::triggered, this, &MainWindow::ungroupShapes);
            m_contextMenu->addAction(ungroupAction);
            m_contextMenu->addSeparator();
        }
        
        if (!m_selectedShapes.empty()) {
            QAction* editAction = new QAction("Редактировать", this);
            connect(editAction, &QAction::triggered, this, &MainWindow::editShape);
            m_contextMenu->addAction(editAction);
            
            m_contextMenu->addSeparator();
            
            QAction* deleteAction = new QAction("Удалить", this);
            connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteShape);
            m_contextMenu->addAction(deleteAction);
        }
    }

    void setupContextMenu() {
        m_contextMenu = new QMenu(this);
        if (!m_contextMenu) return;
        
        m_contextMenu->setStyleSheet(
            "QMenu { background-color: #2d2d30; color: white; border: 1px solid #46464b; }"
            "QMenu::item { padding: 5px 20px; }"
            "QMenu::item:selected { background-color: #4682b4; }"
            "QMenu::separator { height: 1px; background-color: #46464b; margin: 5px 0; }"
        );
    }

    void updateStatusMessage() {
        if (m_isPlacingCustomShape) {
            QString mode = (m_customShapeMode == 0) ? "расстановка точек" : "длины и углы";
            QString msg = QString("Режим: %1 | Точек: %2").arg(mode).arg(m_tempAbsolutePoints.size());
            if (m_tempAbsolutePoints.size() >= 3) {
                msg += " - можно замкнуть фигуру (Esc - отмена)";
            } else {
                msg += " (Esc - отмена)";
            }
            m_labelInfo->setText(msg);
        } else {
            m_labelInfo->setText("");
        }
    }

    void addShapeToTree(Shape* shape) {
        if (!shape || !m_objectTree) return;
        
        QTreeWidgetItem* item = new QTreeWidgetItem;
        QString displayName = shape->displayName();
        if (displayName.isEmpty()) {
            displayName = QString("%1_%2").arg(shape->shapeName()).arg(rand() % 1000);
            shape->setDisplayName(displayName);
        }
        item->setText(0, displayName);
        item->setData(0, Qt::UserRole, QVariant::fromValue(shape));
        
        // Если это группа, добавляем дочерние элементы
        if (auto* group = dynamic_cast<GroupShape*>(shape)) {
            for (auto* childShape : group->getGroupedShapes()) {
                QTreeWidgetItem* childItem = new QTreeWidgetItem;
                childItem->setText(0, childShape->displayName());
                childItem->setData(0, Qt::UserRole, QVariant::fromValue(childShape));
                item->addChild(childItem);
                m_shapeToItem[childShape] = childItem;
            }
            item->setExpanded(true);
        }
        
        m_objectTree->addTopLevelItem(item);
        m_shapeToItem[shape] = item;
    }

    void removeShapeFromTree(Shape* shape) {
        if (!shape || !m_objectTree || !m_shapeToItem.contains(shape)) return;
        
        QTreeWidgetItem* item = m_shapeToItem[shape];
        
        // Если это группа, удаляем все дочерние элементы из map
        if (auto* group = dynamic_cast<GroupShape*>(shape)) {
            for (auto* childShape : group->getGroupedShapes()) {
                m_shapeToItem.remove(childShape);
            }
        }
        
        delete item;
        m_shapeToItem.remove(shape);
    }

    void updateObjectSelection() {
        if (!m_objectTree) return;
        
        // Снимаем выделение со всех элементов
        m_objectTree->clearSelection();
        
        // Выделяем элементы соответствующие выделенным фигурам
        for (auto* shape : m_selectedShapes) {
            if (m_shapeToItem.contains(shape)) {
                m_shapeToItem[shape]->setSelected(true);
            }
        }
    }

    bool eventFilter(QObject* obj, QEvent* event) override {
        if (obj == m_canvas && m_canvas) {
            if (event->type() == QEvent::Paint) {
                QPainter painter(m_canvas);
                painter.setRenderHint(QPainter::Antialiasing);
                
                painter.translate(0, m_canvas->height());
                painter.scale(1, -1);
                
                painter.fillRect(QRect(0, 0, m_canvas->width(), m_canvas->height()), Qt::white);
                
                // Рисуем существующие фигуры
                for (auto* shape : m_shapes) {
                    try {
                        if (shape) shape->draw(painter);
                    } catch (...) { continue; }
                }
                
                // Рисуем выделенные фигуры
                if (!m_selectedShapes.empty()) {
                    for (auto* shape : m_selectedShapes) {
                        try {
                            // Рисуем ограничивающий прямоугольник для каждой выделенной фигуры
                            Rectangle bounds = shape->boundingBox();
                            QPen pen(QColor(0, 120, 0, 240));  // Зеленый для множественного выделения
                            pen.setWidth(2);
                            pen.setStyle(Qt::DashLine);
                            painter.setPen(pen);
                            painter.setBrush(Qt::NoBrush);
                            painter.drawRect(bounds.x, bounds.y, bounds.width, bounds.height);
                        } catch (...) {}
                    }
                }
                
                // Рисуем точку привязки для основной выделенной фигуры
                if (m_selectedShape) {
                    try {
                        Point anchor = m_selectedShape->anchorPoint();
                        painter.setBrush(Qt::red);
                        painter.setPen(Qt::NoPen);
                        painter.drawEllipse(QPoint(anchor.x, anchor.y), 5, 5);
                    } catch (...) {}
                }
                
                // Рисуем временные точки для кастомной фигуры
                if (m_isPlacingCustomShape && !m_tempAbsolutePoints.empty()) {
                    painter.setPen(QPen(Qt::blue, 2));
                    painter.setBrush(Qt::blue);
                    
                    // Рисуем точки
                    for (size_t i = 0; i < m_tempAbsolutePoints.size(); ++i) {
                        const Point& p = m_tempAbsolutePoints[i];
                        painter.drawEllipse(QPoint(p.x, p.y), 5, 5);
                        
                        // Сохраняем текущие трансформации
                        painter.save();
                        
                        // Отменяем инверсию для текста
                        painter.resetTransform();
                        // Преобразуем координаты точки обратно в экранные координаты
                        QPoint screenPoint(p.x, m_canvas->height() - p.y);
                        painter.setPen(Qt::black);
                        painter.drawText(screenPoint + QPoint(10, -10), QString::number(i + 1));
                        
                        // Восстанавливаем трансформации
                        painter.restore();
                        
                        painter.setPen(QPen(Qt::blue, 2));
                        painter.setBrush(Qt::blue);
                    }
                    
                    // Рисуем линии между точками (с инвертированными координатами)
                    painter.setPen(QPen(Qt::blue, 2, Qt::DashLine));
                    for (size_t i = 0; i < m_tempAbsolutePoints.size() - 1; ++i) {
                        painter.drawLine(
                            m_tempAbsolutePoints[i].x, m_tempAbsolutePoints[i].y,
                            m_tempAbsolutePoints[i + 1].x, m_tempAbsolutePoints[i + 1].y
                        );
                    }
                }
                
                updateStatusMessage();
                return true;
            } else if (event->type() == QEvent::MouseButtonPress) {
                onCanvasMouseDown(static_cast<QMouseEvent*>(event));
                return true;
            } else if (event->type() == QEvent::MouseMove) {
                onCanvasMouseMove(static_cast<QMouseEvent*>(event));
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease) {
                onCanvasMouseUp(static_cast<QMouseEvent*>(event));
                return true;
            } else if (event->type() == QEvent::MouseButtonDblClick) {
                onCanvasMouseClick(static_cast<QMouseEvent*>(event));
                return true;
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }

    Point transformMouseCoordinates(const QPoint& mousePoint) const {
        if (!m_canvas) return Point(0, 0);
        return Point(mousePoint.x(), m_canvas->height() - mousePoint.y());
    }
};

// ==================== Точка входа ====================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    try {
        MainWindow window;
        window.show();
        return app.exec();
    } catch (const std::exception& ex) {
        QMessageBox::critical(nullptr, "Ошибка", 
                              QString("Произошла ошибка: %1").arg(ex.what()));
        return 1;
    }
}

#include "ShapeEditor.moc"
