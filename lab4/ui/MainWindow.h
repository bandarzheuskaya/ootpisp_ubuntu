#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QGroupBox>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPoint>
#include <QColor>

#include <vector>
#include <memory>

#include "Point.h"
#include "Shape.h"
#include "PolygonShape.h"
#include "EllipseShape.h"
#include "GroupShape.h"
#include "EditorJson.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QDoubleSpinBox* m_rotationSpin = nullptr;
    QWidget* m_verticesGroup = nullptr;
    QPushButton* m_groupBtn = nullptr;
    QPushButton* m_ungroupBtn = nullptr;
    QPushButton* m_addToGroupBtn = nullptr;
    QWidget* m_canvas = nullptr;
    QWidget* m_propertiesPanel = nullptr;
    QWidget* m_leftPanel = nullptr;
    QWidget* m_shapesDock = nullptr;

    QWidget* m_rotationGroup = nullptr;

    QStatusBar* m_statusBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    QPushButton* m_togglePropertiesBtn = nullptr;
    QPushButton* m_toggleLeftBtn = nullptr;
    QPushButton* m_toggleShapesListBtn = nullptr;

    QListWidget* m_allShapesList = nullptr;

    QColor m_pageColor = Qt::white;

        Shape* m_propertiesShape = nullptr;

    QListWidget* m_groupChildrenList = nullptr;
    QPushButton* m_removeChildBtn = nullptr;
    QGroupBox* m_groupChildrenBox = nullptr;

    std::vector<std::unique_ptr<Shape>> m_shapes;

    std::vector<Shape*> m_selectedShapes;
    Shape* m_selectedShape = nullptr;

    int m_highlightVertexIndex = -1;
    Shape* m_highlightVertexShape = nullptr;

    int m_highlightAngleIndex = -1;
    Shape* m_highlightAngleShape = nullptr;

    int m_highlightSideIndex = -1;
    Shape* m_highlightShape = nullptr;

    bool m_dragging = false;
    bool m_dragAnchor = false;
    bool m_resizing = false;
    int m_resizeHandleIndex = -1;
    Point m_lastMouse{0.0, 0.0};
    Rectangle m_resizeStartBounds{};
    Point m_resizeStartMouse{0.0, 0.0};

    bool m_draggingEllipseFocus = false;
EllipseShape* m_dragFocusEllipse = nullptr;
int m_dragFocusIndex = -1;

    Point m_viewOffset{200.0, 120.0};
    bool m_panningCanvas = false;
    Point m_panStartMouse{0.0, 0.0};

    double m_pageWidth = 3200.0;
    double m_pageHeight = 2200.0;

    double m_viewScale = 1.0;
    double m_minViewScale = 0.2;
    double m_maxViewScale = 5.0;

    bool m_updatingUi = false;

    QLabel* m_boundsLabel = nullptr;

    QSpinBox* m_anchorX = nullptr;
    QSpinBox* m_anchorY = nullptr;

    QSpinBox* m_anchorInsideX = nullptr;
    QSpinBox* m_anchorInsideY = nullptr;

    QDoubleSpinBox* m_scaleSpin = nullptr;

    QCheckBox* m_noFillCheck = nullptr;
    QPushButton* m_fillButton = nullptr;
    QColor m_pendingFill = Qt::transparent;

    QTableWidget* m_verticesTable = nullptr;
    QPushButton* m_applyVerticesBtn = nullptr;

    QWidget* m_sidesWidget = nullptr;
    QVBoxLayout* m_sidesLayout = nullptr;
    QCheckBox* m_trapezoidIsoscelesCheck = nullptr;

    QPushButton* m_finishCustomBtn = nullptr;

    bool m_isPlacingCustomShape = false;
    double m_currentBuildDirection = 0.0;
    int m_customShapeMode = 0;
    std::vector<Point> m_tempAbsolutePoints;

private:
    void setupUi();
    void applyLightTheme();

    QWidget* createToolbar();
    QWidget* createLeftPanel();
    QWidget* createShapesListPanel();
    QWidget* createPropertiesPanel();

    void groupSelected();
void ungroupSelected();
void addSelectedToGroup();

    void createInitialShapes();
int hitEllipseFocus(const Point& p, EllipseShape* ellipse) const;
    void updatePropertiesPanel();
    void updateFillButton();

        void rebuildGroupChildrenList();
    void removeSelectedChildFromGroup();

    void rebuildAllShapesList();
    void rebuildVerticesTable();
    void rebuildSidesPanel();

    Point mouseToScene(const QPoint& pos) const;
    Shape* hitTest(const Point& p);
    int hitResizeHandle(const Point& p, const Rectangle& b) const;

    void paintCanvas();

    void onMousePress(QMouseEvent* e);
    void onMouseDoubleClick(QMouseEvent* e);
    void onMouseMove(QMouseEvent* e);
    void onMouseRelease(QMouseEvent* e);
    void onWheel(QWheelEvent* e);

    void deleteSelected();

    void saveToFile();
    void loadFromFile();

    void saveSelectedShapeToFile();
    void loadSingleShapeFromFile();

    void cancelCustomMode();
    void finishCustomShape();
    void closeCurrentCustomShape();

    void startCustomShapeByPoints();
    void startCustomShapeBySegments();

    bool appendFirstHorizontalSegment(double length);
    bool appendNextPointByLengthAndAngle(double length, double interiorAngleDeg);

    Shape* currentPropertiesShape() const;
    std::vector<Shape*> allShapesSortedById() const;

    void createShapeAndSelect(std::unique_ptr<Shape> shape);

    std::unique_ptr<Shape> createEllipseShape();
    std::unique_ptr<Shape> createRectanglePolygon();
    std::unique_ptr<Shape> createTrianglePolygon();
    std::unique_ptr<Shape> createHexagonPolygon();
    std::unique_ptr<Shape> createTrapezoidPolygon();
    std::unique_ptr<Shape> createRegularPolygonShape(int sides);

    std::unique_ptr<Shape> createStyledShape(std::unique_ptr<Shape> shape);
    std::unique_ptr<Shape> createDefaultShape(std::unique_ptr<Shape> shape);
};