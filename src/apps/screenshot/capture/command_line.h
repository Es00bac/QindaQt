// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "capture_request.h"

#include <QString>
#include <QStringList>

namespace QindaQt::Screenshot {

// What one process launch should do.
//
// AGENT-CONTRACT: desktop-controls launches `--region`, `--fullscreen`,
// `--active` and `--record-toggle` for the global shortcuts (ADR-0289); the
// same flags are the documented scripting surface. A capture flag alone
// shows the result window; adding --copy and/or --save makes the launch
// windowless (except for the region overlay the user must draw on).
struct Invocation {
    enum class Action { Window, Capture, RecordToggle };

    Action action = Action::Window;
    CaptureOptions capture;
    bool copy = false;
    bool save = false;
    // Empty means the default directory and name. A directory means the
    // default name inside it.
    QString savePath;
    // Non-empty when parsing failed; the launch prints it and exits 2.
    QString error;
    // --help / --version were answered by the parser itself.
    bool handled = false;
    QString handledText;

    [[nodiscard]] bool windowless() const
    {
        return action == Action::RecordToggle || (action == Action::Capture && (copy || save));
    }
};

// Parses argv (including argv[0]). Pure: never exits and never prints.
[[nodiscard]] Invocation parseInvocation(const QStringList &arguments);

} // namespace QindaQt::Screenshot
