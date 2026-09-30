// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/legacy_snapshot.h>
namespace qindaqt::keyring {
struct LegacySourceBinding {
    QString kind,service,uniqueOwner,sessionOwner;
    quint64 pid=0; // Independently selected expected source PID, not app identity.
};
struct LegacyReadReceipt {
    CollectionImportError error=CollectionImportError::None;
    LegacySnapshot snapshot;
};
// Owned one-time compatibility reader; same-thread, no resident dependency.
// Requires explicit pinned source owner/PID and accepted session owner. Borrowed
// readonly admission callback outlives reader and joins ordinary display lifetime.
// Every accepted provider reply/signal checks its ACTUAL sender, current source
// identity and retained PIDFD. No activation/writes/name switch/private files.
// acquire is bounded by a ten-minute total budget and thirty-second prompts;
// two exact read passes reject observed source mutation; known provider data
// change signals permanently retire a completed snapshot. Public APIs provide no
// atomic export: the operator must keep the source quiescent through publication.
// Temporary libdbus wire allocations are not locked/wiped by SecureBuffer;
// composition disables cores/dumping before reads and disposes each reply promptly.
// Returned DTO secrets own locked/wiped pages, never printed or converted to text.
class LegacyReader {
public:
    virtual ~LegacyReader()=default;
    virtual LegacyReadReceipt acquire(const QStringList &requestedAliases={"default"})=0;
    virtual bool live()=0;
};
std::unique_ptr<LegacyReader> openLegacyReader(const QString &busAddress,LegacySourceBinding,
    const std::function<bool()> &admitted);
}
