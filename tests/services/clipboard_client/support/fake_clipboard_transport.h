// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_client/clipboard_transport.h>

class FakeClipboardTransport final : public QindaQt::Services::Clipboard::ClipboardTransport {
    Q_OBJECT
public:
    using ClipboardTransport::ClipboardTransport;
    void start() override { started = true; }
    void stop() override { started = false; }
    void fetchSnapshot(const QString &owner, quint64 token) override
    { lastFetchOwner = owner; lastFetchToken = token; }
    void submitOperation(const QString &owner, quint64 token,
                         const QindaQt::Services::Clipboard::OperationRequest &request) override
    { lastOperationOwner = owner; lastOperationToken = token; lastRequest = request; }
    void announceOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void replySnapshot(const QString &owner, quint64 token,
                       const QindaQt::Services::Clipboard::Snapshot &snapshot)
    { Q_EMIT snapshotReply(owner, token, true, snapshot, {}); }
    void replyOperation(const QString &owner, quint64 token,
                        const QindaQt::Services::Clipboard::OperationResult &result)
    { Q_EMIT operationReply(owner, token, true, result, {}); }

    QString lastFetchOwner;
    quint64 lastFetchToken = 0;
    QString lastOperationOwner;
    quint64 lastOperationToken = 0;
    QindaQt::Services::Clipboard::OperationRequest lastRequest;
    bool started = false;
};
