// SPDX-License-Identifier: GPL-3.0-or-later
#include "compositoroutputauthority.h"

#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Shell {
namespace {

using DecodeError = CompositorOutputAuthorityDecodeError;
using DecodeResult = CompositorOutputAuthorityDecodeResult;

DecodeResult failure(DecodeError error, QString message)
{
    return {{}, error, std::move(message)};
}

bool exactInteger(const QJsonValue &value, quint32 *destination)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < 0.0
        || number > static_cast<double>(std::numeric_limits<quint32>::max())) {
        return false;
    }
    *destination = static_cast<quint32>(number);
    return true;
}

bool exactInteger(const QJsonValue &value, quint32 maximum)
{
    quint32 result = 0;
    return exactInteger(value, &result) && result <= maximum;
}

bool canonicalGeneration(const QJsonValue &value, quint64 *generation)
{
    if (!value.isString()) {
        return false;
    }
    const QString text = value.toString();
    bool converted = false;
    const quint64 parsed = text.toULongLong(&converted, 10);
    if (!converted || parsed == 0 || QString::number(parsed) != text) {
        return false;
    }
    *generation = parsed;
    return true;
}

bool validUniqueOwner(const QString &owner)
{
    if (owner.size() < 4 || owner.size() > 255 || !owner.startsWith(QLatin1Char(':'))) {
        return false;
    }
    bool segmentHasCharacter = false;
    bool hasSeparator = false;
    for (qsizetype index = 1; index < owner.size(); ++index) {
        const QChar character = owner.at(index);
        if (character == QLatin1Char('.')) {
            if (!segmentHasCharacter) {
                return false;
            }
            segmentHasCharacter = false;
            hasSeparator = true;
            continue;
        }
        if (!character.isLetterOrNumber() && character != QLatin1Char('_')
            && character != QLatin1Char('-')) {
            return false;
        }
        segmentHasCharacter = true;
    }
    return hasSeparator && segmentHasCharacter;
}

bool validText(const QString &value, qsizetype maximumUtf8Bytes,
               qsizetype maximumCharacters, bool required)
{
    if ((required && value.isEmpty()) || value.size() > maximumCharacters
        || value.toUtf8().size() > maximumUtf8Bytes
        || value.contains(QChar::Null)) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
        if (character.isHighSurrogate()) {
            if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

bool validOutputId(const QString &value)
{
    using Limits = ShellVisibilityProtocol::WireLimits;
    return validText(value, Limits::MaxIdentifierCharacters * 4,
                     Limits::MaxIdentifierCharacters, true);
}

bool validGeometry(const QJsonValue &value)
{
    constexpr double CoordinateBound = 1'000'000.0;
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue xValue = object.value(QStringLiteral("x"));
    const QJsonValue yValue = object.value(QStringLiteral("y"));
    const QJsonValue widthValue = object.value(QStringLiteral("width"));
    const QJsonValue heightValue = object.value(QStringLiteral("height"));
    if (!xValue.isDouble() || !yValue.isDouble() || !widthValue.isDouble()
        || !heightValue.isDouble()) {
        return false;
    }
    const double x = xValue.toDouble();
    const double y = yValue.toDouble();
    const double width = widthValue.toDouble();
    const double height = heightValue.toDouble();
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(width)
        && std::isfinite(height) && width > 0.0 && height > 0.0
        && std::abs(x) <= CoordinateBound && std::abs(y) <= CoordinateBound
        && width <= CoordinateBound && height <= CoordinateBound
        && std::abs(x + width) <= CoordinateBound
        && std::abs(y + height) <= CoordinateBound;
}

bool validTransform(const QJsonValue &value)
{
    static const QSet<QString> allowed{
        QStringLiteral("normal"), QStringLiteral("rotate-90"),
        QStringLiteral("rotate-180"), QStringLiteral("rotate-270"),
        QStringLiteral("flip-x"), QStringLiteral("flip-x-90"),
        QStringLiteral("flip-x-180"), QStringLiteral("flip-x-270"),
    };
    return value.isString() && allowed.contains(value.toString());
}

bool validPhysicalSize(const QJsonValue &value)
{
    constexpr quint32 MaximumDimensionMillimeters = 10'000;
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    return exactInteger(object.value(QStringLiteral("width")),
                        MaximumDimensionMillimeters)
        && exactInteger(object.value(QStringLiteral("height")),
                        MaximumDimensionMillimeters);
}

bool validOutputObject(const QJsonObject &object, QString *outputId,
                       quint32 *priority, QString *runtimeUuid)
{
    using Limits = ShellVisibilityProtocol::WireLimits;
    const QJsonValue scaleValue = object.value(QStringLiteral("scale"));
    const double scale = scaleValue.toDouble();
    quint32 refreshRate = 0;
    if (!object.value(QStringLiteral("name")).isString()
        || !validOutputId(object.value(QStringLiteral("name")).toString())
        || !validGeometry(object.value(QStringLiteral("geometry")))
        || !scaleValue.isDouble() || !std::isfinite(scale) || scale <= 0.0
        || scale > Limits::MaxOutputScale
        || !exactInteger(object.value(QStringLiteral("refreshRateMilliHz")),
                         &refreshRate)
        || refreshRate == 0
        || !validTransform(object.value(QStringLiteral("transform")))
        || !object.value(QStringLiteral("internal")).isBool()
        || !object.value(QStringLiteral("uuid")).isString()
        || !validText(object.value(QStringLiteral("uuid")).toString(), 128,
                      128, false)
        || !exactInteger(object.value(QStringLiteral("priority")), priority)
        || !validPhysicalSize(object.value(QStringLiteral("physicalSizeMm")))
        || !object.value(QStringLiteral("manufacturer")).isString()
        || !validText(object.value(QStringLiteral("manufacturer")).toString(),
                      128, 128, false)
        || !object.value(QStringLiteral("model")).isString()
        || !validText(object.value(QStringLiteral("model")).toString(), 256,
                      256, false)) {
        return false;
    }
    *outputId = object.value(QStringLiteral("name")).toString();
    *runtimeUuid = object.value(QStringLiteral("uuid")).toString();
    return true;
}

} // namespace

CompositorOutputAuthorityDecodeResult CompositorOutputAuthorityDecoder::decode(
    QByteArrayView payload, const QString &uniqueOwner)
{
    using Limits = ShellVisibilityProtocol::WireLimits;
    if (payload.size() > Limits::MaxPayloadBytes) {
        return failure(DecodeError::PayloadTooLarge,
                       QStringLiteral("compositor output payload exceeds the shell limit"));
    }
    if (!validUniqueOwner(uniqueOwner)) {
        return failure(DecodeError::InvalidOwner,
                       QStringLiteral("compositor output owner is not a unique bus name"));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(payload.data(), payload.size()), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(DecodeError::MalformedPayload,
                       QStringLiteral("compositor output payload is not a JSON object"));
    }
    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("status")).isString()) {
        return failure(DecodeError::MalformedPayload,
                       QStringLiteral("compositor output status is missing"));
    }
    const QString status = root.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("unavailable")) {
        return failure(DecodeError::Unavailable,
                       QStringLiteral("compositor output inventory is unavailable"));
    }
    if (status != QStringLiteral("ok")) {
        return failure(DecodeError::MalformedPayload,
                       QStringLiteral("compositor output status is unknown"));
    }

    quint32 schemaVersion = 0;
    if (!exactInteger(root.value(QStringLiteral("schemaVersion")), &schemaVersion)
        || schemaVersion != 1) {
        return failure(DecodeError::UnsupportedSchema,
                       QStringLiteral("compositor output schema is unsupported"));
    }
    quint64 outputGeneration = 0;
    if (!canonicalGeneration(root.value(QStringLiteral("outputGeneration")),
                             &outputGeneration)) {
        return failure(DecodeError::InvalidGeneration,
                       QStringLiteral("compositor output generation is invalid"));
    }
    if (!root.value(QStringLiteral("outputs")).isArray()) {
        return failure(DecodeError::MalformedPayload,
                       QStringLiteral("compositor output collection is missing"));
    }
    const QJsonArray outputs = root.value(QStringLiteral("outputs")).toArray();
    if (outputs.isEmpty() || outputs.size() > Limits::MaxOutputs) {
        return failure(DecodeError::InvalidOutput,
                       QStringLiteral("compositor output count is outside the shell limit"));
    }

    CompositorOutputAuthorityFrame frame;
    frame.uniqueOwner = uniqueOwner;
    frame.outputGeneration = outputGeneration;
    frame.outputs.reserve(outputs.size());
    QSet<QString> outputIds;
    QSet<QString> runtimeUuids;
    for (const QJsonValue &value : outputs) {
        if (!value.isObject()) {
            return failure(DecodeError::InvalidOutput,
                           QStringLiteral("compositor output entry is not an object"));
        }
        const QJsonObject object = value.toObject();
        QString outputId;
        QString runtimeUuid;
        quint32 priority = 0;
        if (!validOutputObject(object, &outputId, &priority, &runtimeUuid)
            || outputIds.contains(outputId)
            || (!runtimeUuid.isEmpty() && runtimeUuids.contains(runtimeUuid))) {
            return failure(DecodeError::InvalidOutput,
                           QStringLiteral("compositor output identity is invalid or ambiguous"));
        }
        outputIds.insert(outputId);
        if (!runtimeUuid.isEmpty()) {
            runtimeUuids.insert(runtimeUuid);
        }
        frame.outputs.append({outputId, priority});
    }
    return {std::move(frame), DecodeError::None, {}};
}

} // namespace QindaQt::Shell
