// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtDBus/QDBusConnection>

#include <memory>
#include <optional>

namespace QindaQt::Power::Upstream {

// Selection happens once, in the resident composition root: the packaged
// descriptor and user unit pass production explicitly, and the bare binary
// keeps the PB-1 unavailable default so an unconfigured process never touches
// a host bus. See docs/wiki/architecture/power-service.md and ADR-0060.
enum class UpstreamMode {
    Unavailable = 0,
    Production = 1,
};

[[nodiscard]] std::optional<UpstreamMode> parseUpstreamMode(const QString &value);

struct UpstreamComposition {
    std::unique_ptr<BatteryCollaborator> battery;
    std::unique_ptr<ProfileCollaborator> profiles;
    std::unique_ptr<SessionCollaborator> session;
};

// Builds the three collaborators for one mode. The upstream bus and the
// backlight sysfs root are always injected; the unavailable mode ignores both
// and never opens them.
[[nodiscard]] UpstreamComposition composeUpstream(
    UpstreamMode mode, const QDBusConnection &upstreamBus,
    const QString &backlightRoot);

} // namespace QindaQt::Power::Upstream
