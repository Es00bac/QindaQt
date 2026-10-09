// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/android_manager/application_controller.h"
#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_search_ranker.h"

#include <QUuid>

namespace QindaQt::AndroidManager {
using namespace QindaQt::ShellLauncher;
namespace {
QString stateIdentity(ApplicationState state)
{
    switch (state) {
    case ApplicationState::NotMaterialized: return QStringLiteral("notMaterialized");
    case ApplicationState::Ready: return QStringLiteral("ready");
    case ApplicationState::Starting: return QStringLiteral("starting");
    case ApplicationState::Running: return QStringLiteral("running");
    case ApplicationState::Stopping: return QStringLiteral("stopping");
    case ApplicationState::NeedsRepair: return QStringLiteral("needsRepair");
    case ApplicationState::Unavailable: return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}
}

ApplicationController::ApplicationController(RuntimePort *port, QObject *parent, int launchTimeoutMs, SetupPort *setupPort)
    : QObject(parent), m_port(port), m_setupPort(setupPort)
{
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(qBound(1, launchTimeoutMs, 30000));
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        m_pending.reset();
        m_lastError = tr("The launch outcome could not be confirmed. Refresh before trying again.");
        Q_EMIT changed();
    });
    if (setupPort && setupPort->thread() == thread()) {
        connect(setupPort, &SetupPort::availabilityChanged, this, &ApplicationController::changed);
        connect(setupPort, &QObject::destroyed, this, [this] { m_setupPort.clear(); Q_EMIT changed(); });
    } else {
        m_setupPort.clear();
    }
    if (!port) return;
    if (port->thread() != thread()) {
        m_port.clear();
        withdraw(tr("Android support is unavailable on this window's thread."));
        return;
    }
    connect(port, &RuntimePort::snapshotChanged, this, &ApplicationController::synchronize);
    connect(port, &RuntimePort::launchFinished, this, &ApplicationController::finish);
    connect(port, &QObject::destroyed, this, [this] {
        m_port.clear();
        withdraw(tr("Android support disconnected. Refresh when it is available."));
    });
    synchronize();
}

QString ApplicationController::runtimeState() const
{
    switch (m_snapshot.state) {
    case RuntimeState::Unavailable: return QStringLiteral("unavailable");
    case RuntimeState::NeedsSetup: return QStringLiteral("needsSetup");
    case RuntimeState::Ready: return QStringLiteral("ready");
    case RuntimeState::Starting: return QStringLiteral("starting");
    case RuntimeState::NeedsRepair: return QStringLiteral("needsRepair");
    }
    return QStringLiteral("unavailable");
}

QString ApplicationController::runtimeMessage() const
{
    if (!m_snapshot.message.isEmpty()) return m_snapshot.message;
    switch (m_snapshot.state) {
    case RuntimeState::Ready: return tr("Android is ready. Choose an application to launch.");
    case RuntimeState::NeedsSetup: return tr("Set up Android before launching applications.");
    case RuntimeState::Starting: return tr("Android is starting. Launch actions will return when it is ready.");
    case RuntimeState::NeedsRepair: return tr("Android needs attention before applications can launch.");
    case RuntimeState::Unavailable: return tr("Android support is unavailable. Applications will appear after Android is set up and connected.");
    }
    return {};
}

bool ApplicationController::canSetup() const
{
    return m_setupPort && !m_pending && m_setupPort->available();
}
bool ApplicationController::setupRuntime() { return openSetup(SetupTarget::Runtime); }
bool ApplicationController::setupImages() { return openSetup(SetupTarget::Images); }
bool ApplicationController::openSetup(SetupTarget target)
{
    if (!canSetup()) {
        m_lastError = tr("Software Manager setup is unavailable.");
        Q_EMIT changed();
        return false;
    }
    const auto outcome = m_setupPort->openSelection(target);
    if (outcome.accepted()) {
        m_setupMessage = validRuntimeMessage(outcome.message) ? outcome.message : tr("Setup request sent to Software Manager.");
        m_lastError.clear();
    } else {
        m_setupMessage.clear();
        m_lastError = validRuntimeMessage(outcome.message) && !outcome.message.isEmpty()
            ? outcome.message : tr("Software Manager refused the setup request.");
    }
    Q_EMIT changed();
    return outcome.accepted();
}

void ApplicationController::withdraw(QString error)
{
    if (m_pending && error.isEmpty())
        error = tr("Android support changed before the launch outcome was confirmed.");
    m_pending.reset();
    m_deadline.stop();
    m_snapshot = {};
    m_catalog = ApplicationCatalog::build({});
    m_lastError = std::move(error);
    Q_EMIT changed();
}

void ApplicationController::synchronize()
{
    if (!m_port) { withdraw(); return; }
    const auto snapshot = m_port->snapshot();
    QString error;
    if (!validateRuntimeSnapshot(snapshot, &error)) { withdraw(error); return; }
    if (snapshot.attachmentId == m_lastAttachment && !m_lastAttachment.isEmpty()) {
        if (snapshot.revision < m_lastRevision
            || (snapshot.revision == m_lastRevision && snapshot != m_snapshot)) {
            withdraw(tr("Android status changed without a fresh revision. Refresh to recover."));
            return;
        }
        if (snapshot == m_snapshot) return;
    }
    if (m_pending) {
        bool retained = snapshot.attachmentId == m_pending->attachmentId;
        if (retained) {
            retained = false;
            for (const auto &registration : snapshot.registrations)
                retained |= registration.binding == m_pending->application;
        }
        if (!retained) {
            m_pending.reset();
            m_deadline.stop();
            m_lastError = tr("Android support or this application changed before the launch outcome was confirmed.");
        }
    }
    if (!snapshot.attachmentId.isEmpty()) {
        m_lastAttachment = snapshot.attachmentId;
        m_lastRevision = snapshot.revision;
    }
    m_snapshot = snapshot;
    m_catalog = ApplicationCatalog::build({}, snapshot.registrations);
    Q_EMIT changed();
}

const ApplicationStatus *ApplicationController::statusFor(const QString &entryId) const
{
    for (const auto &status : m_snapshot.applications)
        if (managedApplicationEntryId(status.binding) == entryId) return &status;
    return nullptr;
}

bool ApplicationController::launchAvailable(const ApplicationStatus &status) const
{
    return m_port && !m_pending && m_snapshot.state == RuntimeState::Ready
        && status.launchAvailable
        && (status.state == ApplicationState::Ready || status.state == ApplicationState::Running);
}

QVariantList ApplicationController::applications() const
{
    auto entries = m_catalog.entries();
    if (!m_query.trimmed().isEmpty()) {
        const auto matches = LauncherSearchRanker::search(entries, m_query);
        entries.clear();
        for (const auto &match : matches.results) entries.append(match.entry);
    }
    QVariantList rows;
    for (const auto &entry : entries) {
        const auto *status = statusFor(entry.id);
        if (!status) continue;
        rows.append(QVariantMap{
            {QStringLiteral("entryId"), entry.id}, {QStringLiteral("name"), entry.name},
            {QStringLiteral("iconName"), entry.iconName},
            {QStringLiteral("state"), stateIdentity(status->state)},
            {QStringLiteral("message"), status->message},
            {QStringLiteral("windowCount"), status->windowCount.value_or(0)},
            {QStringLiteral("hasWindowCount"), status->windowCount.has_value()},
            {QStringLiteral("launchEnabled"), launchAvailable(*status)},
            {QStringLiteral("launchPending"), m_pending && m_pending->application == status->binding}
        });
    }
    return rows;
}

int ApplicationController::queryLimit() const { return Bounds::maxQueryLength; }

void ApplicationController::setQuery(const QString &query)
{
    if (query.size() > Bounds::maxQueryLength) return;
    if (query == m_query) return;
    m_query = query;
    Q_EMIT changed();
}

bool ApplicationController::refresh()
{
    if (!m_port) { withdraw(tr("Android support is unavailable.")); return false; }
    m_lastError.clear();
    m_port->requestRefresh();
    synchronize();
    Q_EMIT changed();
    return true;
}

bool ApplicationController::launch(const QString &entryId)
{
    const auto *status = statusFor(entryId);
    if (m_pending) {
        m_lastError = tr("Wait for the current launch to be confirmed before starting another.");
        Q_EMIT changed();
        return false;
    }
    if (!status || !launchAvailable(*status)) {
        m_lastError = tr("This application cannot launch until Android is ready.");
        Q_EMIT changed();
        return false;
    }
    const auto intent = m_catalog.makeLaunchIntent(entryId);
    if (!intent.ok() || !intent.intent->managedBinding) return false;
    LaunchRequest request{QUuid::createUuid().toString(QUuid::WithoutBraces),
                          m_snapshot.attachmentId, m_snapshot.revision,
                          *intent.intent->managedBinding};
    m_pending = request;
    m_lastError.clear();
    m_deadline.start();
    Q_EMIT changed();
    // AGENT-GUARD: Set pending before dispatch; the port may synchronously
    // publish a refusal/completion. Accepted dispatch never means Running.
    const auto submission = m_port->requestLaunch(request);
    if (!submission.accepted && m_pending && m_pending->operationId == request.operationId) {
        m_pending.reset();
        m_deadline.stop();
        m_lastError = validRuntimeMessage(submission.message) && !submission.message.isEmpty()
            ? submission.message : tr("Android refused the launch request.");
        Q_EMIT changed();
    }
    return submission.accepted;
}

void ApplicationController::finish(const LaunchResult &result)
{
    if (!m_pending || result.operationId != m_pending->operationId
        || result.attachmentId != m_pending->attachmentId
        || result.application != m_pending->application)
        return;
    if (result.phase == LaunchPhase::Accepted) return;
    m_pending.reset();
    m_deadline.stop();
    if (result.phase != LaunchPhase::Completed) {
        m_lastError = validRuntimeMessage(result.message) && !result.message.isEmpty()
            ? result.message : tr("The Android launch outcome could not be confirmed.");
    }
    Q_EMIT changed();
    // Completion is a result, not a window/process observation. Only a fresh
    // port snapshot can change Ready/Running or the independent window count.
    if (m_port) m_port->requestRefresh();
}
} // namespace QindaQt::AndroidManager
