// SPDX-License-Identifier: GPL-3.0-or-later
#include "manual_lock_gateway_p.h"
#include <utility>
namespace QindaQt::Services::NativeLock {
ManualLockGateway::ManualLockGateway(NativeLockRequest &port, QObject *parent)
    : QObject(parent), m_port(port) {
  connect(&port, &NativeLockRequest::completed, this,
          &ManualLockGateway::finish);
}
bool ManualLockGateway::submit(std::function<void(RequestResult)> completion) {
  if (m_completion)
    return false;
  m_completion = std::move(completion);
  if (!m_port.request())
    finish(RequestResult::Uncertain);
  return true;
}
void ManualLockGateway::finish(RequestResult result) {
  if (!m_completion)
    return;
  auto completion = std::exchange(m_completion, {});
  completion(result);
}
void ManualLockGateway::cancel() {
  if (m_completion) {
    m_port.cancel();
    finish(RequestResult::Uncertain);
  }
}
} // namespace QindaQt::Services::NativeLock
