// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/accessibility_identity.h"

#include <QVector>

#include <algorithm>
#include <utility>

namespace QindaQt::ShellCustomizationEditor {

namespace {

QString edgeName(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
        return QStringLiteral("Top");
    case Profiles::Edge::Bottom:
        return QStringLiteral("Bottom");
    case Profiles::Edge::Left:
        return QStringLiteral("Left");
    case Profiles::Edge::Right:
        return QStringLiteral("Right");
    }
    return QStringLiteral("Unknown");
}

const Profiles::PanelSpec *findPanel(const Profiles::LayoutProfile &profile,
                                     const QString &panelId)
{
    const auto found = std::find_if(profile.panels.cbegin(),
                                    profile.panels.cend(),
                                    [&panelId](const Profiles::PanelSpec &candidate) {
                                        return candidate.id == panelId;
                                    });
    return found == profile.panels.cend() ? nullptr : &*found;
}

} // namespace

void AnnouncementCenter::announce(Announcement announcement)
{
    if (announcement.kind == AnnouncementKind::Polite) {
        m_polite = std::move(announcement);
        return;
    }
    m_assertive = std::move(announcement);
}

QVector<Announcement> AnnouncementCenter::drain()
{
    QVector<Announcement> published;
    if (m_assertive.has_value()) {
        published.append(*m_assertive);
        m_assertive.reset();
    }
    if (m_polite.has_value()) {
        published.append(*m_polite);
        m_polite.reset();
    }
    return published;
}

bool AnnouncementCenter::hasPending() const noexcept
{
    return m_polite.has_value() || m_assertive.has_value();
}

QString panelDisplayName(const Profiles::PanelSpec &panel)
{
    QString name = edgeName(panel.edge) + QStringLiteral(" panel");
    if (panel.output != QLatin1String("*")) {
        name += QStringLiteral(" on %1").arg(panel.output);
    }
    return name;
}

QString appletDisplayName(const Profiles::AppletSpec &applet)
{
    return applet.plugin;
}

QString zoneDisplayName(const QString &zone)
{
    return zone + QStringLiteral(" zone");
}

int dropPositionInSet(const Profiles::PanelSpec &targetPanel, const DropTarget &target)
{
    if (!target.beforeAppletId.has_value()) {
        return targetPanel.applets.size() + 1;
    }
    const auto found = std::find_if(targetPanel.applets.cbegin(),
                                    targetPanel.applets.cend(),
                                    [&target](const Profiles::AppletSpec &candidate) {
                                        return candidate.id == *target.beforeAppletId;
                                    });
    if (found == targetPanel.applets.cend()) {
        return targetPanel.applets.size() + 1;
    }
    return static_cast<int>(found - targetPanel.applets.cbegin()) + 1;
}

QString describeMove(const Profiles::LayoutProfile &profile,
                     const QString &appletName,
                     const DropTarget &target,
                     bool accepted,
                     const QString &rejectionReason)
{
    const Profiles::PanelSpec *panel = findPanel(profile, target.panelId);
    const QString panelName = panel != nullptr
        ? panelDisplayName(*panel)
        : (target.panelId.isEmpty() ? QStringLiteral("unknown panel") : target.panelId);
    const int position = panel != nullptr ? dropPositionInSet(*panel, target) : 1;
    const int total = (panel != nullptr ? panel->applets.size() : 0) + 1;

    QString description = QStringLiteral("Move %1 to %2, %3, position %4 of %5")
                              .arg(appletName,
                                   panelName,
                                   zoneDisplayName(target.zone))
                              .arg(position)
                              .arg(total);
    if (accepted) {
        description += QStringLiteral(" — accepted");
    } else {
        const QString reason = rejectionReason.isEmpty()
            ? QStringLiteral("the target rejected the drop")
            : rejectionReason;
        description += QStringLiteral(" — rejected: %1").arg(reason);
    }
    return description;
}

Announcement moveAnnouncement(const Profiles::LayoutProfile &profile,
                              const QString &appletName,
                              const DropTarget &target,
                              bool accepted,
                              const QString &rejectionReason)
{
    Announcement announcement;
    // Acceptance is announced politely on target change; rejections are
    // announced assertively per the architecture's parity contract.
    announcement.kind = accepted ? AnnouncementKind::Polite : AnnouncementKind::Assertive;
    announcement.message = describeMove(profile, appletName, target, accepted, rejectionReason);
    return announcement;
}

} // namespace QindaQt::ShellCustomizationEditor
