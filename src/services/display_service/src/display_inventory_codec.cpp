// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_service/display_inventory.h>

#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include "display_inventory_validation_p.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::DisplayService
{
namespace
{

InventoryDecodeResult failure(InventoryError error, const char *reason)
{
    return {.frame = {}, .error = error, .reasonCode = QString::fromLatin1(reason)};
}

bool exactInteger(const QJsonValue &value, qint64 minimum, qint64 maximum,
                  qint64 &destination)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(minimum)
        || number > static_cast<double>(maximum)) {
        return false;
    }
    destination = static_cast<qint64>(number);
    return true;
}

bool integerMember(const QJsonObject &object, const QString &key, qint64 minimum,
                   qint64 maximum, qint64 &destination)
{
    return object.contains(key)
        && exactInteger(object.value(key), minimum, maximum, destination);
}

bool stringMember(const QJsonObject &object, const QString &key, QString &destination)
{
    if (!object.value(key).isString()) {
        return false;
    }
    destination = object.value(key).toString();
    return true;
}

bool boolMember(const QJsonObject &object, const QString &key, bool &destination)
{
    if (!object.value(key).isBool()) {
        return false;
    }
    destination = object.value(key).toBool();
    return true;
}

bool transformValue(const QString &name, Display::Transform &transform)
{
    static const QHash<QString, Display::Transform> transforms{
        {QStringLiteral("normal"), Display::Transform::Normal},
        {QStringLiteral("rotate-90"), Display::Transform::Rotate90},
        {QStringLiteral("rotate-180"), Display::Transform::Rotate180},
        {QStringLiteral("rotate-270"), Display::Transform::Rotate270},
        {QStringLiteral("flip-x"), Display::Transform::FlipX},
        {QStringLiteral("flip-x-90"), Display::Transform::FlipX90},
        {QStringLiteral("flip-x-180"), Display::Transform::FlipX180},
        {QStringLiteral("flip-x-270"), Display::Transform::FlipX270},
    };
    const auto iterator = transforms.constFind(name);
    if (iterator == transforms.cend()) {
        return false;
    }
    transform = iterator.value();
    return true;
}

bool canonicalGeneration(const QString &text, quint64 &generation)
{
    bool converted = false;
    generation = text.toULongLong(&converted, 10);
    return converted && generation > 0 && QString::number(generation) == text;
}

bool geometryValue(const QJsonObject &object, QRect &geometry)
{
    qint64 x = 0;
    qint64 y = 0;
    qint64 width = 0;
    qint64 height = 0;
    if (!integerMember(object, QStringLiteral("x"), -Display::kCoordinateBound,
                       Display::kCoordinateBound, x)
        || !integerMember(object, QStringLiteral("y"), -Display::kCoordinateBound,
                          Display::kCoordinateBound, y)
        || !integerMember(object, QStringLiteral("width"), 1,
                          Display::kCoordinateBound, width)
        || !integerMember(object, QStringLiteral("height"), 1,
                          Display::kCoordinateBound, height)) {
        return false;
    }
    const qint64 right = x + width;
    const qint64 bottom = y + height;
    if (right < -Display::kCoordinateBound || right > Display::kCoordinateBound
        || bottom < -Display::kCoordinateBound
        || bottom > Display::kCoordinateBound) {
        return false;
    }
    geometry = QRect(static_cast<int>(x), static_cast<int>(y),
                     static_cast<int>(width), static_cast<int>(height));
    return true;
}

bool sizeValue(const QJsonObject &object, QSize &size)
{
    qint64 width = 0;
    qint64 height = 0;
    if (!integerMember(object, QStringLiteral("width"), 0,
                       Display::kMaxPhysicalDimensionMillimeters, width)
        || !integerMember(object, QStringLiteral("height"), 0,
                          Display::kMaxPhysicalDimensionMillimeters, height)) {
        return false;
    }
    size = QSize(static_cast<int>(width), static_cast<int>(height));
    return true;
}

bool decodeOutput(const QJsonValue &value, InventoryOutput &output)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    QString transformName;
    qint64 refresh = 0;
    qint64 priority = 0;
    if (!stringMember(object, QStringLiteral("name"), output.name)
        || !object.value(QStringLiteral("geometry")).isObject()
        || !geometryValue(object.value(QStringLiteral("geometry")).toObject(),
                          output.geometry)
        || !object.value(QStringLiteral("scale")).isDouble()
        || !integerMember(object, QStringLiteral("refreshRateMilliHz"), 1,
                          Display::kMaxRefreshMilliHertz, refresh)
        || !stringMember(object, QStringLiteral("transform"), transformName)
        || !transformValue(transformName, output.transform)
        || !boolMember(object, QStringLiteral("internal"), output.internal)
        || !stringMember(object, QStringLiteral("uuid"),
                         output.runtimeCompositorUuid)
        || !integerMember(object, QStringLiteral("priority"), 0,
                          std::numeric_limits<quint32>::max(), priority)
        || !object.value(QStringLiteral("physicalSizeMm")).isObject()
        || !sizeValue(object.value(QStringLiteral("physicalSizeMm")).toObject(),
                      output.physicalSizeMillimeters)
        || !stringMember(object, QStringLiteral("manufacturer"),
                         output.manufacturer)
        || !stringMember(object, QStringLiteral("model"), output.model)) {
        return false;
    }
    output.scale = object.value(QStringLiteral("scale")).toDouble();
    if (!std::isfinite(output.scale) || output.scale < Display::kMinimumScale
        || output.scale > Display::kMaximumScale || output.name.isEmpty()
        || !Display::isBoundedText(output.name,
                                   Display::kMaxConnectorNameUtf8Bytes)
        || !Display::isBoundedText(output.runtimeCompositorUuid,
                                   Display::kMaxRuntimeUuidUtf8Bytes)
        || !Display::isBoundedText(output.manufacturer,
                                   Display::kMaxManufacturerUtf8Bytes)
        || !Display::isBoundedText(output.model, Display::kMaxModelUtf8Bytes)) {
        return false;
    }
    output.refreshRateMilliHertz = static_cast<quint32>(refresh);
    output.compositorPriority = static_cast<quint32>(priority);
    return true;
}

} // namespace

InventoryDecodeResult decodeCompositorInventory(const QByteArrayView payload,
                                                const QString &uniqueOwner)
{
    if (payload.size() > kMaximumCompositorInventoryBytes) {
        return failure(InventoryError::PayloadTooLarge, "inventory-payload-too-large");
    }
    if (!Private::validUniqueBusOwner(uniqueOwner)) {
        return failure(InventoryError::InvalidOwner, "invalid-source-owner");
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(payload.data(), payload.size()), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(InventoryError::MalformedPayload, "malformed-inventory-json");
    }
    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("status")).isString()) {
        return failure(InventoryError::MalformedPayload, "missing-inventory-status");
    }
    const QString status = root.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unavailable")) {
        return failure(InventoryError::Unavailable, "inventory-unavailable");
    }
    if (status != QStringLiteral("ok")) {
        return failure(InventoryError::MalformedPayload, "unknown-inventory-status");
    }

    qint64 schemaVersion = 0;
    if (!integerMember(root, QStringLiteral("schemaVersion"), 1, 1, schemaVersion)) {
        return failure(InventoryError::UnsupportedSchema,
                       "unsupported-inventory-schema");
    }
    (void)schemaVersion;
    QString generationText;
    quint64 generation = 0;
    if (!stringMember(root, QStringLiteral("outputGeneration"), generationText)
        || !canonicalGeneration(generationText, generation)) {
        return failure(InventoryError::InvalidGeneration,
                       "invalid-output-generation");
    }
    if (!root.value(QStringLiteral("outputs")).isArray()) {
        return failure(InventoryError::MalformedPayload, "missing-inventory-outputs");
    }
    const QJsonArray array = root.value(QStringLiteral("outputs")).toArray();
    if (array.isEmpty() || array.size() > Display::kMaxOutputs) {
        return failure(InventoryError::InvalidOutput, "invalid-output-count");
    }

    QList<InventoryOutput> outputs;
    QSet<QString> names;
    QSet<QString> runtimeUuids;
    outputs.reserve(array.size());
    for (const QJsonValue &value : array) {
        InventoryOutput output;
        if (!decodeOutput(value, output) || names.contains(output.name)
            || (!output.runtimeCompositorUuid.isEmpty()
                && runtimeUuids.contains(output.runtimeCompositorUuid))) {
            return failure(InventoryError::InvalidOutput,
                           "malformed-or-ambiguous-output");
        }
        names.insert(output.name);
        if (!output.runtimeCompositorUuid.isEmpty()) {
            runtimeUuids.insert(output.runtimeCompositorUuid);
        }
        outputs.push_back(std::move(output));
    }
    return {.frame = {.uniqueOwner = uniqueOwner,
                      .outputGeneration = generation,
                      .outputs = std::move(outputs)},
            .error = InventoryError::None,
            .reasonCode = {}};
}

} // namespace QindaQt::DisplayService
