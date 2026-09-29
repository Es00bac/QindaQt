// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"

#include <QObject>
#include <QString>

namespace QindaQt::Shell {

// AGENT-CONTRACT (ADR-0286): everything besides the everywhere wallpaper that
// decides which picture one output paints — the confirmed per-display and
// per-desktop choices, the ADR-0017 stable id behind each output's connector
// name (QScreen::name()), and the current virtual desktop id. The wallpaper
// controller borrows one source and re-resolves every output on changed().
//
// Every answer may be unknown (empty or no choices): before Display1 or the
// compositor has answered, for ambiguous twin displays, or after a service is
// lost. The controller then falls back through the shared precedence in
// WallpaperAssignments::resolve() and never waits for a source to answer.
//
// Threading: GUI thread only. Production is SessionWallpaperSelection; tests
// inject a fake.
class WallpaperSelectionSource : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~WallpaperSelectionSource() override = default;

    // Borrowed; valid until the next changed() or the source's destruction.
    [[nodiscard]] virtual const Services::WallpaperAssignments::WallpaperAssignments &
    assignments() const = 0;
    [[nodiscard]] virtual QString displayIdForConnector(const QString &connectorName) const = 0;
    [[nodiscard]] virtual QString currentDesktopId() const = 0;

Q_SIGNALS:
    void changed();
};

} // namespace QindaQt::Shell
