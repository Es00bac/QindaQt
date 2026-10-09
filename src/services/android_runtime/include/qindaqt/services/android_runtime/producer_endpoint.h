// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/android_runtime/protocol.h"
#include <QObject>
#include <memory>
namespace QindaQt::Services::AndroidRuntime {
// Helper-side private endpoint. Transfers socket/lifetime/executable FDs for
// the already admitted parent and checks its kernel identity/lifetime. Source
// collection/materialization belongs to the helper's separately qualified
// collaborators; this endpoint never invokes Waydroid or invents app facts.
class ProducerEndpoint final : public QObject {
    Q_OBJECT
public:
    ProducerEndpoint(int socketFd,int parentLifetimeFd,int parentExecutableFd,
                     int expectedParentPid,unsigned expectedUid,QByteArray channelKey,
                     QObject *parent=nullptr);
    ~ProducerEndpoint() override;
    [[nodiscard]] bool live() const;
    [[nodiscard]] bool publishSnapshot(const RuntimeSnapshot &snapshot);
    [[nodiscard]] bool publishResult(const LaunchResult &result);
Q_SIGNALS:
    void refreshRequested();
    void launchRequested(const QindaQt::Services::AndroidRuntime::LaunchRequest &request);
    void disconnected();
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::AndroidRuntime
