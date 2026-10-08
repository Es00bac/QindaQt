// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "voice_configuration_transport.h"
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <memory>
namespace QindaQt::Services::VoiceConfiguration {
// Caller supplies its bus; constructing/start never activates Voice1.
class QtTransport final : public Transport {
    Q_OBJECT
public:
    explicit QtTransport(QDBusConnection connection, QObject *parent = nullptr);
    ~QtTransport() override;
    void start() override;
    void stop() override;
    void fetch(const QString &owner, quint64 token) override;
    void submit(const QString &owner, quint64 token, quint64 request,
                quint64 revision, Operation operation, const QString &key) override;
private Q_SLOTS:
    void onChanged(quint64 revision, const QDBusMessage &message);
private:
    void setOwner(const QString &owner);
    void call(const QString &owner, quint64 token, const QString &method,
              const QVariantList &arguments, bool operation);
    struct Private;
    std::unique_ptr<Private> d;
};
}
