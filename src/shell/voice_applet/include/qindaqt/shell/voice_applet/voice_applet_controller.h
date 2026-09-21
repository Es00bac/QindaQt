// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/voice_applet/voice_applet_presentation.h>
#include <qindaqt/shell/voice_applet/voice_client_interface.h>
#include <qindaqt/shell/voice_applet/voice_request_state.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <functional>

namespace QindaQt::Shell::VoiceApplet {

// Shell-private adapter from the injected Voice1 seam to bounded QML values.
//
// AGENT-CONTRACT: the seam is borrowed from shell composition, shares this
// object's thread, and must outlive this object. Nothing but the projected
// value model crosses into QML; in particular no provider bus name, no PID and
// no unbounded transcript ever reaches a QML file.
//
// AGENT-GUARD: every control path runs through beginVoiceRequest() before it
// reaches the seam, even though VoiceClient admits again. The duplicate
// admission is what lets the projection promise that an enabled control is a
// dispatchable one, and it is why QML may pass an action id it was given
// without the controller trusting it.
class VoiceAppletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString stateId READ stateId NOTIFY stateChanged)
    Q_PROPERTY(QString iconName READ iconName NOTIFY stateChanged)
    Q_PROPERTY(QString summaryLabel READ summaryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString statusLabel READ statusLabel NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY stateChanged)
    Q_PROPERTY(QString languageLabel READ languageLabel NOTIFY stateChanged)
    Q_PROPERTY(QString microphoneLabel READ microphoneLabel NOTIFY stateChanged)
    Q_PROPERTY(QString dictationShortcut READ dictationShortcut NOTIFY stateChanged)
    Q_PROPERTY(QString commandShortcut READ commandShortcut NOTIFY stateChanged)
    Q_PROPERTY(QString routeLabel READ routeLabel NOTIFY stateChanged)
    Q_PROPERTY(QString transcriptText READ transcriptText NOTIFY stateChanged)
    Q_PROPERTY(bool transcriptIsPartial READ transcriptIsPartial NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
    Q_PROPERTY(int levelPercent READ levelPercent NOTIFY levelChanged)
    Q_PROPERTY(bool voiceEnabled READ voiceEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool capturing READ capturing NOTIFY stateChanged)
    Q_PROPERTY(bool commandMode READ commandMode NOTIFY stateChanged)
    Q_PROPERTY(bool controlAvailable READ controlAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
    Q_PROPERTY(quint64 serviceRevision READ serviceRevision NOTIFY stateChanged)
    Q_PROPERTY(QVariantList actionRows READ actionRows NOTIFY stateChanged)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(bool canOpenSettings READ canOpenSettings CONSTANT)
    Q_PROPERTY(bool canOpenConsole READ canOpenConsole CONSTANT)

public:
    // Route launchers the shell composition supplies. Either may be empty; the
    // matching affordance then reports itself unavailable instead of rendering
    // a button that does nothing. The controller never spawns a process itself.
    using Launch = std::function<bool()>;

    VoiceAppletController(VoiceClientInterface *client, bool readGranted,
                          bool controlGranted, QObject *parent = nullptr);
    ~VoiceAppletController() override;

    void setSettingsLaunch(Launch launch);
    void setConsoleLaunch(Launch launch);

    // The user's services.voicePanelTranscript preference, published by shell
    // composition from the Settings1 client. Defaults to shown, matching the
    // schema, so a settings service that never answers does not silently
    // suppress a feature the user did not turn off.
    void setTranscriptVisible(bool visible);

    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString stateId() const { return m_model.stateId; }
    [[nodiscard]] QString iconName() const { return m_model.iconName; }
    [[nodiscard]] QString summaryLabel() const { return m_model.summaryLabel; }
    [[nodiscard]] QString statusLabel() const { return m_model.statusLabel; }
    [[nodiscard]] QString diagnostic() const { return m_model.diagnostic; }
    [[nodiscard]] QString providerLabel() const { return m_model.providerLabel; }
    [[nodiscard]] QString languageLabel() const { return m_model.languageLabel; }
    [[nodiscard]] QString microphoneLabel() const { return m_model.microphoneLabel; }
    [[nodiscard]] QString dictationShortcut() const { return m_model.dictationShortcut; }
    [[nodiscard]] QString commandShortcut() const { return m_model.commandShortcut; }
    [[nodiscard]] QString routeLabel() const { return m_model.routeLabel; }
    [[nodiscard]] QString transcriptText() const { return m_model.transcriptText; }
    [[nodiscard]] bool transcriptIsPartial() const noexcept
    {
        return m_model.transcriptIsPartial;
    }
    [[nodiscard]] QString accessibleName() const { return m_model.accessibleName; }
    [[nodiscard]] QString accessibleDescription() const
    {
        return m_model.accessibleDescription;
    }
    [[nodiscard]] int levelPercent() const noexcept
    {
        return static_cast<int>(m_model.levelPercent);
    }
    [[nodiscard]] bool voiceEnabled() const noexcept { return m_model.enabled; }
    [[nodiscard]] bool capturing() const noexcept { return m_model.capturing; }
    [[nodiscard]] bool commandMode() const noexcept { return m_model.commandMode; }
    [[nodiscard]] bool controlAvailable() const noexcept
    {
        return m_model.controlGranted && m_model.phase == ServicePhase::Ready;
    }
    [[nodiscard]] bool operationPending() const noexcept { return m_request.pending(); }
    [[nodiscard]] quint64 serviceRevision() const noexcept { return m_model.revision; }
    [[nodiscard]] QVariantList actionRows() const;
    [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
    [[nodiscard]] QString feedback() const { return m_feedback; }
    [[nodiscard]] bool canOpenSettings() const noexcept
    {
        return static_cast<bool>(m_settingsLaunch);
    }
    [[nodiscard]] bool canOpenConsole() const noexcept
    {
        return static_cast<bool>(m_consoleLaunch);
    }

    // Opening the popup asks for fresh truth. Closing it changes nothing: the
    // chip keeps reporting capture state whether or not the popup is open.
    Q_INVOKABLE void setExpanded(bool expanded);

    // Dispatch by the id the projection published. An id this controller did
    // not offer is refused without reaching the seam.
    Q_INVOKABLE bool invokeAction(const QString &actionId);
    Q_INVOKABLE bool setVoiceEnabled(bool enabled);
    Q_INVOKABLE bool selectProvider(const QString &providerId);
    Q_INVOKABLE void dismissFeedback();

    // Open the Voice page of Settings, and the standalone Voice console.
    // Both report failure through feedback rather than throwing it away.
    Q_INVOKABLE bool openSettings();
    Q_INVOKABLE bool openConsole();

Q_SIGNALS:
    void stateChanged();
    void levelChanged();
    void feedbackChanged();

private:
    void refreshModel();
    void publishFeedback(const QString &feedback);
    [[nodiscard]] bool dispatch(Services::Voice::OperationKind kind,
                                const QString &providerId, bool enable);
    void onOperationCompleted(quint64 requestId,
                              const Services::Voice::OperationResult &result);

    VoiceClientInterface *m_client = nullptr;
    bool m_readGranted = false;
    bool m_controlGranted = false;
    bool m_expanded = false;
    VoiceAppletModel m_model;
    RequestState m_request;
    QString m_feedback;
    Launch m_settingsLaunch;
    Launch m_consoleLaunch;
    bool m_transcriptVisible = true;
};

} // namespace QindaQt::Shell::VoiceApplet
