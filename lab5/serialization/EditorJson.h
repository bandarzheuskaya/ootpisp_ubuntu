#pragma once

#include <QString>
#include <vector>
#include <memory>

class Shape;

struct EditorDocumentData {
    double pageWidth = 3200.0;
    double pageHeight = 2200.0;
    double viewScale = 1.0;
    double viewOffsetX = 120.0;
    double viewOffsetY = 80.0;
    int nextId = 1;
    std::vector<std::unique_ptr<Shape>> shapes;
};

namespace EditorJson {
    bool saveDocument(
        const QString& fileName,
        const std::vector<std::unique_ptr<Shape>>& shapes,
        double pageWidth,
        double pageHeight,
        double viewScale,
        double viewOffsetX,
        double viewOffsetY,
        int nextId,
        QString* errorMessage = nullptr
    );

    bool loadDocument(
        const QString& fileName,
        EditorDocumentData& data,
        QString* errorMessage = nullptr
    );

    bool saveSingleShape(
        const QString& fileName,
        const Shape& shape,
        QString* errorMessage = nullptr
    );

    std::unique_ptr<Shape> loadSingleShape(
        const QString& fileName,
        QString* errorMessage = nullptr
    );
}