#include "MainWindow.h"

#include "GeometryUtils.h"
#include "PolygonShape.h"
#include "EllipseShape.h"
#include "GroupShape.h"
#include "EditorJson.h"

#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

template <typename Fn>
QWidget* makeGroup(const QString& title, Fn fn) {
    auto* box = new QGroupBox(title);
    auto* l = new QVBoxLayout(box);
    l->setContentsMargins(6, 6, 6, 6);
    l->setSpacing(4);
    fn(l);
    return box;
}

void clearLayoutItems(QLayout* layout) {
    if (!layout) return;

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout()) {
            clearLayoutItems(item->layout());
            delete item->layout();
        }

        if (item->widget()) {
            item->widget()->deleteLater();
        }

        delete item;
    }
}

void regenerateIdsRecursively(Shape* shape) {
    if (!shape) return;

    shape->id = Shape::generateId();

    if (auto* group = dynamic_cast<GroupShape*>(shape)) {
        for (auto& child : group->children) {
            regenerateIdsRecursively(child.get());
        }
    }
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
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

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
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

void MainWindow::keyPressEvent(QKeyEvent* event) {
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

QWidget* MainWindow::createToolbar() {
    auto* bar = new QWidget;
    bar->setFixedHeight(72);

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    m_toggleLeftBtn = new QPushButton("Скрыть инструменты");
    connect(m_toggleLeftBtn, &QPushButton::clicked, this, [this]() {
        const bool visible = m_leftPanel->isVisible();
        m_leftPanel->setVisible(!visible);
        m_toggleLeftBtn->setText(visible ? "Показать инструменты" : "Скрыть инструменты");
    });
    layout->addWidget(m_toggleLeftBtn);

    m_toggleShapesListBtn = new QPushButton("Список фигур");
    connect(m_toggleShapesListBtn, &QPushButton::clicked, this, [this]() {
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

QWidget* MainWindow::createLeftPanel() {
    m_leftPanel = new QWidget;
    m_leftPanel->setMinimumWidth(300);
    m_leftPanel->setMaximumWidth(300);

    auto* outer = new QVBoxLayout(m_leftPanel);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(10);

    auto makeButton = [](const QString& text, const QString& color) {
        auto* btn = new QPushButton(text);
        btn->setMinimumHeight(38);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "background:%1;"
            "border:1px solid #9bc9ee;"
            "border-radius:8px;"
            "padding:7px 10px;"
            "text-align:left;"
            "}"
            "QPushButton:hover { background:#c7e6ff; }"
        ).arg(color));
        return btn;
    };

    auto* shapesBox = new QGroupBox("Фигуры");
    auto* shapesLayout = new QVBoxLayout(shapesBox);
    shapesLayout->setSpacing(6);

    auto* addEllipseBtn = makeButton("Эллипс", "#eaf6ff");
    connect(addEllipseBtn, &QPushButton::clicked, this, [this]() {
        createShapeAndSelect(createDefaultShape(createEllipseShape()));
    });
    shapesLayout->addWidget(addEllipseBtn);

    auto* addRectBtn = makeButton("Прямоугольник", "#eaf6ff");
    connect(addRectBtn, &QPushButton::clicked, this, [this]() {
        createShapeAndSelect(createDefaultShape(createRectanglePolygon()));
    });
    shapesLayout->addWidget(addRectBtn);

    auto* addTriangleBtn = makeButton("Треугольник", "#eaf6ff");
    connect(addTriangleBtn, &QPushButton::clicked, this, [this]() {
        createShapeAndSelect(createDefaultShape(createTrianglePolygon()));
    });
    shapesLayout->addWidget(addTriangleBtn);

    auto* addHexBtn = makeButton("Шестиугольник", "#eaf6ff");
    connect(addHexBtn, &QPushButton::clicked, this, [this]() {
        createShapeAndSelect(createDefaultShape(createHexagonPolygon()));
    });
    shapesLayout->addWidget(addHexBtn);

    auto* addTrapBtn = makeButton("Трапеция", "#eaf6ff");
    connect(addTrapBtn, &QPushButton::clicked, this, [this]() {
        createShapeAndSelect(createDefaultShape(createTrapezoidPolygon()));
    });
    shapesLayout->addWidget(addTrapBtn);

    auto* addRegularPolygonBtn = makeButton("Правильный многоугольник", "#eaf6ff");
    connect(addRegularPolygonBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        int sides = QInputDialog::getInt(
            this,
            "Правильный многоугольник",
            "Введите количество сторон:",
            5, 3, 20, 1, &ok
        );

        if (!ok) return;
        createShapeAndSelect(createDefaultShape(createRegularPolygonShape(sides)));
    });
    shapesLayout->addWidget(addRegularPolygonBtn);

    outer->addWidget(shapesBox);

    auto* customBox = new QGroupBox("Построение");
    auto* customLayout = new QVBoxLayout(customBox);
    customLayout->setSpacing(6);

    auto* addCustomBtn = makeButton("Кастомная фигура", "#fff4d9");
    connect(addCustomBtn, &QPushButton::clicked, this, [this]() {
        startCustomShapeBySegments();
    });
    customLayout->addWidget(addCustomBtn);

    m_finishCustomBtn = makeButton("Замкнуть фигуру", "#ffe8b8");
    m_finishCustomBtn->setVisible(false);
    connect(m_finishCustomBtn, &QPushButton::clicked, this, [this]() {
        closeCurrentCustomShape();
    });
    customLayout->addWidget(m_finishCustomBtn);

    outer->addWidget(customBox);

    auto* groupBox = new QGroupBox("Составные фигуры");
    auto* groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setSpacing(6);

    auto* groupBtn = makeButton("Создать составную", "#eaf9ee");
    connect(groupBtn, &QPushButton::clicked, this, [this]() {
        groupSelected();
    });
    groupLayout->addWidget(groupBtn);

    auto* addToGroupBtn = makeButton("Добавить в составную", "#eaf9ee");
    connect(addToGroupBtn, &QPushButton::clicked, this, [this]() {
        addSelectedToGroup();
    });
    groupLayout->addWidget(addToGroupBtn);

    auto* ungroupBtn = makeButton("Разгруппировать", "#eaf9ee");
    connect(ungroupBtn, &QPushButton::clicked, this, [this]() {
        ungroupSelected();
    });
    groupLayout->addWidget(ungroupBtn);

    outer->addWidget(groupBox);

    auto* fileBox = new QGroupBox("Файлы");
    auto* fileLayout = new QVBoxLayout(fileBox);
    fileLayout->setSpacing(6);

    auto* loadShapeBtn = makeButton("Загрузить фигуру", "#f0eaff");
    connect(loadShapeBtn, &QPushButton::clicked, this, [this]() {
        loadSingleShapeFromFile();
    });
    fileLayout->addWidget(loadShapeBtn);

    outer->addWidget(fileBox);

    outer->addStretch();

    return m_leftPanel;
}

QWidget* MainWindow::createShapesListPanel() {
    m_shapesDock = new QWidget;
    m_shapesDock->setMinimumWidth(250);
    m_shapesDock->setMaximumWidth(250);

    auto* outer = new QVBoxLayout(m_shapesDock);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    outer->addWidget(new QLabel("Список фигур"));

    m_allShapesList = new QListWidget;
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

QWidget* MainWindow::createPropertiesPanel() {
    m_propertiesPanel = new QWidget;
    m_propertiesPanel->setMinimumWidth(650);
    m_propertiesPanel->setMaximumWidth(650);

    auto* outer = new QVBoxLayout(m_propertiesPanel);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* bodyWidget = new QWidget;
    auto* body = new QVBoxLayout(bodyWidget);

    body->addWidget(makeGroup("Границы", [this](QVBoxLayout* l) {
        m_boundsLabel = new QLabel("Фигура не выбрана");
        m_boundsLabel->setWordWrap(true);
        l->addWidget(m_boundsLabel);
    }));

    body->addWidget(makeGroup("Фигура", [this](QVBoxLayout* l) {
    auto* saveShapeBtn = new QPushButton("Сохранить выбранную фигуру");
    saveShapeBtn->setMinimumHeight(38);
    saveShapeBtn->setStyleSheet(
        "QPushButton {"
        "background:#f0eaff;"
        "border:1px solid #b9a7ee;"
        "border-radius:8px;"
        "padding:7px 10px;"
        "}"
        "QPushButton:hover { background:#e4d8ff; }"
    );

    connect(saveShapeBtn, &QPushButton::clicked, this, [this]() {
        saveSelectedShapeToFile();
    });

    l->addWidget(saveShapeBtn);
}));

    body->addWidget(makeGroup("Точка привязки", [this](QVBoxLayout* l) {
        auto* row1 = new QHBoxLayout;
        row1->addWidget(new QLabel("Абс. X:"));
        m_anchorX = new QSpinBox;
        m_anchorX->setRange(-5000, 5000);
        row1->addWidget(m_anchorX);

        row1->addWidget(new QLabel("Абс. Y:"));
        m_anchorY = new QSpinBox;
        m_anchorY->setRange(-5000, 5000);
        row1->addWidget(m_anchorY);

        l->addLayout(row1);

        auto* row2 = new QHBoxLayout;
        row2->addWidget(new QLabel("Лок. X:"));
        m_anchorInsideX = new QSpinBox;
        m_anchorInsideX->setRange(-5000, 5000);
        row2->addWidget(m_anchorInsideX);

        row2->addWidget(new QLabel("Лок. Y:"));
        m_anchorInsideY = new QSpinBox;
        m_anchorInsideY->setRange(-5000, 5000);
        row2->addWidget(m_anchorInsideY);

        l->addLayout(row2);

        auto applyAbsAnchor = [this]() {
            if (m_updatingUi) return;
            Shape* target = currentPropertiesShape();
            if (!target) return;

            target->anchor = {double(m_anchorX->value()), double(m_anchorY->value())};
            updatePropertiesPanel();
            m_canvas->update();
        };

        auto applyInnerAnchor = [this]() {
            if (m_updatingUi) return;
            Shape* target = currentPropertiesShape();
            if (!target) return;

            target->setAnchorInside({double(m_anchorInsideX->value()), double(m_anchorInsideY->value())});
            updatePropertiesPanel();
            m_canvas->update();
        };

        connect(m_anchorX, &QSpinBox::editingFinished, this, applyAbsAnchor);
        connect(m_anchorY, &QSpinBox::editingFinished, this, applyAbsAnchor);
        connect(m_anchorInsideX, &QSpinBox::editingFinished, this, applyInnerAnchor);
        connect(m_anchorInsideY, &QSpinBox::editingFinished, this, applyInnerAnchor);
    }));

    body->addWidget(makeGroup("Масштаб", [this](QVBoxLayout* l) {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Коэффициент:"));

        m_scaleSpin = new QDoubleSpinBox;
        m_scaleSpin->setRange(1.0, 5000.0);
        m_scaleSpin->setSingleStep(5.0);
        m_scaleSpin->setDecimals(0);
        m_scaleSpin->setSuffix("%");
        row->addWidget(m_scaleSpin);

        l->addLayout(row);

        connect(m_scaleSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
            if (m_updatingUi) return;
            Shape* target = currentPropertiesShape();
            if (!target) return;

            const double newPercent = m_scaleSpin->value();
            if (newPercent <= 0.0) return;

            const double factor = newPercent / target->currentScale;
            if (std::abs(factor - 1.0) < 1e-9) return;

            target->scaleUniform(factor);
            updatePropertiesPanel();
            m_canvas->update();
        });
    }));

   m_rotationGroup = makeGroup("Поворот эллипса", [this](QVBoxLayout* l) {
    auto* row = new QHBoxLayout;

    row->addWidget(new QLabel("Угол наклона:"));

    m_rotationSpin = new QDoubleSpinBox;
    m_rotationSpin->setRange(0.0, 359.0);
    m_rotationSpin->setDecimals(0);
    m_rotationSpin->setSingleStep(5.0);
    m_rotationSpin->setSuffix("°");
    m_rotationSpin->setMinimumWidth(120);

    row->addWidget(m_rotationSpin);
    row->addStretch();

    l->addLayout(row);

    connect(m_rotationSpin, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (m_updatingUi) return;

        auto* ellipse = dynamic_cast<EllipseShape*>(currentPropertiesShape());
        if (!ellipse) return;

        ellipse->setRotationDeg(m_rotationSpin->value());

        m_canvas->update();
        updatePropertiesPanel();
    });
});

body->addWidget(m_rotationGroup);

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

    m_verticesGroup = makeGroup("Вершины", [this](QVBoxLayout* l) {
        m_verticesTable = new QTableWidget;
        m_verticesTable->setColumnCount(3);
        m_verticesTable->setHorizontalHeaderLabels({"№", "X", "Y"});
        m_verticesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_verticesTable->verticalHeader()->setVisible(false);
        l->addWidget(m_verticesTable);

        m_applyVerticesBtn = new QPushButton("Применить вершины");
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
            updatePropertiesPanel();
            m_canvas->update();
        });
    });

    body->addWidget(m_verticesGroup);

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

            auto children = group->groupedShapes();
            if (row >= 0 && row < (int)children.size()) {
                m_propertiesShape = children[row];
            } else {
                m_propertiesShape = nullptr;
            }

            updatePropertiesPanel();
            m_canvas->update();
        });

        l->addWidget(m_groupChildrenList);

        m_removeChildBtn = new QPushButton("Убрать из состава");
        connect(m_removeChildBtn, &QPushButton::clicked, this, [this]() {
            removeSelectedChildFromGroup();
        });
        l->addWidget(m_removeChildBtn);
    }

    m_groupChildrenBox->setVisible(false);
    body->addWidget(m_groupChildrenBox);

    body->addStretch();
    scroll->setWidget(bodyWidget);
    outer->addWidget(scroll);

    return m_propertiesPanel;
}

void MainWindow::setupUi() {
    auto* central = new QWidget;
    setCentralWidget(central);

    QMenu* fileMenu = menuBar()->addMenu("Файл");

    QAction* saveAction = fileMenu->addAction("Сохранить файл");
    QAction* loadAction = fileMenu->addAction("Загрузить файл");


    connect(saveAction, &QAction::triggered, this, [this]() { saveToFile(); });
    connect(loadAction, &QAction::triggered, this, [this]() { loadFromFile(); });
   

    auto* closeCornerBtn = new QToolButton(this);
    closeCornerBtn->setText("✕");
    closeCornerBtn->setFixedSize(28, 28);
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
    m_statusLabel = new QLabel("Выделение: Shift + клик. Delete – удалить. Esc – отменить построение.");
    m_statusBar->addWidget(m_statusLabel, 1);

    rebuildAllShapesList();

    m_leftPanel->hide();
    m_shapesDock->hide();
    m_propertiesPanel->hide();

    m_toggleLeftBtn->setText("Показать инструменты");
    m_togglePropertiesBtn->setText("Показать свойства");
}

void MainWindow::applyLightTheme() {
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
        QLineEdit, QSpinBox, QDoubleSpinBox, QTableWidget, QListWidget {
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

std::unique_ptr<Shape> MainWindow::createEllipseShape() {
    auto s = std::make_unique<EllipseShape>();
    s->setSemiAxes(90.0, 70.0, false); // горизонтальный эллипс
    s->ensureStyleArrays();
    return s;
}

std::unique_ptr<Shape> MainWindow::createRectanglePolygon() {
    auto s = std::make_unique<PolygonShape>("Прямоугольник");
    s->relVerts = {{0, 0}, {220, 0}, {220, 150}, {0, 150}};
    s->angleEditingEnabled = false;
    s->ensureStyleArrays();
    return s;
}

std::unique_ptr<Shape> MainWindow::createTrianglePolygon() {
    auto s = std::make_unique<PolygonShape>("Треугольник");
    s->relVerts = {{0, 0}, {180, 0}, {90, 156}};
    s->angleEditingEnabled = true;
    s->ensureStyleArrays();
    return s;
}

std::unique_ptr<Shape> MainWindow::createHexagonPolygon() {
    auto s = std::make_unique<PolygonShape>("Шестиугольник");
    s->relVerts = {{60, 0}, {180, 0}, {240, 90}, {180, 180}, {60, 180}, {0, 90}};
    s->angleEditingEnabled = true;
    s->ensureStyleArrays();
    return s;
}

std::unique_ptr<Shape> MainWindow::createTrapezoidPolygon() {
    auto s = std::make_unique<PolygonShape>("Трапеция");
    s->relVerts = {{50, 0}, {210, 0}, {260, 140}, {0, 140}};
    s->angleEditingEnabled = false;
    s->trapezoidMode = true;
    s->isIsoscelesTrapezoid = false;
    s->ensureStyleArrays();
    return s;
}

std::unique_ptr<Shape> MainWindow::createRegularPolygonShape(int sides) {
    if (sides < 3) return nullptr;

    auto poly = std::make_unique<PolygonShape>("Правильный многоугольник");
    poly->angleEditingEnabled = true;

    const double radius = 120.0;
    const double angleStep = 2.0 * M_PI / sides;
    const double startAngle = M_PI / 2.0;

    for (int i = 0; i < sides; ++i) {
        double angle = startAngle + i * angleStep;
        poly->relVerts.push_back({radius * std::cos(angle), radius * std::sin(angle)});
    }

    poly->ensureStyleArrays();
    return poly;
}

std::unique_ptr<Shape> MainWindow::createStyledShape(std::unique_ptr<Shape> shape) {
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
        shape->sideWidths[i] = 2.0 + ((shape->id + i) % 4) * 2.0;
    }

    return shape;
}

std::unique_ptr<Shape> MainWindow::createDefaultShape(std::unique_ptr<Shape> shape) {
    if (!shape) return nullptr;

    shape->ensureStyleArrays();
    shape->fillColor = Qt::transparent;

    for (int i = 0; i < shape->sideCount(); ++i) {
        shape->sideColors[i] = Qt::black;
        shape->sideWidths[i] = 3.0;
    }

    return shape;
}

void MainWindow::createInitialShapes() {
    createShapeAndSelect(createStyledShape(createEllipseShape()));
    createShapeAndSelect(createStyledShape(createRectanglePolygon()));
    createShapeAndSelect(createStyledShape(createTrianglePolygon()));
    createShapeAndSelect(createStyledShape(createHexagonPolygon()));
    createShapeAndSelect(createStyledShape(createTrapezoidPolygon()));

    for (int i = 0; i < (int)m_shapes.size(); ++i) {
        auto& shape = m_shapes[i];
        if (!shape) continue;

        Rectangle b = shape->boundingBox();
        Point currentCenter{
            b.x + b.width * 0.5,
            b.y + b.height * 0.5
        };

        Point targetCenter{
            250.0 + i * 320.0,
            m_pageHeight * 0.5
        };

        shape->moveBy(targetCenter - currentCenter);

        b = shape->boundingBox();
        Point newCenter{
            b.x + b.width * 0.5,
            b.y + b.height * 0.5
        };

        shape->setAnchorInside(newCenter);
    }

    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;
    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}
void MainWindow::rebuildGroupChildrenList() {
    if (!m_groupChildrenBox || !m_groupChildrenList || !m_removeChildBtn) return;

    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    const bool isGroup = (group != nullptr);

    m_updatingUi = true;

    m_groupChildrenBox->setVisible(isGroup);
    m_groupChildrenList->clear();

    if (!isGroup) {
        m_removeChildBtn->setEnabled(false);
        m_propertiesShape = nullptr;
        m_updatingUi = false;
        return;
    }

    auto children = group->groupedShapes();

    for (int i = 0; i < (int)children.size(); ++i) {
        Shape* child = children[i];
        QString text = QString("%1. ID %2 – %3")
            .arg(i + 1)
            .arg(child->id)
            .arg(child->kindName());
        m_groupChildrenList->addItem(text);
    }

    int rowToSelect = -1;
    if (m_propertiesShape) {
        for (int i = 0; i < (int)children.size(); ++i) {
            if (children[i] == m_propertiesShape) {
                rowToSelect = i;
                break;
            }
        }
    }

    if (rowToSelect >= 0) {
        m_groupChildrenList->setCurrentRow(rowToSelect);
    } else {
        m_groupChildrenList->clearSelection();
        m_propertiesShape = nullptr;
    }

    m_removeChildBtn->setEnabled(m_groupChildrenList->currentRow() >= 0);

    m_updatingUi = false;
}

void MainWindow::removeSelectedChildFromGroup() {
    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    if (!group) return;

    int row = m_groupChildrenList ? m_groupChildrenList->currentRow() : -1;
    if (row < 0) return;
    if (row >= (int)group->children.size()) return;

    std::unique_ptr<Shape> extracted = std::move(group->children[row]);
    group->children.erase(group->children.begin() + row);

    if (extracted) {
        Shape* raw = extracted.get();
        m_shapes.push_back(std::move(extracted));
        m_selectedShape = raw;
        m_selectedShapes = {raw};
        m_propertiesShape = nullptr;
    }

    if (group->children.empty()) {
        m_shapes.erase(
            std::remove_if(m_shapes.begin(), m_shapes.end(),
                [group](const std::unique_ptr<Shape>& ptr) { return ptr.get() == group; }),
            m_shapes.end()
        );
    } else if (group->children.size() == 1) {
        std::unique_ptr<Shape> lastChild = std::move(group->children[0]);

        m_shapes.erase(
            std::remove_if(m_shapes.begin(), m_shapes.end(),
                [group](const std::unique_ptr<Shape>& ptr) { return ptr.get() == group; }),
            m_shapes.end()
        );

        Shape* raw = lastChild.get();
        m_shapes.push_back(std::move(lastChild));
        m_selectedShape = raw;
        m_selectedShapes = {raw};
        m_propertiesShape = nullptr;
    } else {
        group->rebuildAnchorToCenter();
    }

    rebuildAllShapesList();
    rebuildGroupChildrenList();
    updatePropertiesPanel();
    m_canvas->update();
}

Point MainWindow::mouseToScene(const QPoint& pos) const {
    return {
        (double(pos.x()) - m_viewOffset.x) / m_viewScale,
        (double(m_canvas->height() - pos.y()) - m_viewOffset.y) / m_viewScale
    };
}

Shape* MainWindow::hitTest(const Point& p) {
    for (int i = (int)m_shapes.size() - 1; i >= 0; --i) {
        if (m_shapes[i] && m_shapes[i]->contains(p)) {
            return m_shapes[i].get();
        }
    }
    return nullptr;
}

int MainWindow::hitResizeHandle(const Point& p, const Rectangle& b) const {
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

int MainWindow::hitEllipseFocus(const Point& p, EllipseShape* ellipse) const {
    if (!ellipse) return -1;

    double tolerance = 12.0 / m_viewScale;

    if (distancePts(p, ellipse->focus1Abs()) <= tolerance) return 0;
    if (distancePts(p, ellipse->focus2Abs()) <= tolerance) return 1;

    return -1;
}

void MainWindow::createShapeAndSelect(std::unique_ptr<Shape> shape) {
    if (!shape) return;

    if (shape->id <= 0) {
        shape->id = Shape::generateId();
    }

    shape->ensureStyleArrays();

    Rectangle b = shape->boundingBox();
    Point currentCenter{
        b.x + b.width * 0.5,
        b.y + b.height * 0.5
    };

    Point screenCenter{
        double(m_canvas->width()) * 0.5,
        double(m_canvas->height()) * 0.5
    };

    Point targetCenter = mouseToScene(QPoint(
        int(screenCenter.x),
        int(screenCenter.y)
    ));

    shape->moveBy(targetCenter - currentCenter);

    b = shape->boundingBox();
    Point newCenter{
        b.x + b.width * 0.5,
        b.y + b.height * 0.5
    };

    shape->setAnchorInside(newCenter);

    m_selectedShape = shape.get();
    m_selectedShapes = {m_selectedShape};
    m_propertiesShape = nullptr;
    m_shapes.push_back(std::move(shape));

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

void MainWindow::saveToFile() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить фигуры",
        "",
        "JSON Files (*.json);;All Files (*)"
    );
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    QString err;
    if (!EditorJson::saveDocument(
            fileName,
            m_shapes,
            m_pageWidth,
            m_pageHeight,
            m_viewScale,
            m_viewOffset.x,
            m_viewOffset.y,
            Shape::s_nextId,
            &err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }

    m_statusLabel->setText("Файл успешно сохранён.");
}

void MainWindow::loadFromFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Загрузить файл",
        "",
        "JSON Files (*.json);;All Files (*)"
    );

    if (fileName.isEmpty()) return;

    EditorDocumentData data;
    QString err;

    if (EditorJson::loadDocument(fileName, data, &err)) {
        m_shapes = std::move(data.shapes);
        m_pageWidth = data.pageWidth;
        m_pageHeight = data.pageHeight;
        m_viewScale = data.viewScale;
        m_viewOffset.x = data.viewOffsetX;
        m_viewOffset.y = data.viewOffsetY;

        int maxId = 0;
        for (const auto& shape : m_shapes) {
            if (shape) {
                maxId = std::max(maxId, shape->id);
            }
        }

        Shape::s_nextId = std::max(data.nextId, maxId + 1);

        m_selectedShape = nullptr;
        m_selectedShapes.clear();
        m_propertiesShape = nullptr;

        m_highlightVertexIndex = -1;
        m_highlightVertexShape = nullptr;
        m_highlightSideIndex = -1;
        m_highlightShape = nullptr;
        m_highlightAngleIndex = -1;
        m_highlightAngleShape = nullptr;

        rebuildAllShapesList();
        updatePropertiesPanel();
        m_canvas->update();

        m_statusLabel->setText("Файл успешно загружен.");
        return;
    }

    QString shapeErr;
    auto shape = EditorJson::loadSingleShape(fileName, &shapeErr);

    if (shape) {
        QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            "Загрузка фигуры",
            "Выбранный файл не является полным проектом, но содержит отдельную фигуру.\n\nДобавить эту фигуру на текущий лист?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (answer == QMessageBox::Yes) {
            regenerateIdsRecursively(shape.get());
            createShapeAndSelect(std::move(shape));
            m_statusLabel->setText("Фигура добавлена на лист.");
        }

        return;
    }

    QMessageBox::warning(
        this,
        "Ошибка загрузки",
        err.isEmpty() ? "Не удалось загрузить файл." : err
    );
}
void MainWindow::saveSelectedShapeToFile() {
    Shape* target = currentPropertiesShape();
    if (!target) {
        QMessageBox::information(this, "Сохранение фигуры", "Сначала выбери фигуру.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить фигуру",
        "",
        "JSON Files (*.json);;All Files (*)"
    );
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    QString err;
    if (!EditorJson::saveSingleShape(fileName, *target, &err)) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }

    m_statusLabel->setText("Фигура успешно сохранена.");
}

void MainWindow::loadSingleShapeFromFile() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Загрузить фигуру",
        "",
        "JSON Files (*.json);;All Files (*)"
    );
    if (fileName.isEmpty()) return;

    QString err;
    auto shape = EditorJson::loadSingleShape(fileName, &err);
    if (!shape) {
        QMessageBox::warning(this, "Ошибка", err);
        return;
    }

    regenerateIdsRecursively(shape.get());

    createShapeAndSelect(std::move(shape));
    m_statusLabel->setText("Фигура успешно загружена.");
}

void MainWindow::cancelCustomMode() {
    m_isPlacingCustomShape = false;
    m_customShapeMode = 0;
    m_tempAbsolutePoints.clear();
    m_currentBuildDirection = 0.0;
    if (m_finishCustomBtn) m_finishCustomBtn->setVisible(false);
    m_statusLabel->setText("Выделение: Shift + клик. Delete – удалить. Esc – отменить построение.");
    m_canvas->update();
}

void MainWindow::startCustomShapeByPoints() {
    m_isPlacingCustomShape = true;
    m_customShapeMode = 0;
    m_tempAbsolutePoints.clear();
    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;
    m_statusLabel->setText("Кликай по листу, чтобы добавить точки кастомной фигуры. Esc – отмена.");
    m_canvas->update();
}

void MainWindow::startCustomShapeBySegments() {
    m_isPlacingCustomShape = true;
    m_customShapeMode = 1;
    m_tempAbsolutePoints.clear();
    m_currentBuildDirection = 0.0;
    m_selectedShape = nullptr;
    m_selectedShapes.clear();
    m_propertiesShape = nullptr;
    m_statusLabel->setText("Кликни первую точку кастомной фигуры. Затем будет запрошена длина первого отрезка.");
    m_canvas->update();
}

bool MainWindow::appendFirstHorizontalSegment(double length) {
    if (m_tempAbsolutePoints.size() != 1) return false;
    if (length <= 1.0) return false;

    const Point& a = m_tempAbsolutePoints[0];
    Point b{a.x + length, a.y};

    m_tempAbsolutePoints.push_back(b);
    m_currentBuildDirection = 0.0;
    return true;
}

bool MainWindow::appendNextPointByLengthAndAngle(double length, double interiorAngleDeg) {
    if (m_tempAbsolutePoints.size() < 2) return false;
    if (length <= 1.0) return false;
    if (interiorAngleDeg <= 0.0 || interiorAngleDeg >= 180.0) return false;

    const Point& b = m_tempAbsolutePoints.back();
    const double turnRad = M_PI - interiorAngleDeg * M_PI / 180.0;
    m_currentBuildDirection += turnRad;

    Point c{
        b.x + length * std::cos(m_currentBuildDirection),
        b.y + length * std::sin(m_currentBuildDirection)
    };

    m_tempAbsolutePoints.push_back(c);
    return true;
}

void MainWindow::closeCurrentCustomShape() {
    if (!m_isPlacingCustomShape || m_customShapeMode != 1) return;

    if (m_tempAbsolutePoints.size() < 3) {
        QMessageBox::warning(this, "Ошибка", "Для замыкания нужно минимум 3 вершины.");
        return;
    }

    finishCustomShape();
}

void MainWindow::finishCustomShape() {
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

    auto poly = std::make_unique<PolygonShape>("Кастомная фигура");
    poly->angleEditingEnabled = true;
    poly->anchor = finalPoints.front();

    for (const auto& p : finalPoints) {
        poly->relVerts.push_back(p - finalPoints.front());
    }

    poly->ensureStyleArrays();
    poly->fillColor = Qt::transparent;
    for (int i = 0; i < poly->sideCount(); ++i) {
        poly->sideColors[i] = Qt::black;
        poly->sideWidths[i] = 3.0;
    }

    createShapeAndSelect(std::move(poly));
    cancelCustomMode();
    updatePropertiesPanel();
    m_statusLabel->setText("Кастомная фигура создана.");
}

void MainWindow::paintCanvas() {
    QPainter painter(m_canvas);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(m_canvas->rect(), QColor(170, 170, 170));

    QRectF pageScreen(
        m_viewOffset.x,
        m_canvas->height() - (m_viewOffset.y + m_pageHeight * m_viewScale),
        m_pageWidth * m_viewScale,
        m_pageHeight * m_viewScale
    );

    QRectF shadowRect = pageScreen.translated(6, 6);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 35));
    painter.drawRect(shadowRect);

    painter.setPen(QPen(QColor(140, 140, 140), 1));
    painter.setBrush(m_pageColor);
    painter.drawRect(pageScreen);

    painter.translate(0, m_canvas->height());
    painter.scale(1, -1);
    painter.translate(m_viewOffset.x, m_viewOffset.y);
    painter.scale(m_viewScale, m_viewScale);

    painter.setClipRect(QRectF(0.0, 0.0, m_pageWidth, m_pageHeight));

    for (const auto& shape : m_shapes) {
        shape->draw(painter);
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

    auto* selectedGroup = dynamic_cast<GroupShape*>(m_selectedShape);
    if (selectedGroup && m_propertiesShape && m_propertiesShape != m_selectedShape) {
        Rectangle b = m_propertiesShape->boundingBox();

        QPen pen(QColor(255, 140, 0), 0, Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(b.x, b.y, b.width, b.height));
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

    if (auto* ellipse = dynamic_cast<EllipseShape*>(target)) {
        Point f1 = ellipse->focus1Abs();
        Point f2 = ellipse->focus2Abs();

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 140, 0));
        painter.drawEllipse(QPointF(f1.x, f1.y), 6.0 / m_viewScale, 6.0 / m_viewScale);
        painter.drawEllipse(QPointF(f2.x, f2.y), 6.0 / m_viewScale, 6.0 / m_viewScale);

        QPen fp(QColor(255, 140, 0), 0);
        fp.setCosmetic(true);
        painter.setPen(fp);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(f1.x, f1.y), 10.0 / m_viewScale, 10.0 / m_viewScale);
        painter.drawEllipse(QPointF(f2.x, f2.y), 10.0 / m_viewScale, 10.0 / m_viewScale);
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

void MainWindow::onMousePress(QMouseEvent* e) {
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
            m_statusLabel->setText(QString("Точек: %1. Двойной клик – завершить, Esc – отмена.").arg((int)m_tempAbsolutePoints.size()));
            m_canvas->update();
            return;
        }

        if (m_customShapeMode == 1) {
            if (m_tempAbsolutePoints.empty()) {
                m_tempAbsolutePoints.push_back(p);
                m_finishCustomBtn->setVisible(true);

                bool ok = false;
                double length = QInputDialog::getDouble(
                    this, "Первый отрезок", "Введите длину первого отрезка:",
                    100.0, 1.0, 5000.0, 0, &ok
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

                m_statusLabel->setText("Первый отрезок построен. Теперь кликай для добавления следующей стороны или нажми 'Замкнуть фигуру'.");
                m_canvas->update();
                return;
            }

            bool okLen = false;
            double length = QInputDialog::getDouble(
                this, "Следующий отрезок", "Введите длину следующего отрезка:",
                100.0, 1.0, 5000.0, 0, &okLen
            );
            if (!okLen) return;

            bool okAngle = false;
            double angle = QInputDialog::getDouble(
                this, "Угол между сторонами", "Введите угол между предыдущей и новой стороной:",
                120.0, 1.0, 179.0, 0, &okAngle
            );
            if (!okAngle) return;

            if (!appendNextPointByLengthAndAngle(length, angle)) {
                QMessageBox::warning(this, "Ошибка", "Не удалось построить следующий отрезок.");
                return;
            }

            m_statusLabel->setText(QString("Вершин: %1. Кликни для добавления ещё одной стороны или нажми 'Замкнуть фигуру'.")
                .arg((int)m_tempAbsolutePoints.size()));
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

    if (auto* ellipse = dynamic_cast<EllipseShape*>(target)) {
        int focusHit = hitEllipseFocus(p, ellipse);
        if (focusHit != -1) {
            m_draggingEllipseFocus = true;
            m_dragFocusEllipse = ellipse;
            m_dragFocusIndex = focusHit;
            m_lastMouse = p;
            return;
        }
    }

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

void MainWindow::onMouseDoubleClick(QMouseEvent* e) {
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

void MainWindow::onMouseMove(QMouseEvent* e) {
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

    if (m_draggingEllipseFocus && m_dragFocusEllipse) {
        Point center = m_dragFocusEllipse->center();
        Point newOffset = p - center;

        Point f1;
        Point f2;

        if (m_dragFocusIndex == 0) {
            f1 = p;
            f2 = center - newOffset;
        } else {
            f2 = p;
            f1 = center - newOffset;
        }

        m_dragFocusEllipse->setFromAbsoluteFoci(f1, f2);
        m_canvas->update();
        return;
    }

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

        Rectangle oldBounds = m_selectedShape->boundingBox();

        double u = 0.5;
        double v = 0.5;

        if (oldBounds.width > 1e-9) {
            u = (m_selectedShape->anchor.x - oldBounds.x) / oldBounds.width;
        }

        if (oldBounds.height > 1e-9) {
            v = (m_selectedShape->anchor.y - oldBounds.y) / oldBounds.height;
        }

        

        Rectangle target{left, bottom, right - left, top - bottom};
        m_selectedShape->resizeToBoundingBox(target);

        Point newAnchor{
            target.x + target.width * u,
            target.y + target.height * v
        };

        m_selectedShape->setAnchorInside(newAnchor);

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

void MainWindow::onMouseRelease(QMouseEvent*) {
    if (m_dragging || m_resizing || m_draggingEllipseFocus) {
        updatePropertiesPanel();
        m_canvas->update();
    }

    m_dragging = false;
    m_dragAnchor = false;
    m_resizing = false;
    m_resizeHandleIndex = -1;
    m_panningCanvas = false;

    m_draggingEllipseFocus = false;
    m_dragFocusEllipse = nullptr;
    m_dragFocusIndex = -1;
}

void MainWindow::onWheel(QWheelEvent* e) {
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

void MainWindow::deleteSelected() {
    if (m_selectedShapes.empty()) return;

    m_shapes.erase(
        std::remove_if(m_shapes.begin(), m_shapes.end(), [this](const std::unique_ptr<Shape>& ptr) {
            return std::find(m_selectedShapes.begin(), m_selectedShapes.end(), ptr.get()) != m_selectedShapes.end();
        }),
        m_shapes.end()
    );

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

Shape* MainWindow::currentPropertiesShape() const {
    if (!m_selectedShape) return nullptr;

    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    if (!group) {
        return m_selectedShape;
    }

    if (!m_propertiesShape) {
        return m_selectedShape;
    }

    for (auto* child : group->groupedShapes()) {
        if (child == m_propertiesShape) {
            return m_propertiesShape;
        }
    }

    return m_selectedShape;
}

std::vector<Shape*> MainWindow::allShapesSortedById() const {
    std::vector<Shape*> out;
    out.reserve(m_shapes.size());

    for (const auto& s : m_shapes) {
        out.push_back(s.get());
    }

    std::sort(out.begin(), out.end(), [](Shape* a, Shape* b) {
        return a->id < b->id;
    });

    return out;
}

void MainWindow::groupSelected() {
    if (m_selectedShapes.size() < 2) {
        QMessageBox::warning(this, "Ошибка", "Нужно выбрать минимум 2 фигуры.");
        return;
    }

    auto group = std::make_unique<GroupShape>();
    group->id = Shape::generateId();

    std::vector<Shape*> selected = m_selectedShapes;

    for (Shape* s : selected) {
        auto it = std::find_if(m_shapes.begin(), m_shapes.end(),
            [s](const std::unique_ptr<Shape>& ptr) { return ptr.get() == s; });

        if (it != m_shapes.end()) {
            group->children.push_back(std::move(*it));
            m_shapes.erase(it);
        }
    }

    if (group->children.empty()) return;

    group->rebuildAnchorToCenter();

    m_selectedShape = group.get();
    m_selectedShapes = {m_selectedShape};
    m_propertiesShape = nullptr;
    m_shapes.push_back(std::move(group));

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

void MainWindow::ungroupSelected() {
    auto* group = dynamic_cast<GroupShape*>(m_selectedShape);
    if (!group) {
        QMessageBox::information(this, "Разгруппировка", "Сначала выбери составную фигуру.");
        return;
    }

    auto it = std::find_if(m_shapes.begin(), m_shapes.end(),
        [group](const std::unique_ptr<Shape>& ptr) { return ptr.get() == group; });

    if (it == m_shapes.end()) return;

    std::vector<std::unique_ptr<Shape>> extracted;
    for (auto& child : group->children) {
        extracted.push_back(std::move(child));
    }
    group->children.clear();

    m_shapes.erase(it);

    m_selectedShapes.clear();
    for (auto& child : extracted) {
        m_selectedShapes.push_back(child.get());
        m_shapes.push_back(std::move(child));
    }

    m_selectedShape = m_selectedShapes.empty() ? nullptr : m_selectedShapes.back();
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

void MainWindow::addSelectedToGroup() {
    GroupShape* group = nullptr;

    for (Shape* s : m_selectedShapes) {
        group = dynamic_cast<GroupShape*>(s);
        if (group) break;
    }

    if (!group) {
        QMessageBox::information(
            this,
            "Составная фигура",
            "Нужно выбрать составную фигуру и ещё хотя бы одну обычную фигуру через Shift + клик."
        );
        return;
    }

    std::vector<Shape*> toAdd;
    for (Shape* s : m_selectedShapes) {
        if (s != group) toAdd.push_back(s);
    }

    if (toAdd.empty()) {
        QMessageBox::information(this, "Составная фигура", "Нет фигур для добавления.");
        return;
    }

    for (Shape* s : toAdd) {
        auto it = std::find_if(m_shapes.begin(), m_shapes.end(),
            [s](const std::unique_ptr<Shape>& ptr) { return ptr.get() == s; });

        if (it != m_shapes.end()) {
            group->children.push_back(std::move(*it));
            m_shapes.erase(it);
        }
    }

    group->rebuildAnchorToCenter();

    m_selectedShape = group;
    m_selectedShapes = {group};
    m_propertiesShape = nullptr;

    rebuildAllShapesList();
    updatePropertiesPanel();
    m_canvas->update();
}

void MainWindow::rebuildAllShapesList() {
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
            if (item->data(Qt::UserRole).toInt() == m_selectedShape->id) {
                m_allShapesList->setCurrentRow(i);
                break;
            }
        }
    }

    m_updatingUi = false;
}

void MainWindow::updateFillButton() {
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

void MainWindow::rebuildVerticesTable() {
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

void MainWindow::rebuildSidesPanel() {

    if (!m_sidesLayout) return;

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
        m_sidesLayout->addWidget(new QLabel("У составной фигуры стороны редактируются у выбранной фигуры из списка ниже."));
        return;
    }

    if (auto* poly = dynamic_cast<PolygonShape*>(target); poly && poly->trapezoidMode) {
        m_trapezoidIsoscelesCheck = new QCheckBox("Равнобедренная трапеция");
        m_trapezoidIsoscelesCheck->setChecked(poly->isIsoscelesTrapezoid);

        connect(m_trapezoidIsoscelesCheck, &QCheckBox::toggled, this, [this, poly](bool checked) {
            if (m_updatingUi) return;
            poly->isIsoscelesTrapezoid = checked;
            m_canvas->update();
            QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
        });

        m_sidesLayout->addWidget(m_trapezoidIsoscelesCheck);
    }

    target->ensureStyleArrays();
    std::vector<double> lengths = target->sideLengths();
    std::vector<double> angles = target->interiorAngles();

      if (auto* ellipse = dynamic_cast<EllipseShape*>(target)) {
    auto* info = new QLabel(
        QString("Тип: %1\nОриентация: %2\nБольшая полуось a: %3 px\nМалая полуось b: %4 px")
            .arg(ellipse->isCircle() ? "окружность" : "эллипс")
            .arg(ellipse->isCircle() ? "не важно" : (ellipse->isVertical() ? "вертикальный" : "горизонтальный"))
            .arg((int)std::round(ellipse->semiMajor))
            .arg((int)std::round(ellipse->semiMinorAxis()))
    );
    info->setWordWrap(true);
    info->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_sidesLayout->addWidget(info);

    Point f1 = ellipse->focus1Abs();
    Point f2 = ellipse->focus2Abs();

    auto* f1RowWrap = new QWidget;
    auto* f1Row = new QHBoxLayout(f1RowWrap);
    f1Row->setContentsMargins(0, 0, 0, 0);
    f1Row->setSpacing(6);

    f1Row->addWidget(new QLabel("Фокус 1 X:"));
    auto* f1x = new QDoubleSpinBox;
    f1x->setRange(-5000.0, 5000.0);
    f1x->setDecimals(0);
    f1x->setValue(std::round(f1.x));
    f1x->setFixedWidth(110);
    f1Row->addWidget(f1x);

    f1Row->addWidget(new QLabel("Y:"));
    auto* f1y = new QDoubleSpinBox;
    f1y->setRange(-5000.0, 5000.0);
    f1y->setDecimals(0);
    f1y->setValue(std::round(f1.y));
    f1y->setFixedWidth(110);
    f1Row->addWidget(f1y);

    f1Row->addStretch();
    m_sidesLayout->addWidget(f1RowWrap);

    auto* f2RowWrap = new QWidget;
    auto* f2Row = new QHBoxLayout(f2RowWrap);
    f2Row->setContentsMargins(0, 0, 0, 0);
    f2Row->setSpacing(6);

    f2Row->addWidget(new QLabel("Фокус 2 X:"));
    auto* f2x = new QDoubleSpinBox;
    f2x->setRange(-5000.0, 5000.0);
    f2x->setDecimals(0);
    f2x->setValue(std::round(f2.x));
    f2x->setFixedWidth(110);
    f2Row->addWidget(f2x);

    f2Row->addWidget(new QLabel("Y:"));
    auto* f2y = new QDoubleSpinBox;
    f2y->setRange(-5000.0, 5000.0);
    f2y->setDecimals(0);
    f2y->setValue(std::round(f2.y));
    f2y->setFixedWidth(110);
    f2Row->addWidget(f2y);

    f2Row->addStretch();
    m_sidesLayout->addWidget(f2RowWrap);

    auto* aRowWrap = new QWidget;
    auto* aRow = new QHBoxLayout(aRowWrap);
    aRow->setContentsMargins(0, 0, 0, 0);
    aRow->setSpacing(6);

    aRow->addWidget(new QLabel("Большая полуось a:"));
    auto* aSpin = new QDoubleSpinBox;
    aSpin->setRange(1.0, 5000.0);
    aSpin->setDecimals(0);
    aSpin->setValue(std::round(ellipse->semiMajor));
    aSpin->setFixedWidth(110);
    aRow->addWidget(aSpin);
    aRow->addStretch();

    m_sidesLayout->addWidget(aRowWrap);

    auto* btnRowWrap = new QWidget;
    auto* btnRow = new QHBoxLayout(btnRowWrap);
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);

    auto* circleBtn = new QPushButton("Сделать окружностью");
    btnRow->addWidget(circleBtn);
    btnRow->addStretch();

    m_sidesLayout->addWidget(btnRowWrap);

    auto applyFoci = [this, ellipse, f1x, f1y, f2x, f2y]() {
        if (m_updatingUi) return;

        ellipse->setFromAbsoluteFoci(
            {f1x->value(), f1y->value()},
            {f2x->value(), f2y->value()}
        );

        m_canvas->update();
        QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
    };

    connect(f1x, &QDoubleSpinBox::editingFinished, this, applyFoci);
    connect(f1y, &QDoubleSpinBox::editingFinished, this, applyFoci);
    connect(f2x, &QDoubleSpinBox::editingFinished, this, applyFoci);
    connect(f2y, &QDoubleSpinBox::editingFinished, this, applyFoci);

    connect(aSpin, &QDoubleSpinBox::editingFinished, this, [this, ellipse, aSpin]() {
        if (m_updatingUi) return;
        ellipse->setSemiMajor(aSpin->value());
        m_canvas->update();
        QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
    });

    connect(circleBtn, &QPushButton::clicked, this, [this, ellipse]() {
        if (m_updatingUi) return;
        ellipse->setFromAbsoluteFoci(ellipse->center(), ellipse->center());
        m_canvas->update();
        QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
    });
}

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
            m_canvas->update();
            QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
        });
        row->addWidget(colorBtn);

        row->addWidget(new QLabel("Толщина:"));
        auto* wSpin = new QSpinBox;
        wSpin->setRange(1, 1000000);
        wSpin->setValue((int)std::round(target->sideWidths[i] / 2.0));
        row->addWidget(wSpin);

        connect(wSpin, &QSpinBox::editingFinished, this, [this, i, target, wSpin]() {
            if (m_updatingUi) return;
            target->sideWidths[i] = wSpin->value() * 2.0;
            m_canvas->update();
            QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
        });

        row->addWidget(new QLabel("Длина:"));
        auto* lenSpin = new QDoubleSpinBox;
        lenSpin->setRange(1.0, 5000.0);
        lenSpin->setDecimals(0);
        lenSpin->setValue(i < (int)lengths.size() ? std::round(lengths[i]) : 0.0);
        row->addWidget(lenSpin);

        connect(lenSpin, &QDoubleSpinBox::editingFinished, this, [this, i, target, lenSpin]() {
            if (m_updatingUi) return;
            target->setSideLength(i, lenSpin->value());
            m_canvas->update();
            QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
        });

        if (target->supportsAngleEditing() && i < (int)angles.size()) {
            row->addWidget(new QLabel("Угол:"));
            auto* angSpin = new QDoubleSpinBox;
            angSpin->setRange(1.0, 359.0);
            angSpin->setDecimals(0);
            angSpin->setValue(std::round(angles[i]));
            row->addWidget(angSpin);

            connect(angSpin, &QDoubleSpinBox::editingFinished, this, [this, i, target, angSpin]() {
                if (m_updatingUi) return;
                target->setAngleAt(i, angSpin->value());
                m_canvas->update();
                QTimer::singleShot(0, this, [this]() { updatePropertiesPanel(); });
            });
        }

        row->addStretch();
        col->addLayout(row);
        m_sidesLayout->addWidget(rowWidget);
    }

    m_sidesLayout->addStretch();
}

void MainWindow::updatePropertiesPanel() {
    m_updatingUi = true;

    Shape* target = currentPropertiesShape();

    bool showVertices = false;
    if (auto* poly = dynamic_cast<PolygonShape*>(target)) {
        showVertices = poly->supportsVerticesEditing();
    }

    if (m_verticesGroup) {
        m_verticesGroup->setVisible(showVertices);
    }

    if (!target) {
        m_boundsLabel->setText("Фигура не выбрана");

        m_anchorX->setValue(0);
        m_anchorY->setValue(0);
        m_anchorInsideX->setValue(0);
        m_anchorInsideY->setValue(0);

        m_scaleSpin->setValue(100.0);

        m_pendingFill = Qt::transparent;
        m_noFillCheck->setChecked(true);
        m_noFillCheck->setEnabled(false);
        m_fillButton->setEnabled(false);

        if (m_rotationGroup) {
            m_rotationGroup->setVisible(false);
        }

        if (m_rotationSpin) {
            m_rotationSpin->setValue(0.0);
            m_rotationSpin->setEnabled(false);
        }

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
        QString("Тип: %1\nID: %2\nВиртуальные границы: левый верхний (%3, %4),\nправый нижний (%5, %6)")
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

    auto* ellipse = dynamic_cast<EllipseShape*>(target);

    if (m_rotationGroup) {
        m_rotationGroup->setVisible(ellipse != nullptr);
    }

    if (m_rotationSpin) {
        if (ellipse) {
            m_rotationSpin->setEnabled(true);
            m_rotationSpin->setValue(ellipse->rotationDeg);
        } else {
            m_rotationSpin->setEnabled(false);
            m_rotationSpin->setValue(0.0);
        }
    }

    rebuildVerticesTable();
    rebuildSidesPanel();
    rebuildGroupChildrenList();

    m_updatingUi = false;
}