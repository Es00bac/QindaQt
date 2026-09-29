// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/polkit_agent_selection.h"

#include <QFileInfo>

namespace QindaQt::SessionSupervisor {

QStringList defaultPolkitAgentCandidates()
{
    // ADR-0290 (owner decision, 2026-09-28): no Plasma/KDE fallback. Two
    // agents racing to register with polkitd at every login -- the
    // supervisor's KDE candidate here and XDG autostart's polkit-gnome --
    // was the live defect this agent replaces; there is exactly one
    // candidate now, and src/session_autostart's basename skip table keeps
    // every other distribution agent's autostart entry from racing it.
    return {
        QStringLiteral(QINDAQT_POLKIT_AGENT_INSTALL_PATH),
    };
}

QString resolvePolkitAgentExecutable(bool disabled, const QString &configured)
{
    if (disabled) {
        return {};
    }
    if (!configured.trimmed().isEmpty()) {
        return configured;
    }
    for (const QString &candidate : defaultPolkitAgentCandidates()) {
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return {};
}

} // namespace QindaQt::SessionSupervisor
