// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/app_shell/application_coordinator.h"

#include <QRegularExpression>
#include <QThread>

namespace QindaQt::AppShell {
namespace {

const QRegularExpression &objectNamePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]{0,63}$"));
    return pattern;
}

const QRegularExpression &mimeTypePattern()
{
    static const QRegularExpression pattern(QStringLiteral(
        "^[A-Za-z0-9][A-Za-z0-9!#$&^_.+-]{0,126}/[A-Za-z0-9*][A-Za-z0-9!#$&^_.+*-]{0,126}$"));
    return pattern;
}

QString stateMessage(const QString &label, IntegrationState state, const QString &detail)
{
    if (state == IntegrationState::NotRequired || state == IntegrationState::Ready) {
        return {};
    }
    const QString condition = state == IntegrationState::Unavailable
        ? QStringLiteral("unavailable")
        : QStringLiteral("degraded");
    const QString prefix = QStringLiteral("%1 integration is %2.").arg(label, condition);
    return detail.trimmed().isEmpty() ? prefix : prefix + QLatin1Char(' ') + detail.trimmed();
}

} // namespace

ApplicationCoordinator::ApplicationCoordinator(QObject *parent)
    : QObject(parent)
    , m_actions(this)
{
    qRegisterMetaType<PortalRequest>();
    qRegisterMetaType<PortalResult>();
    connect(&m_actions, &ActionRegistry::menusChanged, this, &ApplicationCoordinator::menusChanged);
    connect(&m_actions,
            &ActionRegistry::activationRequested,
            this,
            &ApplicationCoordinator::actionRequested);
}

QString ApplicationCoordinator::applicationName() const { return m_applicationName; }

void ApplicationCoordinator::setApplicationName(const QString &name)
{
    setMetadata(m_applicationName,
                name,
                MaximumLabelLength,
                &ApplicationCoordinator::applicationNameChanged);
}

QString ApplicationCoordinator::windowTitle() const { return m_windowTitle; }

void ApplicationCoordinator::setWindowTitle(const QString &title)
{
    setMetadata(m_windowTitle,
                title,
                MaximumLabelLength,
                &ApplicationCoordinator::windowTitleChanged);
}

QString ApplicationCoordinator::initialFocusObjectName() const
{
    return m_initialFocusObjectName;
}

void ApplicationCoordinator::setInitialFocusObjectName(const QString &objectName)
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    if (!objectName.isEmpty() && !objectNamePattern().match(objectName).hasMatch()) {
        setLastError(makeError(ErrorCode::InvalidArgument,
                               QStringLiteral("Initial focus object name is invalid")));
        return;
    }
    setMetadata(m_initialFocusObjectName,
                objectName,
                MaximumIdentifierLength,
                &ApplicationCoordinator::initialFocusObjectNameChanged);
}

QString ApplicationCoordinator::focusOwnerObjectName() const
{
    return m_focusOwnerObjectName;
}

QVariantList ApplicationCoordinator::menus() const { return m_actions.menus(); }

const ActionRegistry &ApplicationCoordinator::actionRegistry() const { return m_actions; }

Error ApplicationCoordinator::replaceActions(const QList<ActionSpec> &actions)
{
    const Error error = m_actions.replaceActions(actions);
    setLastError(error);
    return error;
}

Error ApplicationCoordinator::setActionEnabled(const QString &actionId, bool enabled)
{
    const Error error = m_actions.setEnabled(actionId, enabled);
    setLastError(error);
    return error;
}

Error ApplicationCoordinator::setActionChecked(const QString &actionId, bool checked)
{
    const Error error = m_actions.setChecked(actionId, checked);
    setLastError(error);
    return error;
}

IntegrationState ApplicationCoordinator::settingsState() const { return m_settingsState; }
IntegrationState ApplicationCoordinator::sessionState() const { return m_sessionState; }

void ApplicationCoordinator::setSettingsState(IntegrationState state, const QString &detail)
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    if (m_settingsState == state && m_settingsDetail == detail.left(MaximumDiagnosticLength)) {
        return;
    }
    m_settingsState = state;
    m_settingsDetail = detail.left(MaximumDiagnosticLength);
    emit integrationStateChanged();
}

void ApplicationCoordinator::setSessionState(IntegrationState state, const QString &detail)
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    if (m_sessionState == state && m_sessionDetail == detail.left(MaximumDiagnosticLength)) {
        return;
    }
    m_sessionState = state;
    m_sessionDetail = detail.left(MaximumDiagnosticLength);
    emit integrationStateChanged();
}

bool ApplicationCoordinator::degraded() const { return !degradedMessage().isEmpty(); }

QString ApplicationCoordinator::degradedMessage() const
{
    QStringList messages;
    const QString settings = stateMessage(QStringLiteral("Settings"),
                                          m_settingsState,
                                          m_settingsDetail);
    const QString session = stateMessage(QStringLiteral("Session"),
                                         m_sessionState,
                                         m_sessionDetail);
    if (!settings.isEmpty()) {
        messages.append(settings);
    }
    if (!session.isEmpty()) {
        messages.append(session);
    }
    return messages.join(QLatin1Char('\n'));
}

bool ApplicationCoordinator::quitPending() const { return m_pendingQuitId != 0; }

quint64 ApplicationCoordinator::requestQuit(const QString &reason)
{
    if (const Error error = verifyThread(); !error.ok()) {
        setLastError(error);
        return 0;
    }
    if (m_pendingQuitId != 0) {
        setLastError(makeError(ErrorCode::Busy,
                               QStringLiteral("A quit decision is already pending"),
                               true));
        return 0;
    }
    if (reason.trimmed().isEmpty() || reason != reason.trimmed()
        || reason.size() > MaximumLabelLength) {
        setLastError(makeError(ErrorCode::InvalidArgument,
                               QStringLiteral("Quit reason is invalid")));
        return 0;
    }
    m_pendingQuitId = m_nextRequestId++;
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    setLastError(Error::success());
    emit quitPendingChanged();
    emit quitDecisionRequested(m_pendingQuitId, reason);
    return m_pendingQuitId;
}

Error ApplicationCoordinator::resolveQuit(quint64 requestId,
                                           bool approved,
                                           const QString &reason)
{
    if (const Error error = verifyThread(); !error.ok()) {
        setLastError(error);
        return error;
    }
    if (requestId == 0 || requestId != m_pendingQuitId) {
        const Error error = makeError(ErrorCode::StaleRequest,
                                      QStringLiteral("Quit decision does not match the pending request"));
        setLastError(error);
        return error;
    }
    const quint64 resolvedId = m_pendingQuitId;
    m_pendingQuitId = 0;
    emit quitPendingChanged();
    setLastError(Error::success());
    if (approved) {
        emit quitApproved(resolvedId);
    } else {
        emit quitRejected(resolvedId, reason.left(MaximumDiagnosticLength));
    }
    return Error::success();
}

quint64 ApplicationCoordinator::requestOpenFile(const QString &title,
                                                 const QStringList &mimeTypes)
{
    return issuePortalRequest(PortalKind::OpenFile, title, {}, mimeTypes);
}

quint64 ApplicationCoordinator::requestSaveFile(const QString &title,
                                                 const QString &suggestedName,
                                                 const QStringList &mimeTypes)
{
    return issuePortalRequest(PortalKind::SaveFile, title, suggestedName, mimeTypes);
}

quint64 ApplicationCoordinator::requestFolder(const QString &title)
{
    return issuePortalRequest(PortalKind::SelectFolder, title, {}, {});
}

Error ApplicationCoordinator::resolvePortal(quint64 requestId,
                                             bool accepted,
                                             const QList<QUrl> &urls,
                                             const Error &error)
{
    if (const Error threadError = verifyThread(); !threadError.ok()) {
        setLastError(threadError);
        return threadError;
    }
    if (requestId == 0 || requestId != m_pendingPortal.id) {
        const Error stale = makeError(
            ErrorCode::StaleRequest,
            QStringLiteral("Portal result does not match the pending request"));
        setLastError(stale);
        return stale;
    }
    if (urls.size() > MaximumPortalUrlCount || (!accepted && !urls.isEmpty())
        || (accepted && (urls.isEmpty() || !error.ok()))
        || (accepted && m_pendingPortal.kind != PortalKind::OpenFile && urls.size() != 1)
        || (!error.ok() && error.message.size() > MaximumDiagnosticLength)) {
        const Error invalid = makeError(ErrorCode::InvalidArgument,
                                        QStringLiteral("Portal result is internally inconsistent"));
        setLastError(invalid);
        return invalid;
    }
    for (const QUrl &url : urls) {
        if (!url.isValid() || url.isRelative()) {
            const Error invalid = makeError(ErrorCode::InvalidArgument,
                                            QStringLiteral("Portal returned an invalid URL"));
            setLastError(invalid);
            return invalid;
        }
    }

    PortalResult result;
    result.requestId = requestId;
    result.kind = m_pendingPortal.kind;
    result.accepted = accepted;
    result.urls = urls;
    result.error = accepted
        ? Error::success()
        : (error.ok() ? makeError(ErrorCode::Cancelled,
                                  QStringLiteral("Portal request was cancelled"),
                                  true)
                      : makeError(error.code, error.message, error.recoverable));
    m_pendingPortal = {};
    setLastError(result.error.code == ErrorCode::Cancelled ? Error::success() : result.error);
    emit portalFinished(result);
    return Error::success();
}

bool ApplicationCoordinator::activateAction(const QString &actionId)
{
    const Error error = m_actions.requestActivation(actionId);
    setLastError(error);
    return error.ok();
}

void ApplicationCoordinator::reportFocusOwner(const QString &objectName)
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    if (!objectName.isEmpty() && !objectNamePattern().match(objectName).hasMatch()) {
        setLastError(makeError(ErrorCode::InvalidArgument,
                               QStringLiteral("Focus owner object name is invalid")));
        return;
    }
    if (m_focusOwnerObjectName == objectName) {
        return;
    }
    m_focusOwnerObjectName = objectName;
    emit focusOwnerObjectNameChanged();
}

void ApplicationCoordinator::clearError()
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    setLastError(Error::success());
}

ErrorCode ApplicationCoordinator::lastErrorCode() const { return m_lastError.code; }
QString ApplicationCoordinator::lastErrorMessage() const { return m_lastError.message; }
bool ApplicationCoordinator::lastErrorRecoverable() const { return m_lastError.recoverable; }

quint64 ApplicationCoordinator::issuePortalRequest(PortalKind kind,
                                                    const QString &title,
                                                    const QString &suggestedName,
                                                    const QStringList &mimeTypes)
{
    if (const Error error = verifyThread(); !error.ok()) {
        setLastError(error);
        return 0;
    }
    if (m_pendingPortal.id != 0) {
        setLastError(makeError(ErrorCode::Busy,
                               QStringLiteral("A portal request is already pending"),
                               true));
        return 0;
    }
    PortalRequest request;
    request.id = m_nextRequestId++;
    if (m_nextRequestId == 0) {
        m_nextRequestId = 1;
    }
    request.kind = kind;
    request.title = title;
    request.suggestedName = suggestedName;
    request.mimeTypes = mimeTypes;
    if (const Error error = validatePortalRequest(request); !error.ok()) {
        setLastError(error);
        return 0;
    }
    m_pendingPortal = request;
    setLastError(Error::success());
    emit portalRequestIssued(request);
    return request.id;
}

Error ApplicationCoordinator::validatePortalRequest(const PortalRequest &request) const
{
    if (request.title.trimmed().isEmpty() || request.title != request.title.trimmed()
        || request.title.size() > MaximumLabelLength
        || request.mimeTypes.size() > MaximumMimeTypeCount) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Portal title or filter count is invalid"));
    }
    if (request.kind != PortalKind::SaveFile && !request.suggestedName.isEmpty()) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Only save requests may suggest a file name"));
    }
    if (request.suggestedName == QLatin1String(".")
        || request.suggestedName == QLatin1String("..")
        || request.suggestedName.size() > 255
        || request.suggestedName.contains(QLatin1Char('/'))
        || request.suggestedName.contains(QLatin1Char('\\'))
        || request.suggestedName.contains(QChar::Null)) {
        return makeError(ErrorCode::InvalidArgument,
                         QStringLiteral("Suggested file name is invalid"));
    }
    for (const QString &mimeType : request.mimeTypes) {
        if (!mimeTypePattern().match(mimeType).hasMatch()) {
            return makeError(ErrorCode::InvalidArgument,
                             QStringLiteral("Portal MIME filter is invalid"));
        }
    }
    return Error::success();
}

Error ApplicationCoordinator::verifyThread() const
{
    if (thread() != QThread::currentThread()) {
        return makeError(ErrorCode::WrongThread,
                         QStringLiteral("Application coordinator mutation must run on its owning thread"));
    }
    return Error::success();
}

void ApplicationCoordinator::setLastError(const Error &error)
{
    if (m_lastError.code == error.code && m_lastError.message == error.message
        && m_lastError.recoverable == error.recoverable) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void ApplicationCoordinator::setMetadata(
    QString &storage,
    const QString &value,
    qsizetype maximumLength,
    void (ApplicationCoordinator::*changedSignal)())
{
    if (const Error error = verifyThread(); !error.ok()) {
        return;
    }
    const QString normalized = value.trimmed();
    if ((!value.isEmpty() && normalized.isEmpty()) || normalized.size() > maximumLength) {
        setLastError(makeError(ErrorCode::InvalidArgument,
                               QStringLiteral("Application metadata is invalid")));
        return;
    }
    if (storage == normalized) {
        return;
    }
    storage = normalized;
    setLastError(Error::success());
    emit (this->*changedSignal)();
}

} // namespace QindaQt::AppShell
