// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/display_transport.h>

namespace QindaQt::DisplayClient::TestSupport {

class FakeDisplayTransport final : public DisplayTransport {
  Q_OBJECT

public:
  struct Fetch {
    QString owner;
    quint64 requestId = 0;
  };
  struct Operation {
    Display::OperationKind kind = Display::OperationKind::Stage;
    QString owner;
    quint64 requestId = 0;
    QString transactionId;
    Display::Candidate candidate;
  };

  void start() override { running = true; }
  void stop() override { running = false; }
  void requestActivation() override { ++activationRequests; }

  void fetchSnapshot(const QString &owner, quint64 requestId) override {
    fetches.push_back({owner, requestId});
  }

  void submitStage(const QString &owner, quint64 requestId,
                   const QString &transactionId,
                   const Display::Candidate &candidate) override {
    operations.push_back({Display::OperationKind::Stage, owner, requestId,
                          transactionId, candidate});
    maybeReplyInline(operations.constLast());
  }

  void submitPreview(const QString &owner, quint64 requestId,
                     const QString &transactionId) override {
    operations.push_back(
        {Display::OperationKind::Preview, owner, requestId, transactionId, {}});
    maybeReplyInline(operations.constLast());
  }

  void submitConfirm(const QString &owner, quint64 requestId,
                     const QString &transactionId) override {
    operations.push_back(
        {Display::OperationKind::Confirm, owner, requestId, transactionId, {}});
    maybeReplyInline(operations.constLast());
  }

  void submitCancel(const QString &owner, quint64 requestId,
                    const QString &transactionId) override {
    operations.push_back(
        {Display::OperationKind::Cancel, owner, requestId, transactionId, {}});
    maybeReplyInline(operations.constLast());
  }

  void publishOwner(const QString &owner) { Q_EMIT ownerChanged(owner); }
  void publishInvalidation(const QString &owner, const QString &epoch,
                           quint64 revision, bool available = true) {
    Q_EMIT invalidated(owner, epoch, revision, available);
  }
  void finishActivation(bool success, const QString &reason = {}) {
    Q_EMIT activationFinished(success, reason);
  }
  void replySnapshot(const Fetch &fetch, const Display::Snapshot &snapshot,
                     bool success = true, const QString &reason = {}) {
    Q_EMIT snapshotReply(fetch.owner, fetch.requestId, success, snapshot,
                         reason);
  }
  void replyOperation(const Operation &operation,
                      const Display::OperationResult &result,
                      bool success = true, const QString &reason = {}) {
    Q_EMIT operationReply(operation.owner, operation.requestId, success, result,
                          reason);
  }
  void replyOperationAs(const QString &owner, quint64 requestId,
                        const Display::OperationResult &result,
                        bool success = true, const QString &reason = {}) {
    Q_EMIT operationReply(owner, requestId, success, result, reason);
  }

  bool running = false;
  int activationRequests = 0;
  QList<Fetch> fetches;
  QList<Operation> operations;
  bool inlineOperationReply = false;
  Display::OperationResult inlineResult;

private:
  void maybeReplyInline(const Operation &operation) {
    if (inlineOperationReply) {
      Q_EMIT operationReply(operation.owner, operation.requestId, true,
                            inlineResult, {});
    }
  }
};

inline Display::OperationResult
operationResult(Display::OperationKind kind, Display::OperationStatus status,
                QString epoch, quint64 revision, QString transactionId,
                QString diagnostic = {},
                Display::ErrorCode error = Display::ErrorCode::None) {
  if ((status == Display::OperationStatus::Rejected ||
       status == Display::OperationStatus::Uncertain) &&
      error == Display::ErrorCode::None) {
    error = Display::ErrorCode::InvalidTransition;
  }
  return {.kind = kind,
          .status = status,
          .error = error,
          .initiatingEpoch = std::move(epoch),
          .initiatingRevision = revision,
          .observedRevision = revision,
          .transactionId = std::move(transactionId),
          .diagnostic = std::move(diagnostic)};
}

} // namespace QindaQt::DisplayClient::TestSupport
