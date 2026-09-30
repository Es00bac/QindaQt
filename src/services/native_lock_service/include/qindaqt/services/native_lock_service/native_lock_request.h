// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
namespace QindaQt::Services::NativeLock {
// Admission describes the compositor accepting a request, never physical
// protection or completed authentication. Unknown outcomes are not replayed.
enum class RequestResult { Admitted, Rejected, Uncertain };
class NativeLockRequest : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~NativeLockRequest() override = default;
  // Same-thread, at most one pending operation. False means nothing was sent.
  // A true result yields exactly one completed signal unless this object dies.
  virtual bool request() = 0;
  // Cancels a pending request with one Uncertain completion; later native
  // replies must be fenced before another request may be submitted.
  virtual void cancel() = 0;
Q_SIGNALS:
  void completed(RequestResult result);
};
} // namespace QindaQt::Services::NativeLock
Q_DECLARE_METATYPE(QindaQt::Services::NativeLock::RequestResult)
