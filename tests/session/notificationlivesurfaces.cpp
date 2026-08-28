// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationlivesurfaces.h"

#include "compositorprobeclient.h"
#include "notificationliveevidenceclient.h"
#include "notificationliveruntime.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>
#include <optional>

namespace QindaQt::Test {
namespace {

std::optional<QJsonObject> findUniqueSurface(const QJsonArray &surfaces,
                                             QLatin1StringView scope)
{
    std::optional<QJsonObject> result;
    int popupCount = 0;
    int centerCount = 0;
    for (const QJsonValue &value : surfaces) {
        const QJsonObject surface = value.toObject();
        if (surface.value(QStringLiteral("scope"))
            == QStringLiteral("notification-popup")) {
            ++popupCount;
        } else if (surface.value(QStringLiteral("scope"))
                   == QStringLiteral("notification-center")) {
            ++centerCount;
        }
        if (surface.value(QStringLiteral("scope")) == scope) {
            if (result) {
                return std::nullopt;
            }
            result = surface;
        }
    }
    return popupCount <= 1 && centerCount <= 1 ? result : std::nullopt;
}

bool anchorsAreTopRight(const QJsonArray &anchors)
{
    return anchors.size() == 2 && anchors.contains(QStringLiteral("top"))
        && anchors.contains(QStringLiteral("right"));
}

bool numbersMatch(const QJsonObject &first, const QJsonObject &second,
                  QLatin1StringView field)
{
    return std::abs(first.value(field).toDouble()
                    - second.value(field).toDouble())
        < 0.01;
}

} // namespace

bool validateNotificationLiveSurface(
    QLatin1StringView scope, const QJsonObject &shellSnapshot,
    const NotificationLiveExpectations &expectations,
    CompositorProbeClient &compositor, QString *error)
{
    const QLatin1StringView role = scope == QLatin1StringView("notification-center")
        ? QLatin1StringView("center")
        : QLatin1StringView("popup");
    const QJsonObject shellWindow = windowEvidence(shellSnapshot, role);
    if (!shellWindow.value(QStringLiteral("visible")).toBool()) {
        *error = QStringLiteral("shell did not report mapped %1").arg(scope);
        return false;
    }

    std::optional<QJsonObject> observed;
    if (!awaitNotificationLiveCondition([&] {
            QString queryError;
            const auto surfaces = compositor.developmentShellSurfaces(&queryError);
            if (!surfaces) {
                *error = std::move(queryError);
                return false;
            }
            observed = findUniqueSurface(*surfaces, scope);
            if (!observed || !observed->value(QStringLiteral("committed")).toBool()
                || !observed->value(QStringLiteral("mapped")).toBool()) {
                return false;
            }
            const QJsonObject geometry =
                observed->value(QStringLiteral("geometry")).toObject();
            const QJsonObject desired =
                observed->value(QStringLiteral("desiredSize")).toObject();
            return numbersMatch(geometry, desired, QLatin1StringView("width"))
                && numbersMatch(geometry, desired, QLatin1StringView("height"));
        })) {
        if (error->isEmpty()) {
            *error = QStringLiteral("compositor did not map %1").arg(scope);
        }
        return false;
    }

    const QJsonObject surface = *observed;
    const QJsonObject geometry = surface.value(QStringLiteral("geometry")).toObject();
    const QJsonObject desired =
        surface.value(QStringLiteral("desiredSize")).toObject();
    const QJsonObject shellGeometry =
        shellWindow.value(QStringLiteral("geometry")).toObject();
    const QJsonObject margins = surface.value(QStringLiteral("margins")).toObject();
    const QString outputName = shellWindow.value(QStringLiteral("outputName")).toString();
    const bool center = role == QLatin1StringView("center");
    const double expectedX = expectations.logicalWidth
        - desired.value(QStringLiteral("width")).toDouble() - 16.0;
    const double observedY = geometry.value(QStringLiteral("y")).toDouble();
    const double maximumY = expectations.logicalHeight
        - desired.value(QStringLiteral("height")).toDouble();
    if (surface.value(QStringLiteral("processId")).toString().toLongLong()
            != expectations.shellProcessId
        || surface.value(QStringLiteral("layer")) != QStringLiteral("overlay")
        || !anchorsAreTopRight(surface.value(QStringLiteral("anchors")).toArray())
        || margins.value(QStringLiteral("left")).toInt(-1) != 0
        || margins.value(QStringLiteral("top")).toInt(-1) != 16
        || margins.value(QStringLiteral("right")).toInt(-1) != 16
        || margins.value(QStringLiteral("bottom")).toInt(-1) != 0
        || surface.value(QStringLiteral("exclusiveZone")).toInt(-1) != 0
        || outputName.isEmpty()
        || surface.value(QStringLiteral("outputName")).toString() != outputName
        || surface.value(QStringLiteral("desiredOutputName")).toString()
            != outputName
        || !surface.value(QStringLiteral("acceptsFocus")).toBool()
        // KWin may designate an on-demand popup as its active window when the
        // disposable session has no incumbent application. The client-side
        // activation event is the keyboard-focus authority: popup stays false,
        // while the explicitly opened center must become true.
        || (center != shellWindow.value(QStringLiteral("active")).toBool())
        || !numbersMatch(geometry, desired, QLatin1StringView("width"))
        || !numbersMatch(geometry, desired, QLatin1StringView("height"))
        || !numbersMatch(shellGeometry, desired, QLatin1StringView("width"))
        || !numbersMatch(shellGeometry, desired, QLatin1StringView("height"))
        || std::abs(geometry.value(QStringLiteral("x")).toDouble() - expectedX)
            >= 0.01
        // A zero-exclusive-zone layer surface is offset below any earlier
        // exclusive top shell surface. Its requested 16px margin is therefore
        // a lower bound rather than an absolute global Y coordinate.
        || observedY < 16.0 || observedY > maximumY) {
        // AGENT-NOTE: Keep both authority snapshots in a failing live-row
        // diagnostic. Without the values, every protocol/geometry mismatch
        // collapses to the same message after the disposable session exits.
        *error = QStringLiteral(
                     "compositor-owned %1 role/output/geometry invalid: "
                     "surface=%2 shell=%3")
                     .arg(scope,
                          QString::fromUtf8(QJsonDocument(surface).toJson(
                              QJsonDocument::Compact)),
                          QString::fromUtf8(QJsonDocument(shellWindow).toJson(
                              QJsonDocument::Compact)));
        return false;
    }
    return true;
}

} // namespace QindaQt::Test
