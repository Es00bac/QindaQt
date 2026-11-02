// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <QDBusConnection>
namespace qindaqt::keyring::service {
struct RequesterIdentity {
    quint32 pid = 0, uid = 0;
    quint64 started = 0;
    QString executable;
    QStringList parents;
};
// Same-thread, bounded /proc reads and bus credentials. No argv, environment,
// script contents or secrets. Failed lookup never falls back to caller labels.
RequesterIdentity identifyRequester(const QDBusConnection &, const QString &owner);
RequesterIdentity identifyLocalRequester();
bool requesterUnchanged(const QDBusConnection &, const QString &owner, const RequesterIdentity &);
void describeRequester(protocol::PromptMetadata &, const RequesterIdentity &);
}
