// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QtCore/QObject>

namespace QindaQt::DisplayClient {

// Transport implementations bind every request and signal to one exact unique
// owner. They never decide publication, retries, or operation replay policy.
// Implementations and callers share one Qt thread; every completion is emitted
// asynchronously as a bounded value and late completion is permitted. Request
// ids are allocated by the Client and must be echoed unchanged. Activation is
// only a process-start request: ownerChanged() remains the sole authority for a
// usable owner baseline.
class DisplayTransport : public QObject {
  Q_OBJECT

public:
  explicit DisplayTransport(QObject *parent = nullptr) : QObject(parent) {}
  ~DisplayTransport() override = default;

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void requestActivation() = 0;
  virtual void fetchSnapshot(const QString &owner, quint64 requestId) = 0;
  virtual void submitStage(const QString &owner, quint64 requestId,
                           const QString &transactionId,
                           const Display::Candidate &candidate) = 0;
  virtual void submitPreview(const QString &owner, quint64 requestId,
                             const QString &transactionId) = 0;
  virtual void submitConfirm(const QString &owner, quint64 requestId,
                             const QString &transactionId) = 0;
  virtual void submitCancel(const QString &owner, quint64 requestId,
                            const QString &transactionId) = 0;

Q_SIGNALS:
  void ownerChanged(const QString &owner);
  void activationFinished(bool success, const QString &reasonCode);
  void invalidated(const QString &owner, const QString &epoch, quint64 revision,
                   bool available);
  void snapshotReply(const QString &owner, quint64 requestId,
                     bool transportSuccess,
                     const QindaQt::Display::Snapshot &snapshot,
                     const QString &reasonCode);
  void operationReply(const QString &owner, quint64 requestId,
                      bool transportSuccess,
                      const QindaQt::Display::OperationResult &result,
                      const QString &reasonCode);
};

} // namespace QindaQt::DisplayClient
