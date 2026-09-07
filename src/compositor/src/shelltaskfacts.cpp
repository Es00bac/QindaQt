// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shelltaskfacts.h"

#include "qindaqt/compositor/containerappearance.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QSet>

#include <limits>
#include <utility>

namespace QindaQt::Compositor {
namespace {

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

bool cleanText(const QString &value, bool required)
{
    if ((required && value.isEmpty())
        || value.size() > ShellTaskFactsMaximumTextCharacters
        || value.contains(QChar::Null)) {
        return false;
    }
    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Other_Format) {
            return false;
        }
        if (character.isHighSurrogate()) {
            if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate()) {
                return false;
            }
            ++index;
        } else if (character.isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

QString roleName(ShellTaskWindowRole role)
{
    switch (role) {
    case ShellTaskWindowRole::Standalone: return QStringLiteral("standalone");
    case ShellTaskWindowRole::ContainerPrimary: return QStringLiteral("container-primary");
    case ShellTaskWindowRole::ContainerMember: return QStringLiteral("container-member");
    }
    return {};
}

QString authorityName(ShellTaskContainerAuthority authority)
{
    return authority == ShellTaskContainerAuthority::ControlBridge
        ? QStringLiteral("control-bridge") : QStringLiteral("hybrid-process");
}

QString windowTypeName(ShellTaskWindowType type)
{
    return type == ShellTaskWindowType::Normal
        ? QStringLiteral("normal") : QStringLiteral("non-normal");
}

QString windowOwnerName(ShellTaskWindowOwner owner)
{
    return owner == ShellTaskWindowOwner::Application
        ? QStringLiteral("application") : QStringLiteral("bound-shell");
}

QJsonArray stringArray(const QStringList &values)
{
    QJsonArray result;
    for (const QString &value : values) {
        result.append(value);
    }
    return result;
}

QByteArray unavailableJson(const QString &epoch, quint64 revision,
                           const QString &code, const QString &message)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("unavailable")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("epoch"), epoch},
        {QStringLiteral("revision"), QString::number(revision)},
        {QStringLiteral("failure"),
         QJsonObject{{QStringLiteral("code"), code},
                     {QStringLiteral("message"), message}}},
    }).toJson(QJsonDocument::Compact);
}

QJsonObject stateJson(const ShellTaskFactsCandidate &candidate)
{
    QJsonArray outputs;
    for (const auto &output : candidate.outputs) {
        outputs.append(QJsonObject{{QStringLiteral("id"), output.id}});
    }
    QJsonArray workspaces;
    for (const auto &workspace : candidate.workspaces) {
        workspaces.append(QJsonObject{{QStringLiteral("id"), workspace.id}});
    }
    QJsonArray containers;
    for (const auto &container : candidate.containers) {
        containers.append(QJsonObject{
            {QStringLiteral("id"), container.id},
            {QStringLiteral("revision"), QString::number(container.revision)},
            {QStringLiteral("authority"), authorityName(container.authority)},
        });
    }
    QJsonArray windows;
    for (const auto &window : candidate.windows) {
        windows.append(QJsonObject{
            {QStringLiteral("id"), window.windowId},
            {QStringLiteral("applicationId"), window.applicationId},
            {QStringLiteral("applicationName"), window.applicationName},
            {QStringLiteral("title"), window.title},
            {QStringLiteral("colorHex"), window.colorHex},
            {QStringLiteral("role"), roleName(window.role)},
            {QStringLiteral("windowType"), windowTypeName(window.type)},
            {QStringLiteral("ownerRole"), windowOwnerName(window.owner)},
            {QStringLiteral("active"), window.active},
            {QStringLiteral("minimized"), window.minimized},
            {QStringLiteral("maximized"), window.maximized},
            {QStringLiteral("fullscreen"), window.fullscreen},
            {QStringLiteral("demandsAttention"), window.demandsAttention},
            {QStringLiteral("outputId"), window.outputId},
            {QStringLiteral("workspaceIds"), stringArray(window.workspaceIds)},
            {QStringLiteral("onAllWorkspaces"), window.onAllWorkspaces},
            {QStringLiteral("containerId"), window.containerId},
        });
    }
    return {
        {QStringLiteral("actionRevision"),
         QString::number(candidate.actionGeneration.revision)},
        {QStringLiteral("outputs"), outputs},
        {QStringLiteral("workspaces"), workspaces},
        {QStringLiteral("containers"), containers},
        {QStringLiteral("windows"), windows},
    };
}

bool validateContainers(const ShellTaskFactsCandidate &candidate,
                        QSet<QString> *containerIds, QString *error)
{
    if (candidate.containers.size() > ShellTaskFactsMaximumContainers) {
        setError(error, QStringLiteral("task container count exceeds the bound"));
        return false;
    }
    for (const auto &container : candidate.containers) {
        if (!cleanText(container.id, true) || container.revision == 0
            || containerIds->contains(container.id)) {
            setError(error, QStringLiteral("task container lineage is invalid"));
            return false;
        }
        containerIds->insert(container.id);
    }
    return true;
}

bool validateWindows(const ShellTaskFactsCandidate &candidate,
                     const QSet<QString> &outputIds,
                     const QSet<QString> &workspaceIds,
                     const QSet<QString> &containerIds, QString *error)
{
    if (candidate.windows.size() > ShellTaskFactsMaximumWindows) {
        setError(error, QStringLiteral("task window count exceeds the bound"));
        return false;
    }
    QSet<QString> windowIds;
    QHash<QString, int> primaryCounts;
    QHash<QString, int> memberCounts;
    int activeCount = 0;
    for (const auto &window : candidate.windows) {
        const bool grouped = window.role != ShellTaskWindowRole::Standalone;
        if (!cleanText(window.windowId, true)
            || !cleanText(window.applicationId, true)
            || !cleanText(window.applicationName, true)
            || !cleanText(window.title, false)
            || !cleanText(window.outputId, true)
            || windowIds.contains(window.windowId)
            || !outputIds.contains(window.outputId)
            || grouped != !window.containerId.isEmpty()
            || (grouped && !containerIds.contains(window.containerId))
            || (window.active && window.minimized)
            || (!window.colorHex.isEmpty() && !isValidContainerColor(window.colorHex))
            || (!window.colorHex.isEmpty()
                && window.role != ShellTaskWindowRole::ContainerPrimary)) {
            setError(error, QStringLiteral("task window facts are invalid"));
            return false;
        }
        if (window.workspaceIds.size() > ShellTaskFactsMaximumWorkspaces
            || window.onAllWorkspaces != window.workspaceIds.isEmpty()) {
            setError(error, QStringLiteral("task window workspace scope is invalid"));
            return false;
        }
        QSet<QString> seenWorkspaces;
        for (const QString &workspace : window.workspaceIds) {
            if (!workspaceIds.contains(workspace) || seenWorkspaces.contains(workspace)) {
                setError(error, QStringLiteral("task window references an invalid workspace"));
                return false;
            }
            seenWorkspaces.insert(workspace);
        }
        windowIds.insert(window.windowId);
        activeCount += window.active ? 1 : 0;
        if (window.role == ShellTaskWindowRole::ContainerPrimary) {
            ++primaryCounts[window.containerId];
        } else if (window.role == ShellTaskWindowRole::ContainerMember) {
            ++memberCounts[window.containerId];
        }
    }
    if (activeCount > 1) {
        setError(error, QStringLiteral("task facts contain multiple active windows"));
        return false;
    }
    for (const QString &containerId : containerIds) {
        if (primaryCounts.value(containerId) != 1
            || memberCounts.value(containerId) < 1
            || windowIds.contains(containerId)) {
            setError(error, QStringLiteral("task container membership is incomplete"));
            return false;
        }
    }
    return true;
}

} // namespace

bool ShellTaskFactsSnapshot::available() const noexcept
{
    return status == ShellTaskFactsStatus::Ok;
}

bool validateShellTaskFactsCandidate(const ShellTaskFactsCandidate &candidate,
                                     QString *error)
{
    if (!candidate.actionGeneration.isValid()) {
        setError(error, QStringLiteral("task action generation is invalid"));
        return false;
    }
    if (candidate.outputs.isEmpty()
        || candidate.outputs.size() > ShellTaskFactsMaximumOutputs
        || candidate.workspaces.isEmpty()
        || candidate.workspaces.size() > ShellTaskFactsMaximumWorkspaces) {
        setError(error, QStringLiteral("task output or workspace inventory is invalid"));
        return false;
    }
    QSet<QString> outputIds;
    for (const auto &output : candidate.outputs) {
        if (!cleanText(output.id, true) || outputIds.contains(output.id)) {
            setError(error, QStringLiteral("task output identity is invalid"));
            return false;
        }
        outputIds.insert(output.id);
    }
    QSet<QString> workspaceIds;
    for (const auto &workspace : candidate.workspaces) {
        if (!cleanText(workspace.id, true) || workspaceIds.contains(workspace.id)) {
            setError(error, QStringLiteral("task workspace identity is invalid"));
            return false;
        }
        workspaceIds.insert(workspace.id);
    }
    QSet<QString> containerIds;
    return validateContainers(candidate, &containerIds, error)
        && validateWindows(candidate, outputIds, workspaceIds, containerIds, error);
}

ShellTaskFactsStore::ShellTaskFactsStore(QString epoch, quint64 revisionSeed)
    : m_snapshotJson(unavailableJson(
          epoch, revisionSeed, QStringLiteral("task-facts-unavailable"),
          QStringLiteral("no coherent task facts have been published")))
    , m_epoch(std::move(epoch))
    , m_revision(revisionSeed)
{
}

ShellTaskFactsPublishResult ShellTaskFactsStore::publish(
    const ShellTaskFactsCandidate &candidate, QString *error)
{
    if (candidate.actionGeneration.epoch != m_epoch
        || !validateShellTaskFactsCandidate(candidate, error)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("task facts epoch does not match the store");
        }
        return ShellTaskFactsPublishResult::Rejected;
    }
    QJsonObject state = stateJson(candidate);
    const QByteArray canonical = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (m_available && canonical == m_canonicalState) {
        setError(error, {});
        return ShellTaskFactsPublishResult::Unchanged;
    }
    if (m_revision == std::numeric_limits<quint64>::max()) {
        setError(error, QStringLiteral("task facts revision is exhausted"));
        return ShellTaskFactsPublishResult::RevisionExhausted;
    }
    state.insert(QStringLiteral("status"), QStringLiteral("ok"));
    state.insert(QStringLiteral("schemaVersion"), 1);
    state.insert(QStringLiteral("epoch"), m_epoch);
    state.insert(QStringLiteral("revision"), QString::number(m_revision + 1));
    const QByteArray payload = QJsonDocument(state).toJson(QJsonDocument::Compact);
    if (payload.size() > ShellTaskFactsMaximumPayloadBytes) {
        setError(error, QStringLiteral("task facts payload exceeds the bound"));
        return ShellTaskFactsPublishResult::Rejected;
    }
    ++m_revision;
    m_canonicalState = canonical;
    m_snapshotJson = payload;
    m_available = true;
    setError(error, {});
    return ShellTaskFactsPublishResult::Published;
}

bool ShellTaskFactsStore::markUnavailable(const QString &code,
                                          const QString &message)
{
    if (!m_available) {
        return false;
    }
    m_available = false;
    if (m_revision < std::numeric_limits<quint64>::max()) {
        ++m_revision;
    }
    m_snapshotJson = unavailableJson(m_epoch, m_revision, code, message);
    return true;
}

const QByteArray &ShellTaskFactsStore::snapshotJson() const noexcept { return m_snapshotJson; }
const QString &ShellTaskFactsStore::epoch() const noexcept { return m_epoch; }
quint64 ShellTaskFactsStore::revision() const noexcept { return m_revision; }

ShellTaskFactsController::ShellTaskFactsController(
    ShellWindowCredentialSource &credentials, ShellPanelOwnerSource &panelOwner,
    ShellTaskFactsSource &source)
    : m_credentials(credentials), m_panelOwner(panelOwner), m_source(source)
{
}

bool ShellTaskFactsController::authorized(const QString &callerUniqueName) const
{
    const auto panelPid = m_panelOwner.shellPanelProcessId();
    if (!panelPid || *panelPid <= 1 || !callerUniqueName.startsWith(u':')) {
        return false;
    }
    const auto callerPid = m_credentials.processIdForUniqueName(callerUniqueName);
    return callerPid && *callerPid == *panelPid;
}

QByteArray ShellTaskFactsController::snapshot(const QString &callerUniqueName)
{
    // AGENT-GUARD: The source may sample every managed KWin window. The PID
    // join must precede that call so unauthorized peers get constant-size
    // denial independent of titles, counts, or current focus.
    if (!authorized(callerUniqueName)) {
        return QJsonDocument(QJsonObject{
            {QStringLiteral("status"), QStringLiteral("unauthorized")},
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("failure"),
             QJsonObject{{QStringLiteral("code"), QStringLiteral("caller-pid-mismatch")},
                         {QStringLiteral("message"),
                          QStringLiteral("the D-Bus caller does not own the shell panels")}}},
        }).toJson(QJsonDocument::Compact);
    }
    return m_source.snapshotJson();
}

} // namespace QindaQt::Compositor
