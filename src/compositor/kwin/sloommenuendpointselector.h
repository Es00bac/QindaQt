// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

namespace QindaQt::Compositor::KWinIntegration
{

struct SloomMenuEndpoint final
{
    QString serviceName;
    QString objectPath;

    bool operator==(const SloomMenuEndpoint &) const = default;
};

// Chooses Sloom Studio's documented native-Wayland dbusmenu endpoint only for
// a known Sloom KWin identity and only when KWin reports no appmenu address at
// all. The caller must still resolve the well-known service to its exact owner
// and authenticate that owner against the active surface PID.
[[nodiscard]] std::optional<SloomMenuEndpoint> selectSloomMenuEndpoint(
    const QString &desktopFileName, const QString &resourceClass,
    const QString &announcedServiceName, const QString &announcedObjectPath);

} // namespace QindaQt::Compositor::KWinIntegration
