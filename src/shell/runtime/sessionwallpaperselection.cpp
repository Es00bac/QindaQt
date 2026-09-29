// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionwallpaperselection.h"

#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QDebug>

#include <utility>

namespace QindaQt::Shell {
namespace {

using Services::WallpaperAssignments::WallpaperAssignments;

QStringList assignmentScope()
{
    return {QString::fromLatin1(Services::WallpaperAssignments::SettingsKey)};
}

} // namespace

SessionWallpaperSelection::SessionWallpaperSelection(
    const QDBusConnection &bus, Workspaces::WorkspaceController *workspaces,
    QObject *parent)
    : WallpaperSelectionSource(parent),
      m_settingsTransport(bus),
      m_settings(m_settingsTransport, assignmentScope()),
      m_displayTransport(bus),
      m_display(&m_displayTransport),
      m_workspaces(workspaces)
{
    connect(&m_settings,
            &Services::SettingsClient::SettingsClient::snapshotChanged, this,
            &SessionWallpaperSelection::adoptAssignments);
    connect(&m_display, &DisplayClient::Client::snapshotChanged, this,
            &SessionWallpaperSelection::adoptDisplays);
    if (m_workspaces) {
        connect(m_workspaces.data(), &Workspaces::WorkspaceController::stateChanged,
                this, &SessionWallpaperSelection::adoptDesktop);
    }
}

SessionWallpaperSelection::~SessionWallpaperSelection() = default;

void SessionWallpaperSelection::start()
{
    QString error;
    if (!m_settings.start(&error)) {
        qWarning().noquote()
            << "QindaQt wallpaper choices unavailable; every display shows"
               " the everywhere wallpaper:"
            << error;
    }
    m_display.start();
    adoptDesktop();
}

const WallpaperAssignments &SessionWallpaperSelection::assignments() const
{
    return m_assignments;
}

QString SessionWallpaperSelection::displayIdForConnector(const QString &connectorName) const
{
    return m_displayIds.value(connectorName);
}

QString SessionWallpaperSelection::currentDesktopId() const
{
    return m_desktop;
}

void SessionWallpaperSelection::adoptAssignments()
{
    const auto &snapshot = m_settings.snapshot();
    if (!snapshot) {
        return;
    }
    const auto decoded = WallpaperAssignments::decodeSettingsValue(
        snapshot->values.value(QString::fromLatin1(Services::WallpaperAssignments::SettingsKey)));
    // AGENT-NOTE: a malformed stored value shows the everywhere wallpaper on
    // every display (fail closed to today's behaviour). Appearance Settings
    // reports it and replaces it on the next explicit choice (ADR-0286).
    if (!decoded.ok() && !m_reportedUnreadable) {
        m_reportedUnreadable = true;
        qWarning().noquote() << "QindaQt ignored unreadable wallpaper choices:"
                             << decoded.error;
    }
    WallpaperAssignments next = decoded.value.value_or(WallpaperAssignments{});
    if (next == m_assignments) {
        return;
    }
    m_assignments = std::move(next);
    Q_EMIT changed();
}

void SessionWallpaperSelection::adoptDisplays(const Display::Snapshot &snapshot)
{
    // AGENT-CONTRACT: Display1's connector name is the compositor output name
    // that Qt's Wayland platform reports as QScreen::name() (the same join the
    // notification output authority relies on). An ambiguous identity (ADR-0017
    // twins) is not addressable: like an alias, a wallpaper choice must never
    // silently follow the wrong one of two identical monitors.
    QHash<QString, QString> next;
    for (const Display::Output &output : snapshot.outputs) {
        if (output.ambiguousIdentity || output.connectorName.isEmpty()
            || !WallpaperAssignments::isValidDisplayId(output.stableId)) {
            continue;
        }
        next.insert(output.connectorName, output.stableId);
    }
    if (next == m_displayIds) {
        return;
    }
    m_displayIds = std::move(next);
    Q_EMIT changed();
}

void SessionWallpaperSelection::adoptDesktop()
{
    const QString next = m_workspaces ? m_workspaces->currentId() : QString();
    if (next == m_desktop) {
        return;
    }
    m_desktop = next;
    Q_EMIT changed();
}

} // namespace QindaQt::Shell
