// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/applications_controller.h"

#include "qindaqt/application_catalog/launch_support.h"
#include "model/workspace_chooser_client.h"

#include <QProcess>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

using QindaQt::ApplicationCatalog::LaunchSupport;

constexpr int maxReportedDiagnostics = 3;

QVariantMap folderRow(const QindaQt::ApplicationCatalog::CategoryNode &folder)
{
    int count = int(folder.entries.size());
    for (const auto &child : folder.children) {
        count += int(child.entries.size());
    }
    return {{QStringLiteral("id"), folder.id},
            {QStringLiteral("label"), folder.label},
            {QStringLiteral("entryCount"), count}};
}

QVariantMap entryRow(const QindaQt::ShellLauncher::ApplicationEntry &entry,
                     const QindaQt::ApplicationCatalog::ScannedApplication *scanned)
{
    // Launchability is decided from the retained document; entries whose
    // scan lost their document (should not happen: both derive from the same
    // walk) fail closed as non-launchable.
    const auto preparation = scanned
        ? QindaQt::ApplicationCatalog::planApplicationLaunch(
              scanned->documentText, QString(), entry.name,
              scanned->desktopFilePath)
        : QindaQt::ApplicationCatalog::LaunchPreparation{};
    const bool launchable = preparation.support == LaunchSupport::ProcessSpawn;
    QString message;
    if (scanned == nullptr) {
        message = QStringLiteral("Application document is unavailable");
    } else if (preparation.support == LaunchSupport::TerminalRequired) {
        message = QStringLiteral("Runs in a terminal; launch it from a workspace picker");
    } else if (preparation.support == LaunchSupport::DbusActivatable) {
        message = QStringLiteral("Starts through D-Bus activation; launch it from a workspace picker");
    } else if (!launchable) {
        message = preparation.message;
    }
    return {{QStringLiteral("id"), entry.id},
            {QStringLiteral("name"), entry.name},
            {QStringLiteral("iconName"), entry.iconName},
            {QStringLiteral("genericName"), entry.genericName},
            {QStringLiteral("comment"), entry.comment},
            {QStringLiteral("launchable"), launchable},
            {QStringLiteral("message"), message}};
}

} // namespace

ApplicationsController::ApplicationsController(QStringList dataRoots,
                                               QObject *parent)
    : QObject(parent)
    , m_dataRoots(std::move(dataRoots))
{
}

ApplicationsController::~ApplicationsController() = default;

const QindaQt::ApplicationCatalog::CategoryNode *
ApplicationsController::openNode() const
{
    if (m_open.groupId.isEmpty()) {
        return &m_tree;
    }
    const auto *group = m_tree.child(m_open.groupId);
    if (!group) {
        return &m_tree;
    }
    if (m_open.childToken.isEmpty()) {
        return group;
    }
    return group->child(m_open.childToken);
}

QVariantList ApplicationsController::folders() const
{
    QVariantList rows;
    const auto *node = openNode();
    if (!node) {
        return rows;
    }
    // Child folders exist only at the group level; a leaf child has none.
    if (m_open.childToken.isEmpty()) {
        for (const auto &folder : node->children) {
            rows.append(folderRow(folder));
        }
    }
    return rows;
}

QVariantList ApplicationsController::entries() const
{
    QVariantList rows;
    const auto *node = openNode();
    if (!node) {
        return rows;
    }
    for (const auto &entry : node->entries) {
        rows.append(entryRow(entry, m_scan.application(entry.id)));
    }
    return rows;
}

QString ApplicationsController::breadcrumb() const
{
    if (m_open.groupId.isEmpty()) {
        return {};
    }
    QString crumb = m_open.groupId;
    if (!m_open.childToken.isEmpty()) {
        // Humanize the child token the same way the tree labels it.
        const auto *group = m_tree.child(m_open.groupId);
        if (group) {
            const auto *child = group->child(m_open.childToken);
            if (child) {
                crumb += QStringLiteral(" › ") + child->label;
            }
        }
    }
    return crumb;
}

void ApplicationsController::setChooserMode(bool enabled)
{
    if (m_chooserMode == enabled) {
        return;
    }
    m_chooserMode = enabled;
    Q_EMIT chooserModeChanged();
}

void ApplicationsController::chooseForWorkspace(const QString &entryId)
{
    // AGENT-CONTRACT: the picker never names its own window; the compositor
    // resolves the choice against the active window, which is this picker
    // while the user clicks in it (ADR-0165). The window stays open until
    // the compositor swaps it out and closes it.
    const auto reply = chooseApplicationOnCompositor(entryId);
    if (reply.accepted()) {
        Q_EMIT chooserSucceeded();
        return;
    }
    setLastError(reply.message);
}

void ApplicationsController::setLastError(QString message)
{
    if (m_lastError == message) {
        return;
    }
    m_lastError = std::move(message);
    Q_EMIT lastErrorChanged();
}

void ApplicationsController::refresh()
{
    QString scanError;
    m_scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(
        m_dataRoots, &scanError);
    QVector<QindaQt::ShellLauncher::ApplicationEntry> entries;
    entries.reserve(m_scan.applications.size());
    for (const auto &scanned : std::as_const(m_scan.applications)) {
        entries.append(scanned.entry);
    }
    m_tree = QindaQt::ApplicationCatalog::buildCategoryTree(entries);

    QStringList summary;
    summary.append(scanError);
    for (qsizetype index = 0;
         index < m_scan.diagnostics.size()
         && summary.size() <= maxReportedDiagnostics;
         ++index) {
        summary.append(QStringLiteral("%1: %2")
                           .arg(m_scan.diagnostics.at(index).sourceId,
                                m_scan.diagnostics.at(index).message));
    }
    summary.removeAll(QString());
    QString reported;
    if (!summary.isEmpty()) {
        reported = summary.join(QStringLiteral("; "));
        if (m_scan.diagnostics.size() > maxReportedDiagnostics
            || m_scan.diagnosticsTruncated) {
            reported += QStringLiteral(" (more suppressed)");
        }
    }
    setLastError(reported);

    if (!m_ready) {
        m_ready = true;
        Q_EMIT readyChanged();
    }
    Q_EMIT treeChanged();
}

void ApplicationsController::openFolder(const QString &folderId)
{
    if (folderId.isEmpty()) {
        if (!m_open.groupId.isEmpty() || !m_open.childToken.isEmpty()) {
            m_open = {};
            Q_EMIT treeChanged();
        }
        return;
    }
    const QString groupId = folderId.section(QLatin1Char('/'), 0, 0);
    const QString childToken = folderId.section(QLatin1Char('/'), 1);
    if (!m_tree.child(groupId)
        || (!childToken.isEmpty() && !m_tree.child(groupId)->child(childToken))) {
        return;
    }
    if (m_open.groupId == groupId && m_open.childToken == childToken) {
        return;
    }
    m_open = {groupId, childToken};
    Q_EMIT treeChanged();
}

void ApplicationsController::openParentFolder()
{
    if (m_open.childToken.isEmpty()) {
        openFolder(QString());
        return;
    }
    m_open.childToken.clear();
    Q_EMIT treeChanged();
}

void ApplicationsController::activateEntry(const QString &entryId)
{
    const auto *scanned = m_scan.application(entryId);
    if (!scanned) {
        setLastError(QStringLiteral("Application '%1' is no longer installed")
                         .arg(entryId));
        return;
    }
    // ADR-0172: when this window is docked in a container, the application the
    // user picked takes its PLACE rather than opening somewhere else. The
    // compositor owns that decision: it accepts only when the calling window is
    // the active one and is a container member, and it performs the launch and
    // the atomic swap itself. A rejection is the ordinary undocked case, so it
    // falls through to a plain launch rather than surfacing an error.
    //
    // AGENT-NOTE: the compositor route is tried FIRST because it also owns the
    // full desktop-entry launch facility. Terminal-required and
    // D-Bus-activatable entries, which cannot be spawned below, therefore work
    // while docked even though the local fallback still refuses them.
    if (chooseApplicationOnCompositor(entryId).accepted()) {
        Q_EMIT chooserSucceeded();
        return;
    }
    const auto preparation = QindaQt::ApplicationCatalog::planApplicationLaunch(
        scanned->documentText, QString(), scanned->entry.name,
        scanned->desktopFilePath);
    if (preparation.support != LaunchSupport::ProcessSpawn) {
        setLastError(preparation.message.isEmpty()
                         ? QStringLiteral("This application cannot be started directly")
                         : preparation.message);
        return;
    }
    // AGENT-GUARD: One detached launch per activation; the started process
    // outlives the file manager, so failures after a successful spawn are not
    // reportable and no child handle is retained.
    if (QProcess::startDetached(preparation.program, preparation.arguments)) {
        return;
    }
    setLastError(QStringLiteral("Could not start '%1'").arg(scanned->entry.name));
}

} // namespace QindaQt::Apps::FileManager
