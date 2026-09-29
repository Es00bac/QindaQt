// SPDX-License-Identifier: GPL-3.0-or-later
#include "agent_identity.h"

namespace QindaQt::Apps::PolkitAgent {

QString formatUserIdentityLabel(qint64 uid, const QString &loginName, const QString &fullName)
{
    if (uid == 0) {
        return QStringLiteral("Administrator (root)");
    }
    const QString login = loginName.trimmed();
    const QString full = fullName.trimmed();
    if (!full.isEmpty() && !login.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(full, login);
    }
    if (!login.isEmpty()) {
        return login;
    }
    if (!full.isEmpty()) {
        return full;
    }
    // AGENT-NOTE: a passwd lookup can fail for an identity a remote or
    // container-mapped subject carries; an unreadable name must never look
    // like it silently vanished from the identity chooser.
    return QStringLiteral("User %1").arg(uid);
}

int selectPreferredIdentityIndex(const QList<AgentIdentity> &identities)
{
    for (qsizetype index = 0; index < identities.size(); ++index) {
        if (identities.at(index).isCurrentUser) {
            return static_cast<int>(index);
        }
    }
    return identities.isEmpty() ? -1 : 0;
}

} // namespace QindaQt::Apps::PolkitAgent
