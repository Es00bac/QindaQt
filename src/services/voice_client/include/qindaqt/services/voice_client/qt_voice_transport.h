// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_client/voice_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Services::Voice {

// The only D-Bus in the voice consumer stack. Everything above it sees the
// abstract VoiceTransport, so the client and the applet are testable without a
// session bus or an installed provider.
class QtVoiceTransport final : public VoiceTransport {
    Q_OBJECT
public:
    explicit QtVoiceTransport(const QDBusConnection &connection,
                              QString serviceName = {}, QObject *parent = nullptr);
    ~QtVoiceTransport() override;
    void start() override;
    void stop() override;
    void fetchSnapshot(const QString &owner, quint64 token) override;
    void submitOperation(const QString &owner, quint64 token,
                         const OperationRequest &request) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
    void queryOwner();
    void requestActivation(quint64 generation);
    void setOwner(const QString &owner);
    void onOwnerChanged(const QString &service, const QString &oldOwner,
                        const QString &newOwner);

private Q_SLOTS:
    void onChanged(quint64 revision);
    void onLevel(uint levelPercent);
};

} // namespace QindaQt::Services::Voice
