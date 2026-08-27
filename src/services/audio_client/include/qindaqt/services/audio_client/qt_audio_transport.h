// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Audio
{

class QtAudioTransport final : public AudioTransport
{
    Q_OBJECT

public:
    // Retains a value handle to connection; the named Qt connection must stay
    // registered for this object's lifetime. Use this object on its creating
    // Qt thread only. D-Bus errors are normalized to bounded reason codes.
    explicit QtAudioTransport(const QDBusConnection &connection,
                              QString serviceName = {}, QObject *parent = nullptr);
    ~QtAudioTransport() override;

    void start() override;
    void stop() override;
    void fetchSnapshot(const QString &owner, quint64 requestId) override;
    void submitOperation(const QString &owner, quint64 requestId,
                         const OperationRequest &request) override;

private Q_SLOTS:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onChanged(quint64 epoch, quint64 revision);

private:
    void setOwner(const QString &owner);
    void queryInitialOwner();

    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Audio
