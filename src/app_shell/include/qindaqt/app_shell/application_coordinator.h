// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/app_shell/action_registry.h"

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace QindaQt::AppShell {

// The owning application creates one coordinator on its GUI thread and keeps
// it alive for the primary window lifetime. It injects confirmed integration
// state and resolves emitted quit/portal requests. The coordinator never owns
// QCoreApplication lifetime, persistence, D-Bus, or a platform portal.
// Qt's generated QML element wrapper derives from creatable C++ types. Keep
// this public coordinator non-final while QML_ELEMENT remains part of 1.0.
class ApplicationCoordinator : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString applicationName READ applicationName WRITE setApplicationName
                   NOTIFY applicationNameChanged FINAL)
    Q_PROPERTY(QString windowTitle READ windowTitle WRITE setWindowTitle
                   NOTIFY windowTitleChanged FINAL)
    Q_PROPERTY(QString initialFocusObjectName READ initialFocusObjectName
                   WRITE setInitialFocusObjectName NOTIFY initialFocusObjectNameChanged FINAL)
    Q_PROPERTY(QString focusOwnerObjectName READ focusOwnerObjectName
                   NOTIFY focusOwnerObjectNameChanged FINAL)
    Q_PROPERTY(QVariantList menus READ menus NOTIFY menusChanged FINAL)
    Q_PROPERTY(QindaQt::AppShell::IntegrationState settingsState READ settingsState
                   NOTIFY integrationStateChanged FINAL)
    Q_PROPERTY(QindaQt::AppShell::IntegrationState sessionState READ sessionState
                   NOTIFY integrationStateChanged FINAL)
    Q_PROPERTY(bool degraded READ degraded NOTIFY integrationStateChanged FINAL)
    Q_PROPERTY(QString degradedMessage READ degradedMessage
                   NOTIFY integrationStateChanged FINAL)
    Q_PROPERTY(bool quitPending READ quitPending NOTIFY quitPendingChanged FINAL)
    Q_PROPERTY(QindaQt::AppShell::ErrorCode lastErrorCode READ lastErrorCode
                   NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorChanged FINAL)
    Q_PROPERTY(bool lastErrorRecoverable READ lastErrorRecoverable
                   NOTIFY lastErrorChanged FINAL)

public:
    using ErrorCode = QindaQt::AppShell::ErrorCode;
    using IntegrationState = QindaQt::AppShell::IntegrationState;

    explicit ApplicationCoordinator(QObject *parent = nullptr);

    [[nodiscard]] QString applicationName() const;
    void setApplicationName(const QString &name);
    [[nodiscard]] QString windowTitle() const;
    void setWindowTitle(const QString &title);
    [[nodiscard]] QString initialFocusObjectName() const;
    void setInitialFocusObjectName(const QString &objectName);
    [[nodiscard]] QString focusOwnerObjectName() const;

    [[nodiscard]] QVariantList menus() const;
    [[nodiscard]] const ActionRegistry &actionRegistry() const;
    [[nodiscard]] Error replaceActions(const QList<ActionSpec> &actions);
    [[nodiscard]] Error setActionEnabled(const QString &actionId, bool enabled);
    [[nodiscard]] Error setActionChecked(const QString &actionId, bool checked);

    [[nodiscard]] IntegrationState settingsState() const;
    [[nodiscard]] IntegrationState sessionState() const;
    void setSettingsState(IntegrationState state, const QString &detail = {});
    void setSessionState(IntegrationState state, const QString &detail = {});
    [[nodiscard]] bool degraded() const;
    [[nodiscard]] QString degradedMessage() const;

    [[nodiscard]] bool quitPending() const;
    Q_INVOKABLE quint64 requestQuit(const QString &reason = QStringLiteral("window-close"));
    [[nodiscard]] Error resolveQuit(quint64 requestId,
                                    bool approved,
                                    const QString &reason = {});

    Q_INVOKABLE quint64 requestOpenFile(const QString &title,
                                        const QStringList &mimeTypes = {});
    Q_INVOKABLE quint64 requestSaveFile(const QString &title,
                                        const QString &suggestedName,
                                        const QStringList &mimeTypes = {});
    Q_INVOKABLE quint64 requestFolder(const QString &title);
    [[nodiscard]] Error resolvePortal(quint64 requestId,
                                      bool accepted,
                                      const QList<QUrl> &urls = {},
                                      const Error &error = {});

    Q_INVOKABLE bool activateAction(const QString &actionId);
    Q_INVOKABLE void reportFocusOwner(const QString &objectName);
    Q_INVOKABLE void clearError();

    [[nodiscard]] ErrorCode lastErrorCode() const;
    [[nodiscard]] QString lastErrorMessage() const;
    [[nodiscard]] bool lastErrorRecoverable() const;

signals:
    void applicationNameChanged();
    void windowTitleChanged();
    void initialFocusObjectNameChanged();
    void focusOwnerObjectNameChanged();
    void menusChanged();
    void integrationStateChanged();
    void quitPendingChanged();
    void lastErrorChanged();

    void actionRequested(const QString &actionId);
    void quitDecisionRequested(quint64 requestId, const QString &reason);
    void quitApproved(quint64 requestId);
    void quitRejected(quint64 requestId, const QString &reason);
    void portalRequestIssued(const QindaQt::AppShell::PortalRequest &request);
    void portalFinished(const QindaQt::AppShell::PortalResult &result);

private:
    [[nodiscard]] quint64 issuePortalRequest(PortalKind kind,
                                             const QString &title,
                                             const QString &suggestedName,
                                             const QStringList &mimeTypes);
    [[nodiscard]] Error validatePortalRequest(const PortalRequest &request) const;
    [[nodiscard]] Error verifyThread() const;
    void setLastError(const Error &error);
    void setMetadata(QString &storage,
                     const QString &value,
                     qsizetype maximumLength,
                     void (ApplicationCoordinator::*changedSignal)());

    ActionRegistry m_actions;
    QString m_applicationName;
    QString m_windowTitle;
    QString m_initialFocusObjectName;
    QString m_focusOwnerObjectName;
    IntegrationState m_settingsState = IntegrationState::NotRequired;
    IntegrationState m_sessionState = IntegrationState::NotRequired;
    QString m_settingsDetail;
    QString m_sessionDetail;
    quint64 m_nextRequestId = 1;
    quint64 m_pendingQuitId = 0;
    PortalRequest m_pendingPortal;
    Error m_lastError;
};

} // namespace QindaQt::AppShell
