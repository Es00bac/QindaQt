// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "voice_configuration_transport.h"
#include <memory>
namespace QindaQt::Services::VoiceConfiguration {
// Caller supplies the session bus address. Same-thread, nonactivating native
// reply authority integrated with Qt; no Qt private connection access.
class QtTransport final : public Transport {
    Q_OBJECT
public:
    explicit QtTransport(QString busAddress, QObject *parent = nullptr);
    ~QtTransport() override;
    void start() override;
    void stop() override;
    void fetch(const QString &owner, quint64 token) override;
    void submit(const QString &owner, quint64 token, quint64 request,
                quint64 revision, Operation operation, const QString &key) override;
private:
    void setOwner(const QString &owner);
    void process();
    void scheduleProcess();
    void call(const QString &owner, quint64 token, const QString &method,
              const QVariantList &arguments, bool operation);
    struct Private;
    std::unique_ptr<Private> d;
};
}
