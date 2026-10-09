// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/runtime_port.h"
#include "qindaqt/services/android_runtime/protocol.h"
#include <memory>
namespace QindaQt::Services::AndroidRuntime {
struct AuthenticatedProducerPeer {
    qint64 processId = 0;
    quint32 userId = 0;
};

// Owning private-descriptor RuntimePort. Caller transfers three distinct FDs:
// private seqpacket socket, lifetime pidfd held from supervised birth, and the
// already verified native producer executable object. Caller has admitted its
// installed executable/session/runtime/recipe context; these parameters must
// never come from an untrusted message, public bus name or command-line mode.
// Kernel packet PID/UID + live pidfd + unchanged executable are checked before
// publication and on every getter/send. No automatic mutation replay or kill.
// This transport does not collect guest signer/UID/task/surface evidence.
class DescriptorRuntimePort final : public RuntimePort {
    Q_OBJECT
public:
    DescriptorRuntimePort(int socketFd,int lifetimeFd,int executableFd,
                          int expectedPid,unsigned expectedUid,QByteArray channelKey,
                          QObject *parent=nullptr);
    ~DescriptorRuntimePort() override;
    RuntimeSnapshot snapshot() const override;
    void requestRefresh() override;
    LaunchSubmission requestLaunch(const LaunchRequest &request) override;
    [[nodiscard]] bool producerLive() const;
    // Present only after a valid credential/key packet and while the pinned
    // process still lives. This is host process identity, never renderer role
    // or guest app/task identity; role admission remains trusted bootstrap.
    [[nodiscard]] std::optional<AuthenticatedProducerPeer> authenticatedPeer() const;
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::AndroidRuntime
