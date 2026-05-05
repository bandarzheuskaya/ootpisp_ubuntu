#include "EditorJson.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "Shape.h"

namespace {

void removeIdsRecursively(QJsonObject& obj) {
    obj.remove("id");

    if (!obj.contains("children") || !obj["children"].isArray()) {
        return;
    }

    QJsonArray children = obj["children"].toArray();

    for (int i = 0; i < children.size(); ++i) {
        QJsonObject child = children[i].toObject();
        removeIdsRecursively(child);
        children[i] = child;
    }

    obj["children"] = children;
}

}

bool EditorJson::saveDocument(
    const QString& fileName,
    const std::vector<std::unique_ptr<Shape>>& shapes,
    double pageWidth,
    double pageHeight,
    double viewScale,
    double viewOffsetX,
    double viewOffsetY,
    int nextId,
    QString* errorMessage
) {
    QJsonObject root;
    root["format"] = "shape_editor";
    root["version"] = 2;
    root["nextId"] = nextId;
    root["pageWidth"] = pageWidth;
    root["pageHeight"] = pageHeight;
    root["viewScale"] = viewScale;
    root["viewOffsetX"] = viewOffsetX;
    root["viewOffsetY"] = viewOffsetY;

    QJsonArray shapesArray;
    for (const auto& shape : shapes) {
        if (shape) shapesArray.append(shape->save());
    }
    root["shapes"] = shapesArray;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = "Не удалось открыть файл для записи.";
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool EditorJson::loadDocument(
    const QString& fileName,
    EditorDocumentData& data,
    QString* errorMessage
) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = "Не удалось открыть файл.";
        return false;
    }

    QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) *errorMessage = "Файл повреждён или имеет неверный формат.";
        return false;
    }

    QJsonObject root = doc.object();

    if (root["format"].toString() != "shape_editor") {
        if (errorMessage) {
            *errorMessage = "Это не полный файл проекта. Возможно, выбран файл отдельной фигуры. Используйте пункт «Загрузить фигуру».";
        }
        return false;
    }

    if (!root.contains("shapes") || !root["shapes"].isArray()) {
        if (errorMessage) {
            *errorMessage = "В файле проекта отсутствует список фигур.";
        }
        return false;
    }

    data.pageWidth = root["pageWidth"].toDouble(3200.0);
    data.pageHeight = root["pageHeight"].toDouble(2200.0);
    data.viewScale = root["viewScale"].toDouble(1.0);
    data.viewOffsetX = root["viewOffsetX"].toDouble(120.0);
    data.viewOffsetY = root["viewOffsetY"].toDouble(80.0);
    data.nextId = root["nextId"].toInt(1);
    data.shapes.clear();

    QJsonArray arr = root["shapes"].toArray();

    for (const auto& v : arr) {
        if (!v.isObject()) continue;

        auto shape = Shape::load(v.toObject());
        if (shape) {
            data.shapes.push_back(std::move(shape));
        }
    }

    return true;
}

bool EditorJson::saveSingleShape(
    const QString& fileName,
    const Shape& shape,
    QString* errorMessage
) {
    QJsonObject obj = shape.save();

    removeIdsRecursively(obj);

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = "Не удалось открыть файл для записи.";
        return false;
    }

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

std::unique_ptr<Shape> EditorJson::loadSingleShape(
    const QString& fileName,
    QString* errorMessage
) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = "Не удалось открыть файл.";
        return nullptr;
    }

    QByteArray raw = file.readAll();
    file.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) *errorMessage = "Файл повреждён или имеет неверный формат.";
        return nullptr;
    }

    auto shape = Shape::load(doc.object());
    if (!shape && errorMessage) {
        *errorMessage = "Не удалось загрузить фигуру.";
    }

    return shape;
}