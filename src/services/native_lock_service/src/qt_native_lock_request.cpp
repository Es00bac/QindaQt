// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/native_lock_service/qt_native_lock_request.h"
#include "qindaqt/compositor_names/compositor_names.h"
#include "qindaqt/platform/compositor_attachment/compositor_attachment.h"
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QMetaType>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <utility>
namespace QindaQt::Services::NativeLock {
using Platform::Compositor::AttachmentIdentity;
class QtNativeLockRequest::Private {
public:
  Private(QtNativeLockRequest *object, QDBusConnection connection,
          Platform::Compositor::CompositorAttachment &binding)
      : q(object), bus(std::move(connection)), attachment(binding) {}
  bool same(const AttachmentIdentity &expectedIdentity) const {
    const auto current = attachment.identity();
    if (!attachment.sameBus(bus) || !current ||
        current->sessionOwner != expectedIdentity.sessionOwner ||
        current->compositorOwner != expectedIdentity.compositorOwner ||
        current->compositorPid != expectedIdentity.compositorPid ||
        current->socketBasename != expectedIdentity.socketBasename || !bus.interface())
      return false;
    // AGENT-GUARD: Recheck the shared daemon before admission and completion.
    auto query = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    query << QString(CompositorNames::service);
    const QDBusReply<QString> currentOwner = bus.call(query, QDBus::Block, 250);
    if (!currentOwner.isValid() || currentOwner.value() != expectedIdentity.compositorOwner)
      return false;
    query = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("GetConnectionUnixProcessID"));
    query << expectedIdentity.compositorOwner;
    const QDBusReply<uint> pid = bus.call(query, QDBus::Block, 250);
    const auto final = attachment.identity();
    return pid.isValid() && pid.value() == expectedIdentity.compositorPid && final &&
           final->sessionOwner == expectedIdentity.sessionOwner &&
           final->compositorOwner == expectedIdentity.compositorOwner &&
           final->compositorPid == expectedIdentity.compositorPid &&
           final->socketBasename == expectedIdentity.socketBasename;
  }
  void unsubscribe() {
    if (owner.isEmpty() || nonce.isEmpty())
      return;
    bus.disconnect(owner, QString(CompositorNames::nativeLockPath),
                   QString(CompositorNames::nativeLockInterface),
                   QStringLiteral("lockAdmissionReceipt"),
                   QStringList{nonce}, QStringLiteral("sb"), q,
                   SLOT(admissionReceipt(QString,bool,QDBusMessage)));
    owner.clear();
    nonce.clear();
  }
  void finish(RequestResult result) {
    if (!pending)
      return;
    pending = false;
    ++generation;
    unsubscribe();
    if (watcher) {
      watcher->disconnect(q);
      watcher->deleteLater();
      watcher = nullptr;
    }
    Q_EMIT q->completed(result);
  }
  void maybeFinish() {
    if (!pending || !replyReady || !receiptReady)
      return;
    if (!same(expected)) {
      finish(RequestResult::Uncertain);
      return;
    }
    finish(receiptAdmitted ? RequestResult::Admitted : RequestResult::Rejected);
  }
  void receive(const QString &value, bool admitted,
               const QDBusMessage &message) {
    if (!pending)
      return;
    const auto args = message.arguments();
    if (message.type() != QDBusMessage::SignalMessage ||
        message.service() != expected.compositorOwner ||
        message.path() != QString(CompositorNames::nativeLockPath) ||
        message.interface() != QString(CompositorNames::nativeLockInterface) ||
        message.member() != QStringLiteral("lockAdmissionReceipt") ||
        message.signature() != QStringLiteral("sb") || args.size() != 2 ||
        args.at(0).metaType() != QMetaType::fromType<QString>() ||
        args.at(1).metaType() != QMetaType::fromType<bool>() ||
        value != nonce || admitted != args.at(1).toBool() || receiptReady) {
      finish(RequestResult::Uncertain);
      return;
    }
    receiptReady = true;
    receiptAdmitted = admitted;
    maybeFinish();
  }
  QtNativeLockRequest *q;
  QDBusConnection bus;
  Platform::Compositor::CompositorAttachment &attachment;
  QDBusPendingCallWatcher *watcher = nullptr;
  AttachmentIdentity expected;
  QString owner, nonce;
  quint64 generation = 0;
  bool pending = false, replyReady = false, receiptReady = false;
  bool receiptAdmitted = false;
};
QtNativeLockRequest::QtNativeLockRequest(
    QDBusConnection bus, Platform::Compositor::CompositorAttachment &attachment,
    QObject *parent)
    : NativeLockRequest(parent),
      d(std::make_unique<Private>(this, std::move(bus), attachment)) {
  connect(&attachment, &Platform::Compositor::CompositorAttachment::revoked,
          this, &QtNativeLockRequest::cancel);
  connect(&attachment, &Platform::Compositor::CompositorAttachment::attached,
          this, &QtNativeLockRequest::cancel);
}
QtNativeLockRequest::~QtNativeLockRequest() = default;
void QtNativeLockRequest::cancel() { d->finish(RequestResult::Uncertain); }
bool QtNativeLockRequest::request() {
  if (d->pending || !d->bus.isConnected() ||
      thread() != d->attachment.thread() ||
      QThread::currentThread() != thread())
    return false;
  const auto identity = d->attachment.identity();
  if (!identity || !d->same(*identity))
    return false;
  d->expected = *identity;
  d->owner = identity->compositorOwner;
  d->nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
  d->nonce.remove(QLatin1Char('-'));
  d->nonce = d->nonce.toLower();
  d->replyReady = d->receiptReady = d->receiptAdmitted = false;
  if (!d->bus.connect(d->owner, QString(CompositorNames::nativeLockPath),
                      QString(CompositorNames::nativeLockInterface),
                      QStringLiteral("lockAdmissionReceipt"),
                      QStringList{d->nonce}, QStringLiteral("sb"), this,
                      SLOT(admissionReceipt(QString,bool,QDBusMessage)))) {
    d->owner.clear();
    d->nonce.clear();
    return false;
  }
  auto message = QDBusMessage::createMethodCall(
      d->owner, QString(CompositorNames::nativeLockPath),
      QString(CompositorNames::nativeLockInterface),
      QStringLiteral("RequestLockWithReceipt"));
  message << d->nonce;
  d->pending = true;
  const auto generation = ++d->generation;
  d->watcher = new QDBusPendingCallWatcher(d->bus.asyncCall(message, 1500), this);
  connect(d->watcher, &QDBusPendingCallWatcher::finished, this,
          [this, generation] {
            if (!d->pending || generation != d->generation || !d->watcher)
              return;
            const auto reply = d->watcher->reply();
            if (reply.type() != QDBusMessage::ReplyMessage ||
                !reply.signature().isEmpty() || !reply.arguments().isEmpty()) {
              d->finish(RequestResult::Uncertain);
              return;
            }
            d->replyReady = true;
            d->maybeFinish();
          });
  QTimer::singleShot(1500, this, [this, generation] {
    if (d->pending && generation == d->generation)
      d->finish(RequestResult::Uncertain);
  });
  return true;
}
void QtNativeLockRequest::admissionReceipt(const QString &nonce, bool admitted,
                                           const QDBusMessage &message) {
  d->receive(nonce, admitted, message);
}
} // namespace QindaQt::Services::NativeLock
