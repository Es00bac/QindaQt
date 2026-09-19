// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/obs_client/obs_client.h>
#include <qindaqt/shell/obs_applet/obs_applet_presentation.h>

#include <QObject>
#include <QHash>
#include <QVariantList>

namespace QindaQt::Shell::ObsApplet {

// Shell-private adapter from the obs-websocket client to bounded QML values.
//
// AGENT-CONTRACT: the client is borrowed from shell composition, shares this
// object's thread and must outlive this object. Only the projection crosses
// into QML; the password never enters this class at all — the shell supplies
// it to the client.
//
// AGENT-GUARD: every control path checks `controlAvailable` first. An
// enabled control in this popup is a dispatchable one, so a user never
// presses a button that silently does nothing.
class ObsAppletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString iconName READ iconName NOTIFY stateChanged)
    Q_PROPERTY(QString summaryLabel READ summaryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY
                   stateChanged)
    Q_PROPERTY(bool controlAvailable READ controlAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool recordingAvailable READ recordingAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool streamingAvailable READ streamingAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool virtualCameraAvailable READ virtualCameraAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool sceneAvailable READ sceneAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString pendingText READ pendingText NOTIFY stateChanged)
    Q_PROPERTY(QString unavailableText READ unavailableText NOTIFY stateChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY stateChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)
    Q_PROPERTY(bool virtualCamera READ virtualCamera NOTIFY stateChanged)
    Q_PROPERTY(QString recordingElapsed READ recordingElapsed NOTIFY stateChanged)
    Q_PROPERTY(QString streamingElapsed READ streamingElapsed NOTIFY stateChanged)
    Q_PROPERTY(QString droppedFramesWarning READ droppedFramesWarning NOTIFY
                   stateChanged)
    Q_PROPERTY(QStringList sceneNames READ sceneNames NOTIFY stateChanged)
    Q_PROPERTY(QString currentScene READ currentScene NOTIFY stateChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
    explicit ObsAppletController(Obs::ObsClient *client,
                                 QObject *parent = nullptr,
                                 bool controlGranted = true);
    ~ObsAppletController() override;

    [[nodiscard]] QString iconName() const { return m_model.iconName; }
    [[nodiscard]] QString summaryLabel() const { return m_model.summaryLabel; }
    [[nodiscard]] QString accessibleName() const {
        return m_model.accessibleName;
    }
    [[nodiscard]] QString accessibleDescription() const {
        return m_model.accessibleDescription;
    }
    [[nodiscard]] bool controlAvailable() const {
        return m_model.controlAvailable;
    }
    [[nodiscard]] bool recordingAvailable() const;
    [[nodiscard]] bool streamingAvailable() const;
    [[nodiscard]] bool virtualCameraAvailable() const;
    [[nodiscard]] bool sceneAvailable() const;
    [[nodiscard]] QString pendingText() const;
    [[nodiscard]] QString unavailableText() const {
        return m_model.unavailableText;
    }
    [[nodiscard]] bool recording() const { return m_model.recording; }
    [[nodiscard]] bool streaming() const { return m_model.streaming; }
    [[nodiscard]] bool virtualCamera() const { return m_model.virtualCamera; }
    [[nodiscard]] QString recordingElapsed() const {
        return m_model.recordingElapsed;
    }
    [[nodiscard]] QString streamingElapsed() const {
        return m_model.streamingElapsed;
    }
    [[nodiscard]] QString droppedFramesWarning() const {
        return m_model.droppedFramesWarning;
    }
    [[nodiscard]] QStringList sceneNames() const { return m_model.sceneNames; }
    [[nodiscard]] QString currentScene() const { return m_model.currentScene; }
    [[nodiscard]] QString feedback() const { return m_feedback; }

    // The three output actions and the scene switch. Each is a no-op with feedback
    // when the client is not ready, never a silent one.
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void toggleStreaming();
    Q_INVOKABLE void toggleVirtualCamera();
    Q_INVOKABLE void selectScene(const QString &sceneName);
    // Opens OBS itself, for everything this popup deliberately does not do.
    Q_INVOKABLE bool openObs();
    Q_INVOKABLE bool openStreamingSettings();

Q_SIGNALS:
    void stateChanged();
    void feedbackChanged();

private:
    void republish();
    void setFeedback(const QString &text);
    [[nodiscard]] bool dispatchable(const QString &action);
    [[nodiscard]] bool actionAvailable(const QString &action) const;
    void trackRequest(quint64 requestId, const QString &action);
    void clearObservedRequests(const QString &action);

    Obs::ObsClient *m_client = nullptr;
    AppletModel m_model;
    QString m_feedback;
    bool m_controlGranted = true;
    QHash<quint64, QString> m_requests;
};

} // namespace QindaQt::Shell::ObsApplet
