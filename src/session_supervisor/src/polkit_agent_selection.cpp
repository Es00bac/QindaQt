// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/polkit_agent_selection.h"

#include <QFileInfo>

namespace QindaQt::SessionSupervisor {

QStringList defaultPolkitAgentCandidates()
{
    return {
        QStringLiteral("/usr/libexec/polkit-kde-authentication-agent-1"),
        QStringLiteral("/usr/lib/polkit-kde-authentication-agent-1"),
        QStringLiteral("/usr/lib64/libexec/polkit-kde-authentication-agent-1"),
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
