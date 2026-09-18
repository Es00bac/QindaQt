// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/obs_applet/obs_applet_presentation.h>

#include <QCoreApplication>

namespace QindaQt::Shell::ObsApplet {

using namespace QindaQt::Obs;

namespace {

constexpr double DroppedFrameWarningFraction = 0.01;

QString tr(const char *text) {
    return QCoreApplication::translate("ObsApplet", text);
}

} // namespace

QString formatElapsed(qint64 milliseconds) {
    if (milliseconds < 0) {
        return QStringLiteral("—");
    }
    const qint64 totalSeconds = milliseconds / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

AppletModel projectApplet(const ObsSnapshot &snapshot) {
    AppletModel model;
    model.recording = snapshot.record.active;
    model.streaming = snapshot.stream.active;
    model.virtualCamera = snapshot.virtualCam.active;
    model.recordingElapsed = model.recording
                                 ? formatElapsed(snapshot.record.durationMs)
                                 : QStringLiteral("—");
    model.streamingElapsed = model.streaming
                                 ? formatElapsed(snapshot.stream.durationMs)
                                 : QStringLiteral("—");
    model.sceneNames = snapshot.scenes.names;
    model.currentScene = snapshot.scenes.currentProgramScene;
    model.controlAvailable = snapshot.ready();

    if (!snapshot.ready()) {
        // AGENT-CONTRACT: "OBS is not running" and "OBS refused us" are
        // different sentences, because the user's next action is different.
        model.iconName = QStringLiteral("camera-video-symbolic");
        if (snapshot.reasonCode == QLatin1String(ReasonCodes::AuthRequired) ||
            snapshot.reasonCode == QLatin1String(ReasonCodes::AuthRejected)) {
            model.unavailableText =
                tr("OBS did not accept QindaQt's password. Open Settings → "
                   "Streaming and set OBS up again.");
        } else if (snapshot.reasonCode ==
                   QLatin1String(ReasonCodes::RpcVersion)) {
            model.unavailableText =
                tr("This version of OBS speaks a control protocol QindaQt "
                   "does not.");
        } else if (snapshot.reasonCode == QLatin1String(ReasonCodes::Malformed)) {
            model.unavailableText =
                tr("OBS sent something QindaQt could not read. The connection "
                   "was closed.");
        } else {
            model.unavailableText = tr("OBS is not running.");
        }
        model.summaryLabel = tr("OBS");
        model.accessibleName = tr("OBS");
        model.accessibleDescription = model.unavailableText;
        return model;
    }

    // AGENT-GUARD: the glyph states the most consequential thing OBS is
    // doing. Streaming outranks recording, which outranks the camera: a user
    // who is live needs to see that first.
    if (model.streaming) {
        model.iconName = QStringLiteral("media-record-symbolic");
        model.summaryLabel = tr("Streaming");
    } else if (model.recording) {
        model.iconName = QStringLiteral("media-record-symbolic");
        model.summaryLabel = tr("Recording");
    } else if (model.virtualCamera) {
        model.iconName = QStringLiteral("camera-web-symbolic");
        model.summaryLabel = tr("Virtual camera");
    } else {
        model.iconName = QStringLiteral("camera-video-symbolic");
        model.summaryLabel = tr("OBS");
    }

    const OutputStatus &stream = snapshot.stream;
    if (stream.active && stream.hasFrameCounts() &&
        stream.droppedFraction() >= DroppedFrameWarningFraction) {
        model.droppedFramesWarning =
            QCoreApplication::translate("ObsApplet",
                                        "%1% of frames dropped. The "
                                        "connection is not keeping up.")
                .arg(stream.droppedFraction() * 100.0, 0, 'f', 1);
    }

    QStringList active;
    if (model.streaming) {
        active.append(tr("streaming"));
    }
    if (model.recording) {
        active.append(tr("recording"));
    }
    if (model.virtualCamera) {
        active.append(tr("virtual camera"));
    }
    model.accessibleName = tr("OBS");
    model.accessibleDescription =
        active.isEmpty()
            ? tr("Connected to OBS and idle.")
            : QCoreApplication::translate("ObsApplet", "OBS is %1.")
                  .arg(active.join(QCoreApplication::translate("ObsApplet",
                                                               ", ")));
    return model;
}

} // namespace QindaQt::Shell::ObsApplet
