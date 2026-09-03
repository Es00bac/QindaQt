// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_client/clipboard_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Services::Clipboard {

class QtClipboardTransport final : public ClipboardTransport {
    Q_OBJECT
public:
    explicit QtClipboardTransport(const QDBusConnection &connection,
                                  QString serviceName = {}, QObject *parent = nullptr);
    ~QtClipboardTransport() override;
    void start() override;
    void stop() override;
    void fetchSnapshot(const QString &owner, quint64 token) override;
    void submitOperation(const QString &owner, quint64 token,
                         const OperationRequest &request) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
    void queryOwner();
    void setOwner(const QString &owner);
    void onOwnerChanged(const QString &service, const QString &oldOwner,
                        const QString &newOwner);

private Q_SLOTS:
    void onChanged(quint64 epoch, quint32 generation, quint64 revision);
};

} // namespace QindaQt::Services::Clipboard
