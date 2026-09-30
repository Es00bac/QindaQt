// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native_lock_request.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <memory>
namespace QindaQt::Platform::Compositor {
class CompositorAttachment;
}
namespace QindaQt::Services::NativeLock {
// Borrows an admitted ordinary attachment, which must outlive this same-thread
// object. Uses a copied connection to the same session bus. The native service
// itself grants only lock admission; no locker descriptor or unlock capability.
class QtNativeLockRequest final : public NativeLockRequest {
  Q_OBJECT
public:
  QtNativeLockRequest(QDBusConnection bus,
                      Platform::Compositor::CompositorAttachment &attachment,
                      QObject *parent = nullptr);
  ~QtNativeLockRequest() override;
  bool request() override;
  void cancel() override;

private Q_SLOTS:
  void admissionReceipt(const QString &nonce, bool admitted,
                       const QDBusMessage &message);

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::NativeLock
