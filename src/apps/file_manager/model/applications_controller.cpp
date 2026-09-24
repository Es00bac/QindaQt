// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/applications_controller.h"

#include "model/applications_listing.h"
#include "model/applications_location.h"

#include "qindaqt/application_catalog/category_tree.h"
#include "qindaqt/application_catalog/launch_support.h"

#include <QProcess>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

using QindaQt::ApplicationCatalog::LaunchSupport;
using QindaQt::ShellLauncher::DiagnosticKind;

constexpr int maxReportedDiagnostics = 3;

} // namespace

ApplicationLaunchSeams ApplicationLaunchSeams::production()
{
    return {&chooseApplicationOnCompositor,
            [](const QString &program, const QStringList &arguments) {
                return QProcess::startDetached(program, arguments);
            }};
}

ApplicationsController::ApplicationsController(QStringList dataRoots,
                                               QObject *parent)
    : ApplicationsController(std::move(dataRoots),
                             ApplicationLaunchSeams::production(), parent)
{
}

ApplicationsController::ApplicationsController(QStringList dataRoots,
                                               ApplicationLaunchSeams seams,
                                               QObject *parent)
    : QObject(parent)
    , m_dataRoots(std::move(dataRoots))
    , m_seams(std::move(seams))
{
}

ApplicationsController::~ApplicationsController() = default;

QString ApplicationsController::location()
{
    return ApplicationsLocation::location();
}

void ApplicationsController::setChooserMode(bool enabled)
{
    if (m_chooserMode == enabled) {
        return;
    }
    m_chooserMode = enabled;
    Q_EMIT chooserModeChanged();
    // Row notes depend on the mode (every entry is choosable in a picker).
    Q_EMIT catalogChanged();
}

ListingResult ApplicationsController::listing() const
{
    ListingResult result;
    result.path = location();
    result.truncated = m_truncated;
    result.entries.reserve(m_scan.applications.size());
    for (const auto &scanned : std::as_const(m_scan.applications)) {
        result.entries.append(ApplicationsListing::row(
            scanned, m_categories.value(scanned.entry.id), m_chooserMode));
    }
    return result;
}

QVariantMap ApplicationsController::describe(const QString &entryId) const
{
    const auto *scanned = m_scan.application(entryId);
    if (!scanned) {
        return {};
    }
    return ApplicationsListing::describe(*scanned, m_categories.value(entryId),
                                         m_chooserMode);
}

QString ApplicationsController::open(const QString &entryId)
{
    if (m_chooserMode) {
        chooseForWorkspace(entryId);
    } else {
        activateEntry(entryId);
    }
    return m_lastError;
}

void ApplicationsController::chooseForWorkspace(const QString &entryId)
{
    // AGENT-CONTRACT: the picker never names its own window; the compositor
    // resolves the choice against the active window, which is this picker
    // while the user clicks in it (ADR-0165). The window stays open until
    // the compositor swaps it out and closes it.
    setLastError({});
    const auto reply = m_seams.chooseOnCompositor(entryId);
    if (reply.accepted()) {
        Q_EMIT chooserSucceeded();
        return;
    }
    setLastError(reply.message);
}

void ApplicationsController::clearLastError()
{
    setLastError({});
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
    // The shared tree is the single category authority (ADR-0164); the
    // Applications place only needs each entry's primary group label.
    m_categories = ApplicationsListing::categoryLabels(
        QindaQt::ApplicationCatalog::buildCategoryTree(entries));
    m_truncated = m_scan.diagnosticsTruncated
        || std::any_of(m_scan.diagnostics.cbegin(), m_scan.diagnostics.cend(),
                       [](const auto &diagnostic) {
                           return diagnostic.kind == DiagnosticKind::SourceLimitReached
                               || diagnostic.kind == DiagnosticKind::EntryLimitReached;
                       });

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
    Q_EMIT catalogChanged();
}

void ApplicationsController::activateEntry(const QString &entryId)
{
    setLastError({});
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
    if (m_seams.chooseOnCompositor(entryId).accepted()) {
        Q_EMIT chooserSucceeded();
        return;
    }
    const auto preparation = QindaQt::ApplicationCatalog::planApplicationLaunch(
        scanned->documentText, QString(), scanned->entry.name,
        scanned->desktopFilePath);
    if (preparation.support != LaunchSupport::ProcessSpawn) {
        setLastError(ApplicationsListing::standaloneLimitation(*scanned, false));
        return;
    }
    // AGENT-GUARD: One detached launch per activation; the started process
    // outlives the file manager, so failures after a successful spawn are not
    // reportable and no child handle is retained.
    if (m_seams.startDetached(preparation.program, preparation.arguments)) {
        return;
    }
    setLastError(QStringLiteral("Could not start '%1'").arg(scanned->entry.name));
}

} // namespace QindaQt::Apps::FileManager
