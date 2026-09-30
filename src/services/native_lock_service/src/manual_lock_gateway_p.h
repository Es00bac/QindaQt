// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/services/native_lock_service/native_lock_request.h"
#include <functional>
namespace QindaQt::Services::NativeLock {
// One retained manual call across every facade interface. No queue or replay.
class ManualLockGateway final : public QObject {
public:
  explicit ManualLockGateway(NativeLockRequest &port,
                             QObject *parent = nullptr);
  bool submit(std::function<void(RequestResult)> completion);
  void cancel();

private:
  void finish(RequestResult result);
  NativeLockRequest &m_port;
  std::function<void(RequestResult)> m_completion;
};
} // namespace QindaQt::Services::NativeLock
