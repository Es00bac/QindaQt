// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_types.h>

#include <QString>
#include <QVariantList>

namespace QindaQt::Shell::ObsApplet {

// What the top bar shows for OBS, as a pure projection of one snapshot.
//
// AGENT-CONTRACT: the glyph and the summary are derived here and nowhere
// else, so the applet, its accessibility text and any row that asserts them
// cannot drift apart. Nothing in this file talks to OBS.
struct AppletModel {
    // Icon name for the top-bar button.
    QString iconName;
    // One short line for the button's tooltip and accessible name.
    QString summaryLabel;
    QString accessibleName;
    QString accessibleDescription;
    // True when the popup's controls can actually do something.
    bool controlAvailable = false;
    // A sentence for the popup when OBS cannot be driven, or empty.
    QString unavailableText;
    bool recording = false;
    bool streaming = false;
    bool virtualCamera = false;
    QString recordingElapsed;
    QString streamingElapsed;
    QString droppedFramesWarning;
    QStringList sceneNames;
    QString currentScene;

    friend bool operator==(const AppletModel &, const AppletModel &) = default;
};

// The one place the snapshot becomes a top bar.
[[nodiscard]] AppletModel projectApplet(const Obs::ObsSnapshot &snapshot);

// Elapsed time as the applet shows it: a dash until OBS reports one, because
// a zero clock reads as "it just started".
[[nodiscard]] QString formatElapsed(qint64 milliseconds);

} // namespace QindaQt::Shell::ObsApplet
