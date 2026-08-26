// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/hybrid_constraints/window_restore_state.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <cmath>

namespace QindaQt::HybridConstraints {
namespace {

using FlagBits = quint32;

constexpr FlagBits maximizeMask = FlagBits(MaximizeAxis::Horizontal)
    | FlagBits(MaximizeAxis::Vertical);
constexpr FlagBits quickTileMask = FlagBits(QuickTileEdge::Left)
    | FlagBits(QuickTileEdge::Right) | FlagBits(QuickTileEdge::Top)
    | FlagBits(QuickTileEdge::Bottom);

void setError(QString *error, const QString &message)
{
    if (error != nullptr) {
        *error = message;
    }
}

bool isFiniteGeometry(const QRectF &geometry) noexcept
{
    return std::isfinite(geometry.x()) && std::isfinite(geometry.y())
        && std::isfinite(geometry.width()) && std::isfinite(geometry.height())
        && geometry.width() >= 0.0 && geometry.height() >= 0.0;
}

bool hasUniqueNonEmptyIds(const QStringList &ids) noexcept
{
    QSet<QString> seen;
    for (const auto &id : ids) {
        if (id.isEmpty() || seen.contains(id)) {
            return false;
        }
        seen.insert(id);
    }
    return true;
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray array;
    for (const auto &value : values) {
        array.append(value);
    }
    return array;
}

bool readRequiredBoolean(const QJsonObject &object,
                         QLatin1StringView key,
                         bool *value,
                         QString *error)
{
    const auto candidate = object.value(key);
    if (!candidate.isBool()) {
        setError(error, QString::fromLatin1(key.data(), key.size())
                            + QStringLiteral(" must be a boolean"));
        return false;
    }
    *value = candidate.toBool();
    return true;
}

bool readIntegerFlags(const QJsonObject &object,
                      QLatin1StringView key,
                      FlagBits knownMask,
                      FlagBits *value,
                      QString *error)
{
    const auto candidate = object.value(key);
    const double number = candidate.toDouble(-1.0);
    if (!candidate.isDouble() || !std::isfinite(number) || std::floor(number) != number
        || number < 0.0 || number > double(knownMask)
        || (static_cast<FlagBits>(number) & ~knownMask) != 0U) {
        setError(error, QString::fromLatin1(key.data(), key.size())
                            + QStringLiteral(" contains unknown state flags"));
        return false;
    }
    *value = static_cast<FlagBits>(number);
    return true;
}

bool readStringList(const QJsonObject &object,
                    QLatin1StringView key,
                    QStringList *values,
                    QString *error)
{
    const auto candidate = object.value(key);
    if (!candidate.isArray()) {
        setError(error, QString::fromLatin1(key.data(), key.size())
                            + QStringLiteral(" must be an array"));
        return false;
    }
    for (const auto &entry : candidate.toArray()) {
        if (!entry.isString()) {
            setError(error, QString::fromLatin1(key.data(), key.size())
                                + QStringLiteral(" entries must be strings"));
            return false;
        }
        values->append(entry.toString());
    }
    return true;
}

std::optional<QRectF> readGeometry(const QJsonObject &object, QString *error)
{
    const auto value = object.value(QLatin1String("geometry"));
    if (!value.isObject()) {
        setError(error, QStringLiteral("geometry must be an object"));
        return std::nullopt;
    }
    const auto geometry = value.toObject();
    constexpr QLatin1StringView keys[]{QLatin1StringView("x"),
                                       QLatin1StringView("y"),
                                       QLatin1StringView("width"),
                                       QLatin1StringView("height")};
    double parts[4]{};
    for (int index = 0; index < 4; ++index) {
        const auto candidate = geometry.value(keys[index]);
        if (!candidate.isDouble() || !std::isfinite(candidate.toDouble())) {
            setError(error, QStringLiteral("geometry components must be finite numbers"));
            return std::nullopt;
        }
        parts[index] = candidate.toDouble();
    }
    QRectF result(parts[0], parts[1], parts[2], parts[3]);
    if (!isFiniteGeometry(result)) {
        setError(error, QStringLiteral("geometry size must be non-negative"));
        return std::nullopt;
    }
    return result;
}

} // namespace

bool WindowRestoreState::isValid(QString *error) const
{
    if (!isFiniteGeometry(geometry)) {
        setError(error, QStringLiteral("geometry must be finite with a non-negative size"));
        return false;
    }
    if ((FlagBits(maximizedAxes.toInt()) & ~maximizeMask) != 0U
        || (FlagBits(quickTileEdges.toInt()) & ~quickTileMask) != 0U) {
        setError(error, QStringLiteral("state contains unknown maximize or quick-tile flags"));
        return false;
    }
    if (!hasUniqueNonEmptyIds(desktopIds) || !hasUniqueNonEmptyIds(activityIds)) {
        setError(error, QStringLiteral("desktop and activity IDs must be non-empty and unique"));
        return false;
    }
    if (keepAbove && keepBelow) {
        setError(error, QStringLiteral("a window cannot be kept both above and below"));
        return false;
    }
    return true;
}

QJsonObject WindowRestoreState::toJson() const
{
    // AGENT-GUARD: Keep every restoration field in this representation. A
    // lossy bridge can make ungrouping permanently change the user's window.
    return {
        {QStringLiteral("schemaVersion"), JsonSchemaVersion},
        {QStringLiteral("geometry"),
         QJsonObject{{QStringLiteral("x"), geometry.x()},
                     {QStringLiteral("y"), geometry.y()},
                     {QStringLiteral("width"), geometry.width()},
                     {QStringLiteral("height"), geometry.height()}}},
        {QStringLiteral("minimized"), minimized},
        {QStringLiteral("maximizedAxes"), int(maximizedAxes.toInt())},
        {QStringLiteral("quickTileEdges"), int(quickTileEdges.toInt())},
        {QStringLiteral("fullscreen"), fullscreen},
        {QStringLiteral("outputId"), outputId},
        {QStringLiteral("desktopIds"), stringArray(desktopIds)},
        {QStringLiteral("activityIds"), stringArray(activityIds)},
        {QStringLiteral("keepAbove"), keepAbove},
        {QStringLiteral("keepBelow"), keepBelow},
        {QStringLiteral("focused"), focused},
        {QStringLiteral("skipTaskbar"), skipTaskbar},
        {QStringLiteral("skipSwitcher"), skipSwitcher},
    };
}

std::optional<WindowRestoreState> WindowRestoreState::fromJson(const QJsonObject &object,
                                                               QString *error)
{
    const auto schemaValue = object.value(QLatin1String("schemaVersion"));
    const double schemaNumber = schemaValue.toDouble(-1.0);
    if (!schemaValue.isDouble() || !std::isfinite(schemaNumber)
        || std::floor(schemaNumber) != schemaNumber
        || (schemaNumber != 1.0 && schemaNumber != double(JsonSchemaVersion))) {
        setError(error, QStringLiteral("unsupported window restore state schema version"));
        return std::nullopt;
    }

    WindowRestoreState state;
    const auto geometry = readGeometry(object, error);
    if (!geometry.has_value()) {
        return std::nullopt;
    }
    state.geometry = *geometry;

    FlagBits maximizedAxes = 0;
    FlagBits quickTileEdges = 0;
    if (!readRequiredBoolean(object, QLatin1String("minimized"), &state.minimized, error)
        || !readIntegerFlags(object, QLatin1String("maximizedAxes"), maximizeMask,
                             &maximizedAxes, error)
        || !readIntegerFlags(object, QLatin1String("quickTileEdges"), quickTileMask,
                             &quickTileEdges, error)
        || !readRequiredBoolean(object, QLatin1String("fullscreen"), &state.fullscreen, error)
        || !readRequiredBoolean(object, QLatin1String("keepAbove"), &state.keepAbove, error)
        || !readRequiredBoolean(object, QLatin1String("keepBelow"), &state.keepBelow, error)
        || !readRequiredBoolean(object, QLatin1String("focused"), &state.focused, error)
        || !readStringList(object, QLatin1String("desktopIds"), &state.desktopIds, error)
        || !readStringList(object, QLatin1String("activityIds"), &state.activityIds, error)) {
        return std::nullopt;
    }

    if (schemaNumber >= 2.0
        && (!readRequiredBoolean(object, QLatin1String("skipTaskbar"),
                                 &state.skipTaskbar, error)
            || !readRequiredBoolean(object, QLatin1String("skipSwitcher"),
                                    &state.skipSwitcher, error))) {
        return std::nullopt;
    }

    const auto output = object.value(QLatin1String("outputId"));
    if (!output.isString()) {
        setError(error, QStringLiteral("outputId must be a string"));
        return std::nullopt;
    }
    state.outputId = output.toString();
    state.maximizedAxes = MaximizeAxes::fromInt(
        static_cast<MaximizeAxes::Int>(maximizedAxes));
    state.quickTileEdges = QuickTileEdges::fromInt(
        static_cast<QuickTileEdges::Int>(quickTileEdges));

    if (!state.isValid(error)) {
        return std::nullopt;
    }
    return state;
}

} // namespace QindaQt::HybridConstraints
