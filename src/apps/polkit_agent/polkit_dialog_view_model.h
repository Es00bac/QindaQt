// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "polkit_attempt_controller.h"
#include "polkit_request_queue.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <vector>

namespace QindaQt::Apps::PolkitAgent {

// The one seam between the pure dialog/session logic above and the QML
// dialog (PolkitDialogContent.qml). Still polkit-type-free: it only ever
// sees AuthenticationRequest and PolkitAttemptController, both pure.
// Re-targets its connections to a new controller every time the queue
// activates one, so the dialog content updates in place rather than being
// torn down and rebuilt per request.
class PolkitDialogViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString message READ message NOTIFY requestChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY requestChanged)
    Q_PROPERTY(QString appIconName READ appIconName NOTIFY requestChanged)
    Q_PROPERTY(QString actionId READ actionId NOTIFY requestChanged)
    Q_PROPERTY(QString vendorName READ vendorName NOTIFY requestChanged)
    Q_PROPERTY(QString programPath READ programPath NOTIFY requestChanged)
    Q_PROPERTY(QStringList identityLabels READ identityLabels NOTIFY requestChanged)
    Q_PROPERTY(bool identityChoiceVisible READ identityChoiceVisible NOTIFY requestChanged)
    Q_PROPERTY(int selectedIdentityIndex READ selectedIdentityIndex WRITE selectIdentity
                  NOTIFY selectedIdentityIndexChanged)
    Q_PROPERTY(QString promptText READ promptText NOTIFY promptChanged)
    Q_PROPERTY(bool promptEchoAllowed READ promptEchoAllowed NOTIFY promptChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit PolkitDialogViewModel(QObject *parent = nullptr);

    // Wires this view model to a queue for production use: every
    // requestActivated attaches, queueIdle detaches. The offscreen QML test
    // calls attachRequest()/detach() directly instead, with a hand-built
    // request and controller and no queue at all.
    void observe(PolkitRequestQueue &queue);
    void attachRequest(const AuthenticationRequest &request, PolkitAttemptController *controller);
    void detach();

    [[nodiscard]] bool isVisible() const noexcept { return m_controller != nullptr; }
    [[nodiscard]] QString message() const { return m_request.message; }
    [[nodiscard]] QString appName() const;
    [[nodiscard]] QString appIconName() const { return m_request.requester.iconName; }
    [[nodiscard]] QString actionId() const { return m_request.actionId; }
    [[nodiscard]] QString vendorName() const { return m_request.vendorName; }
    [[nodiscard]] QString programPath() const { return m_request.requester.programPath; }
    [[nodiscard]] QStringList identityLabels() const;
    [[nodiscard]] bool identityChoiceVisible() const { return m_request.identities.size() > 1; }
    [[nodiscard]] int selectedIdentityIndex() const;
    [[nodiscard]] QString promptText() const { return m_promptText; }
    [[nodiscard]] bool promptEchoAllowed() const { return m_promptEcho; }
    [[nodiscard]] QString statusText() const
    {
        return m_errorText.isEmpty() ? m_infoText : m_errorText;
    }
    [[nodiscard]] bool statusIsError() const { return !m_errorText.isEmpty(); }
    [[nodiscard]] bool busy() const noexcept { return m_busy; }

    Q_INVOKABLE void authenticate(const QString &response);
    Q_INVOKABLE void cancel();
    void selectIdentity(int index);

Q_SIGNALS:
    void visibleChanged();
    void requestChanged();
    void selectedIdentityIndexChanged();
    void promptChanged();
    void statusChanged();
    void busyChanged();
    // Forwarded distinctly from statusChanged so the dialog can clear and
    // refocus the response field on exactly this transition, never on a
    // showInfo/showError that is not a failed attempt.
    void attemptFailed();
    void finished(bool gainedAuthorization);

private:
    void resetTransientState();

    AuthenticationRequest m_request;
    PolkitAttemptController *m_controller = nullptr;
    QString m_promptText;
    bool m_promptEcho = false;
    QString m_infoText;
    QString m_errorText;
    bool m_busy = false;
    std::vector<QMetaObject::Connection> m_connections;
};

} // namespace QindaQt::Apps::PolkitAgent
