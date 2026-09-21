// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_client/voice_transport.h>

#include <QtCore/QList>

// A transport with no bus. Every reply is delivered by the test, so ordering,
// owner attribution and lost replies are all expressible.
class FakeVoiceTransport final : public QindaQt::Services::Voice::VoiceTransport {
    Q_OBJECT
public:
    using Snapshot = QindaQt::Services::Voice::Snapshot;
    using OperationRequest = QindaQt::Services::Voice::OperationRequest;
    using OperationResult = QindaQt::Services::Voice::OperationResult;

    struct Fetch {
        QString owner;
        quint64 token = 0;
    };
    struct Submission {
        QString owner;
        quint64 token = 0;
        OperationRequest request;
    };

    void start() override { started = true; }
    void stop() override { started = false; }

    void fetchSnapshot(const QString &owner, quint64 token) override
    {
        fetches.append(Fetch{owner, token});
    }

    void submitOperation(const QString &owner, quint64 token,
                         const OperationRequest &request) override
    {
        submissions.append(Submission{owner, token, request});
    }

    void becomeOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
    void invalidate(const QString &owner, quint64 revision)
    {
        Q_EMIT invalidated(owner, revision);
    }
    void reportLevel(const QString &owner, quint32 percent)
    {
        Q_EMIT levelReported(owner, percent);
    }
    void answerSnapshot(const Snapshot &snapshot, bool transportSuccess = true,
                        const QString &reasonCode = {})
    {
        const Fetch fetch = fetches.takeLast();
        Q_EMIT snapshotReply(fetch.owner, fetch.token, transportSuccess, snapshot,
                             reasonCode);
    }
    void answerOperation(const OperationResult &result, bool transportSuccess = true,
                         const QString &reasonCode = {})
    {
        const Submission submission = submissions.takeLast();
        Q_EMIT operationReply(submission.owner, submission.token, transportSuccess,
                              result, reasonCode);
    }

    bool started = false;
    QList<Fetch> fetches;
    QList<Submission> submissions;
};
