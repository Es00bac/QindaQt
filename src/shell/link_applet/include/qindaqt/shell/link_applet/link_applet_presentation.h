// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_client/link_client.h>
#include <qindaqt/shell/link_applet/link_applet_types.h>

#include <QtCore/QList>

#include <optional>

namespace QindaQt::Shell::LinkApplet {

struct PendingPairing final {
    QString deviceId, name, model, code; // code is the raw six digits
};

struct ProjectionContext final {
    bool serviceAvailable = false;
    QList<Link::LinkDevice> devices;
    std::optional<PendingPairing> pairing;
    qint64 nowMs = 0; // injected so "last seen" text is deterministic
};

// Pure and deterministic (docs/wiki/shell/link-applet.md).
[[nodiscard]] LinkAppletModel projectLinkApplet(const ProjectionContext &context);

// "007123" -> "007 123"; anything that is not exactly six ASCII digits gives
// an empty string, which the controller treats as "not a pairing request".
[[nodiscard]] QString formatPairingCode(const QString &raw);

} // namespace QindaQt::Shell::LinkApplet
