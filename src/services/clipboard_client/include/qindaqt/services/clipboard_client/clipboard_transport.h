// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_protocol/clipboard_protocol.h>

#include <QtCore/QObject>

namespace QindaQt::Services::Clipboard {

class ClipboardTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ClipboardTransport() override = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetchSnapshot(const QString &owner, quint64 token) = 0;
    virtual void submitOperation(const QString &owner, quint64 token,
                                 const OperationRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 epoch, quint32 generation,
                     quint64 revision);
    void snapshotReply(const QString &owner, quint64 token, bool transportSuccess,
                       const QindaQt::Services::Clipboard::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 token, bool transportSuccess,
                        const QindaQt::Services::Clipboard::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Services::Clipboard
